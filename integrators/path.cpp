/***************************************************************************
 *   Copyright (C) 1998-2008 by authors (see AUTHORS.txt )                 *
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

// path.cpp*
#include "path.h"
#include "bxdf.h"
#include "light.h"
#include "paramset.h"

using namespace lux;

// PathIntegrator Method Definitions
void PathIntegrator::RequestSamples(Sample *sample, const Scene *scene)
{
	vector<u_int> structure;
	structure.push_back(2);	//light position
	structure.push_back(1);	//light number
	structure.push_back(2);	//bsdf sampling for light
	structure.push_back(1);	//bsdf component for light
	structure.push_back(1);	//continue
	structure.push_back(2);	//bsdf sampling for path
	structure.push_back(1);	//bsdf component for path
	sampleOffset = sample->AddxD(structure, maxDepth + 1);
}

SWCSpectrum PathIntegrator::Li(const Scene *scene,
		const RayDifferential &r, const Sample *sample,
		float *alpha) const
{
	SampleGuard guard(sample->sampler, sample);
	// Declare common path integration variables
	SWCSpectrum pathThroughput = 1., L = 0.;
	float V = .1f;
	RayDifferential ray(r);
	bool specularBounce = true;
	if (alpha) *alpha = 1.;
	for (int pathLength = 0; ; ++pathLength) {
		// Find next vertex of path
		Intersection isect;
		if (!scene->Intersect(ray, &isect)) {
			// Stop path sampling since no intersection was found
			// Possibly add emitted light
			// NOTE - Added by radiance - adds horizon in render & reflections
			if (specularBounce) {
				SWCSpectrum Le(0.f);
				for (u_int i = 0; i < scene->lights.size(); ++i)
					Le += scene->lights[i]->Le(ray);
				Le *= pathThroughput;
				L += Le;
			}
			// Set alpha channel
			if (pathLength == 0 && alpha && L.Black())
				*alpha = 0.;
			break;
		}
		if (pathLength == 0) {
			r.maxt = ray.maxt;
		}
		else
			pathThroughput *= scene->Transmittance(ray);
		// Possibly add emitted light at path vertex
		if (specularBounce)
			L += pathThroughput * isect.Le(-ray.d);
		if (pathLength == maxDepth)
			break;
		// Evaluate BSDF at hit point
		BSDF *bsdf = isect.GetBSDF(ray);
		// Sample illumination from lights to find path contribution
		const Point &p = bsdf->dgShading.p;
		const Normal &n = bsdf->dgShading.nn;
		Vector wo = -ray.d;
		float *data = sample->sampler->GetLazyValues(const_cast<Sample *>(sample), sampleOffset, pathLength);
		L += pathThroughput *
			UniformSampleOneLight(scene, p, n,
				wo, bsdf, sample,
				data, data + 2, data + 3, data + 5);

		// Sample BSDF to get new path direction
		Vector wi;
		float pdf;
		BxDFType flags;
		SWCSpectrum f = bsdf->Sample_f(wo, &wi, data[7], data[8], data[9],
			&pdf, BSDF_ALL, &flags);
		if (pdf == .0f || f.Black())
			break;

		const float dp = AbsDot(wi, n)/pdf;

		// Possibly terminate the path - note - radiance - added effiency optimized RR
		if (pathLength > 3) {
			float q = min(1., f.y()*dp);
			if (q < data[6])
				break;
			pathThroughput /= q;
		}

		specularBounce = (flags & BSDF_SPECULAR) != 0;
		pathThroughput *= f;
		pathThroughput *= dp;
		V += dp;

		ray = RayDifferential(p, wi);
	}
	L *= scene->volumeIntegrator->Transmittance(scene, ray, sample, alpha);
	L += scene->volumeIntegrator->Li(scene, ray, sample, alpha);
	XYZColor color(L.ToXYZ());
	if (color.y() > 0.f)
		sample->AddContribution(sample->imageX, sample->imageY,
			color, alpha ? *alpha : 1.f, V);
	return L;
}
SurfaceIntegrator* PathIntegrator::CreateSurfaceIntegrator(const ParamSet &params)
{
	// general
	int maxDepth = params.FindOneInt("maxdepth", 16);
	return new PathIntegrator(maxDepth);
}

