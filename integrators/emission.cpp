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

// emission.cpp*
#include "emission.h"
#include "paramset.h"
#include "dynload.h"
#include "context.h"

using namespace lux;

// EmissionIntegrator Method Definitions
void EmissionIntegrator::RequestSamples(Sample *sample,
		const Scene *scene) {
	tauSampleOffset = sample->Add1D(1);
	scatterSampleOffset = sample->Add1D(1);
}

void EmissionIntegrator::Transmittance(const TsPack *tspack, const Scene *scene,
		const Ray &ray, const Sample *sample, float *alpha, SWCSpectrum *const L) const {
	if (!scene->volumeRegion) 
		return;
	//float step = sample ? stepSize : 4.f * stepSize;
	float step = stepSize;
	float offset = sample->oneD[tauSampleOffset][0];
	SWCSpectrum tau =
		SWCSpectrum(tspack, scene->volumeRegion->Tau(ray, step, offset));
	*L *= Exp(-tau);
}
u_int EmissionIntegrator::Li(const TsPack *tspack, const Scene *scene,
		const RayDifferential &ray, const Sample *sample,
		SWCSpectrum *Lv, float *alpha) const {
	VolumeRegion *vr = scene->volumeRegion;
	float t0, t1;
	if (!vr || !vr->IntersectP(ray, &t0, &t1)) return 0.f;
	// Do emission-only volume integration in _vr_
	*Lv = 0.f;
	// Prepare for volume integration stepping
	u_int N = Ceil2Int((t1-t0) / stepSize);
	float step = (t1 - t0) / N;
	SWCSpectrum Tr(1.f);
	Point p = ray(t0), pPrev;
	Vector w = -ray.d;
	t0 += sample->oneD[scatterSampleOffset][0] * step;
	for (u_int i = 0; i < N; ++i, t0 += step) {
		// Advance to sample at _t0_ and update _T_
		pPrev = p;
		p = ray(t0);
		SWCSpectrum stepTau = SWCSpectrum(tspack, vr->Tau(Ray(pPrev, p - pPrev, 0, 1),
			.5f * stepSize, tspack->rng->floatValue()));	// TODO - REFACT - remove and add random value from sample
		Tr *= Exp(-stepTau);
		// Possibly terminate raymarching if transmittance is small
		if (Tr.Filter(tspack) < 1e-3f) {
			const float continueProb = .5f;
			if (tspack->rng->floatValue() > continueProb) break; // TODO - REFACT - remove and add random value from sample
			Tr /= continueProb;
		}
		// Compute emission-only source term at _p_
		*Lv += Tr * SWCSpectrum(tspack, vr->Lve(p, w));
	}
	*Lv *= step;
	return group;
}
VolumeIntegrator* EmissionIntegrator::CreateVolumeIntegrator(const ParamSet &params) {
	float stepSize  = params.FindOneFloat("stepsize", 1.f);
	return new EmissionIntegrator(stepSize, Context::getActiveLightGroup());
}

static DynamicLoader::RegisterVolumeIntegrator<EmissionIntegrator> r("emission");
