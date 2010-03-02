/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

// transport.cpp*
#include "transport.h"
#include "scene.h"
#include "bxdf.h"
#include "light.h"
#include "mc.h"
#include "volume.h"
#include "camera.h"
#include "material.h"

namespace lux
{

// Integrator Method Definitions
bool VolumeIntegrator::Intersect(const TsPack *tspack, const Scene *scene,
	const Volume *volume, const RayDifferential &ray, Intersection *isect,
	BSDF **bsdf, SWCSpectrum *L) const
{
	const bool hit = scene->Intersect(ray, isect);
	if (hit) {
		isect->dg.ComputeDifferentials(ray);
		DifferentialGeometry dgShading;
		isect->primitive->GetShadingGeometry(isect->WorldToObject.GetInverse(),
			isect->dg, &dgShading);
		isect->material->GetShadingGeometry(isect->dg, &dgShading);
		if (Dot(ray.d, dgShading.nn) > 0.f) {
			if (!volume)
				volume = isect->interior;
			else if (!isect->interior)
				isect->interior = volume;
		} else {
			if (!volume)
				volume = isect->exterior;
			else if (!isect->exterior)
				isect->exterior = volume;
		}
		if (bsdf)
			*bsdf = isect->material->GetBSDF(tspack, isect->dg,
				dgShading, isect->exterior, isect->interior);
	}
	if (volume && L)
		*L *= Exp(-volume->Tau(tspack, ray));
	return hit;
}

bool VolumeIntegrator::Connect(const TsPack *tspack, const Scene *scene,
	const Volume *volume, const Point &p0, const Point &p1, bool clip,
	SWCSpectrum *f, float *pdf, float *pdfR) const
{
	const Vector w = p1 - p0;
	const float length = w.Length();
	const float shadowRayEpsilon = max(MachineEpsilon::E(p0),
		MachineEpsilon::E(length));
	if (shadowRayEpsilon >= length * .5f)
		return false;
	const float maxt = length - shadowRayEpsilon;
	RayDifferential ray(Ray(p0, w / length, shadowRayEpsilon, maxt));
	ray.time = tspack->time;
	if (clip)
		tspack->camera->ClampRay(ray);
	const Vector d(ray.d);
	Intersection isect;
	const BxDFType flags(BxDFType(BSDF_SPECULAR | BSDF_TRANSMISSION));
	// The for loop prevents an infinite sequence when the ray is almost
	// parallel to the surface and is self shadowed
	// This should be much less frequent with dynamic epsilon,
	// but it's safer to keep it
	for (u_int i = 0; i < 10000; ++i) {
		BSDF *bsdf;
		if (!scene->Intersect(tspack, volume, ray, &isect, &bsdf, f))
			return true;

		*f *= bsdf->f(tspack, d, -d, flags);
		if (f->Black())
			return false;
		const float cost = Dot(bsdf->nn, d);
		if (cost > 0.f)
			volume = isect.exterior;
		else
			volume = isect.interior;
		*f *= fabsf(cost);
		if (pdf)
			*pdf *= bsdf->Pdf(tspack, d, -d);
		if (pdfR)
			*pdfR *= bsdf->Pdf(tspack, -d, d);

		ray.mint = ray.maxt + MachineEpsilon::E(ray.maxt);
		ray.maxt = maxt;
	}
	return false;
}

// Integrator Utility Functions
SWCSpectrum UniformSampleAllLights(const TsPack *tspack, const Scene *scene,
	const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
	const Sample *sample,
	const float *lightSample, const float *lightNum,
	const float *bsdfSample, const float *bsdfComponent)
{
	SWCSpectrum L(0.f);
	for (u_int i = 0; i < scene->lights.size(); ++i) {
		L += EstimateDirect(tspack, scene, scene->lights[i], p, n, wo, bsdf,
			sample, lightSample[0], lightSample[1], *lightNum,
			bsdfSample[0], bsdfSample[1], *bsdfComponent);
	}
	return L;
}

u_int UniformSampleOneLight(const TsPack *tspack, const Scene *scene,
	const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
	const Sample *sample,
	const float *lightSample, const float *lightNum,
	const float *bsdfSample, const float *bsdfComponent, SWCSpectrum *L)
{
	// Randomly choose a single light to sample, _light_
	u_int nLights = scene->lights.size();
	if (nLights == 0) {
		*L = 0.f;
		return 0;
	}
	float ls3 = *lightNum * nLights;
	const u_int lightNumber = min(Floor2UInt(ls3), nLights - 1);
	ls3 -= lightNumber;
	Light *light = scene->lights[lightNumber];
	*L = static_cast<float>(nLights) * EstimateDirect(tspack, scene, light,
		p, n, wo, bsdf, sample, lightSample[0], lightSample[1], ls3,
		bsdfSample[0], bsdfSample[1], *bsdfComponent);
	return scene->lights[lightNumber]->group;
}

SWCSpectrum EstimateDirect(const TsPack *tspack, const Scene *scene, const Light *light,
	const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf, const Sample *sample, 
	float ls1, float ls2, float ls3, float bs1, float bs2, float bcs)
{
	SWCSpectrum Ld(0.f);

	// Dade - use MIS only if it is worth doing
	BxDFType noDiffuse = BxDFType(BSDF_ALL & ~(BSDF_DIFFUSE));
	if (light->IsDeltaLight() || (bsdf->NumComponents(noDiffuse) == 0)) {

		// Dade - trace only a single shadow ray
		Vector wi;
		float lightPdf;
		VisibilityTester visibility;
		SWCSpectrum Li = light->Sample_L(tspack, p, n,
			ls1, ls2, ls3, &wi, &lightPdf, &visibility);
		if (lightPdf > 0.f && !Li.Black()) {
			SWCSpectrum f = bsdf->f(tspack, wi, wo);
			SWCSpectrum fO(1.f);
			visibility.volume = bsdf->GetVolume(wi);
			if (!f.Black() && visibility.TestOcclusion(tspack, scene, &fO)) {
				// Add light's contribution to reflected radiance
				visibility.Transmittance(tspack, scene, sample, &Li);
				Li *= fO;
				Ld += f * Li * (AbsDot(wi, n) / lightPdf);
			}
		}
	} else {
		// Dade - trace 2 shadow rays and use MIS
		// Sample light source with multiple importance sampling
		const BxDFType noSpecular = BxDFType(BSDF_ALL & ~BSDF_SPECULAR);
		Vector wi;
		float lightPdf, bsdfPdf;
		VisibilityTester visibility;
		SWCSpectrum Li = light->Sample_L(tspack, p, n,
			ls1, ls2, ls3, &wi, &lightPdf, &visibility);
		if (lightPdf > 0.f && !Li.Black()) {
			SWCSpectrum f = bsdf->f(tspack, wi, wo, noSpecular);
			SWCSpectrum fO(1.f);
			visibility.volume = bsdf->GetVolume(wi);
			if (!f.Black() && visibility.TestOcclusion(tspack, scene, &fO)) {
				// Add light's contribution to reflected radiance
				visibility.Transmittance(tspack, scene, sample, &Li);
				Li *= fO;

				bsdfPdf = bsdf->Pdf(tspack, wi, wo, noSpecular);
				float weight = PowerHeuristic(1, lightPdf, 1, bsdfPdf);
				Ld += f * Li * (AbsDot(wi, n) * weight / lightPdf);
			}

			// Sample BSDF with multiple importance sampling
			SWCSpectrum fBSDF;
			if (bsdf->Sample_f(tspack, wo, &wi,	bs1, bs2, bcs, &fBSDF, &bsdfPdf, noSpecular, NULL, NULL, true)) {
				lightPdf = light->Pdf(tspack, p, n, wi);
				if (lightPdf > 0.f) {
					// Add light contribution from BSDF sampling
					float weight = PowerHeuristic(1, bsdfPdf, 1, lightPdf);
					Intersection lightIsect;
					Li = SWCSpectrum(1.f);
					RayDifferential ray(p, wi);
					ray.time = tspack->time;
					const Volume *volume = bsdf->GetVolume(wi);
					BSDF *ibsdf;
					const BxDFType flags(BxDFType(BSDF_SPECULAR | BSDF_TRANSMISSION));
					// The for loop prevents an infinite
					// loop when the ray is almost parallel
					// to the surface
					// It should much less frequent with
					// dynamic epsilon, but it's safer
					for (u_int i = 0; i < 10000; ++i) {
						if (!scene->Intersect(tspack, volume, ray, &lightIsect, &ibsdf, &Li)) {
							Li *= light->Le(tspack, ray);
							break;
						} else if (lightIsect.arealight == light) {
							Li *= lightIsect.Le(tspack, -wi);
							break;
						}

						Li *= ibsdf->f(tspack, wi, -wi, flags);
						if (Li.Black())
							break;
						Li *= AbsDot(ibsdf->dgShading.nn, wi);

						ray.mint = ray.maxt + MachineEpsilon::E(ray.maxt);
						ray.maxt = INFINITY;
						volume = ibsdf->GetVolume(wi);
					}
					if (!Li.Black()) {
						scene->Transmittance(tspack, ray, sample, &Li);
						Ld += fBSDF * Li * (AbsDot(wi, n) * weight / bsdfPdf);
					}
				}
			}
		}
	}

	return Ld;
}

}//namespace lux
