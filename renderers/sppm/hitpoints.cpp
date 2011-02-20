/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

/*
 * A scene, to be rendered with SPPM, must have:
 *
 * Renderer "sppm"
 * Sampler "random or lowdiscrepancy" "string pixelsampler" ["linear"] "integer pixelsamples" [1]
 * SurfaceIntegrator "sppm"
 *
 */

#include "renderers/sppmrenderer.h"
#include "integrators/sppm.h"
#include "camera.h"
#include "film.h"
#include "sampling.h"
#include "light.h"
#include "reflection/bxdf.h"

using namespace lux;

//------------------------------------------------------------------------------
// HitPoints methods
//------------------------------------------------------------------------------

HitPoints::HitPoints(SPPMRenderer *engine) {
	renderer = engine;
	pass = 0;

	// Get the count of hit points required
	int xstart, xend, ystart, yend;
    renderer->scene->camera->film->GetSampleExtent(&xstart, &xend, &ystart, &yend);
	filmWidth = xend - xstart;
	filmHeight = yend - ystart;

	hitPoints = new std::vector<HitPoint>(filmWidth * filmHeight);
	LOG(LUX_INFO, LUX_NOERROR) << "Hit points count: " << hitPoints->size();

	// Initialize hit points field
	for (u_int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		hp->photonCount = 0;
		hp->reflectedFlux = XYZColor();

		// hp->accumPhotonRadius2 is initialized in the Init() method
		hp->accumPhotonCount = 0;
		hp->accumReflectedFlux = XYZColor();

		hp->constantHitsCount = 0;
		hp->surfaceHitsCount = 0;
		hp->accumRadiance = XYZColor();
		hp->radiance = XYZColor();
	}
}

HitPoints::~HitPoints() {
	delete lookUpAccel;
	delete hitPoints;
}

void HitPoints::Init() {
	// Not using UpdateBBox() because hp->accumPhotonRadius2 is not yet set
	BBox hpBBox = BBox();
	for (u_int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		if (hp->type == SURFACE)
			hpBBox = Union(hpBBox, hp->position);
	}

	// Calculate initial radius
	Vector ssize = hpBBox.pMax - hpBBox.pMin;
	const float photonRadius = renderer->sppmi->photonStartRadiusScale * ((ssize.x + ssize.y + ssize.z) / 3.f) / ((filmWidth + filmHeight) / 2.f) * 2.f;
	const float photonRadius2 = photonRadius * photonRadius;

	// Expand the bounding box by used radius
	hpBBox.Expand(photonRadius);
	// Update hit points information
	bbox = hpBBox;
	maxPhotonRaidus2 = photonRadius2;

	// Initialize hit points field
	for (u_int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		hp->accumPhotonRadius2 = photonRadius2;
	}

	// Allocate hit points lookup accelerator
	switch (renderer->sppmi->lookupAccelType) {
		case HASH_GRID:
			lookUpAccel = new HashGrid(this);
			break;
		case KD_TREE:
			lookUpAccel = new KdTree(this);
			break;
		case HYBRID_HASH_GRID:
			lookUpAccel = new HybridHashGrid(this);
			break;
		default:
			assert (false);
	}
}

void HitPoints::AccumulateFlux(const unsigned long long photonTraced) {
	LOG(LUX_INFO, LUX_NOERROR) << "Accumulate photons flux";
	for (u_int i = 0; i < hitPoints->size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		switch (hp->type) {
			case CONSTANT_COLOR:
				hp->accumRadiance += hp->eyeThroughput;
				hp->constantHitsCount += 1;
				break;
			case SURFACE:
				if ((hp->accumPhotonCount > 0)) {
					const unsigned long long pcount = hp->photonCount + hp->accumPhotonCount;
					const double alpha = renderer->sppmi->photonAlpha;
					const float g = alpha * pcount / (hp->photonCount * alpha + hp->accumPhotonCount);
					hp->photonCount = pcount;
					hp->reflectedFlux = (hp->reflectedFlux + hp->accumReflectedFlux) * g;

					hp->accumPhotonRadius2 *= g;
					hp->accumPhotonCount = 0;
					hp->accumReflectedFlux = XYZColor();
				}

				hp->surfaceHitsCount += 1;
				break;
			default:
				assert (false);
		}

		const u_int hitCount = hp->constantHitsCount + hp->surfaceHitsCount;
		if (hitCount > 0) {
			const double k = 1.0 / (M_PI * hp->accumPhotonRadius2 * photonTraced);

			hp->radiance = (hp->accumRadiance + hp->surfaceHitsCount * hp->reflectedFlux * k) / hitCount;
		}
	}
}

void HitPoints::SetHitPoints(RandomGenerator *rng) {
	Scene *scene = renderer->scene;
	Sampler *sampler = scene->sampler;

	LOG(LUX_INFO, LUX_NOERROR) << "Building hit points";

	// Initialize the sample
	Sample sample(scene->surfaceIntegrator, scene->volumeIntegrator, *scene);
	sampler->InitSample(&sample);
	sample.contribBuffer = NULL;
	sample.camera = scene->camera->Clone();
	sample.realTime = 0.f;
	sample.rng = rng;

	for (u_int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		sampler->GetNextSample(&sample);

		// Save ray time value
		sample.realTime = sample.camera->GetTime(sample.time);
		// Sample camera transformation
		sample.camera->SampleMotion(sample.realTime);

		// Sample new SWC thread wavelengths
		sample.swl.Sample(sample.wavelengths);

		// Trace the eye path
		TraceEyePath(hp, sample);

		// Free BSDF memory from computing image sample value
		sample.arena.FreeAll();
	}
}

void HitPoints::TraceEyePath(HitPoint *hp, const Sample &sample) {
	Scene &scene = *renderer->scene;
	const u_int sampleOffset = renderer->sppmi->sampleOffset;
	const bool includeEnvironment = renderer->sppmi->includeEnvironment;
	const u_int maxDepth = renderer->sppmi->maxEyePathDepth;

	//--------------------------------------------------------------------------
	// Following code is, given or taken, a copy of path integrator Li() method
	//--------------------------------------------------------------------------

	// Declare common path integration variables
	const SpectrumWavelengths &sw(sample.swl);
	Ray ray;
	const float rayWeight = sample.camera->GenerateRay(scene, sample, &ray);

	const float nLights = scene.lights.size();
	SWCSpectrum pathThroughput(1.f);
	SWCSpectrum L(0.f);
	bool scattered = false;
	hp->eyeAlpha = 1.f;
	hp->eyeDistance = INFINITY;
	u_int vertexIndex = 0;
	const Volume *volume = NULL;

	for (u_int pathLength = 0; ; ++pathLength) {
		const float *data = scene.sampler->GetLazyValues(sample,
			sampleOffset, pathLength);
		// Find next vertex of path
		Intersection isect;
		BSDF *bsdf;
		float spdf;
		if (!scene.Intersect(sample, volume, scattered, ray, data[3], &isect,
			&bsdf, &spdf, NULL, &pathThroughput)) {
			pathThroughput /= spdf;
			// Dade - now I know ray.maxt and I can call volumeIntegrator
			SWCSpectrum Lv;
			scene.volumeIntegrator->Li(scene, ray, sample, &Lv, &hp->eyeAlpha);
			if (!Lv.Black())
				L = Lv;

			// Stop path sampling since no intersection was found
			// Possibly add horizon in render & reflections
			if (includeEnvironment || vertexIndex > 0) {
				BSDF *ibsdf;
				for (u_int i = 0; i < nLights; ++i) {
					SWCSpectrum Le(pathThroughput);
					if (scene.lights[i]->Le(scene, sample, ray, &ibsdf, NULL, NULL, &Le))
						L += Le;
				}
			}

			// Set alpha channel
			if (vertexIndex == 0)
				hp->eyeAlpha = 0.f;

			hp->type = CONSTANT_COLOR;
			hp->eyeThroughput = XYZColor(sw, L * rayWeight);
			return;
		}
		scattered = bsdf->dgShading.scattered;
		pathThroughput /= spdf;
		if (vertexIndex == 0)
			hp->eyeDistance = ray.maxt * ray.d.Length();

		SWCSpectrum Lv;
		scene.volumeIntegrator->Li(scene, ray, sample, &Lv, &hp->eyeAlpha);
		if (!Lv.Black()) {
			Lv *= pathThroughput;
			L += Lv;
		}

		// Possibly add emitted light at path vertex
		Vector wo(-ray.d);
		if (isect.arealight) {
			BSDF *ibsdf;
			SWCSpectrum Le(isect.Le(sample, ray, &ibsdf, NULL, NULL));
			if (!Le.Black()) {
				Le *= pathThroughput;
				L += Le;
			}
		}

		if (pathLength == maxDepth) {
			hp->type = CONSTANT_COLOR;
			hp->eyeThroughput = XYZColor(sw, L * rayWeight);
			return;
		}

		const Point &p = bsdf->dgShading.p;
		const Normal &n = bsdf->dgShading.nn;

		// Sample BSDF to get new path direction
		Vector wi;
		float pdf;
		BxDFType flags;
		SWCSpectrum f;
		if (!bsdf->SampleF(sw, wo, &wi, data[0], data[1], data[2], &f,
			&pdf, BSDF_ALL, &flags, NULL, true)) {
			hp->type = CONSTANT_COLOR;
			hp->eyeThroughput = XYZColor(sw, L  * rayWeight);
			return;
		}

		if ((flags == BxDFType(BSDF_REFLECTION | BSDF_DIFFUSE)) ||
				(flags == BxDFType(BSDF_TRANSMISSION | BSDF_DIFFUSE))) {
			// It is a valid hit point
			hp->type = SURFACE;
			hp->bsdf = bsdf;
			hp->eyeThroughput = XYZColor(sw, pathThroughput * rayWeight);
			hp->position = p;
			hp->wo = wo;
			hp->normal = n;
			return;
		}

		pathThroughput *= f;
		if (pathThroughput.Black()) {
			hp->type = CONSTANT_COLOR;
			hp->eyeThroughput = XYZColor(sw, L * rayWeight);
			return;
		}

		ray = Ray(p, wi);
		ray.time = sample.realTime;
		volume = bsdf->GetVolume(wi);
	}
}

void HitPoints::UpdatePointsInformation() {
	// Calculate hit points bounding box
	bbox = BBox();
	maxPhotonRaidus2 = 0.f;
	for (u_int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		if (hp->type == SURFACE) {
			bbox = Union(bbox, hp->position);
			maxPhotonRaidus2 = max<float>(maxPhotonRaidus2, hp->accumPhotonRadius2);
		}
	}
	LOG(LUX_INFO, LUX_NOERROR) << "Hit points bounding box: " << bbox;
}
