/***************************************************************************
 *   Copyright (C) 1998-2007 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of Lux Renderer.                                    *
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
 *   Lux Renderer website : http://www.luxrender.org                       *
 ***************************************************************************/

// uber.cpp*
#include "uber.h"

// UberMaterial Method Definitions
BSDF *UberMaterial::GetBSDF(MemoryArena &arena, const DifferentialGeometry &dgGeom, const DifferentialGeometry &dgShading) const {
	// Allocate _BSDF_, possibly doing bump-mapping with _bumpMap_
	DifferentialGeometry dgs;
	if (bumpMap)
		Bump(bumpMap, dgGeom, dgShading, &dgs);
	else
		dgs = dgShading;
	BSDF *bsdf = BSDF_ALLOC(arena, BSDF)(dgs, dgGeom.nn);

	Spectrum op = opacity->Evaluate(dgs).Clamp();
	if (op != Spectrum(1.)) {
	    BxDF *tr = BSDF_ALLOC(arena, SpecularTransmission)(-op + Spectrum(1.), 1., 1.);
	    bsdf->Add(tr);
	}

	Spectrum kd = op * Kd->Evaluate(dgs).Clamp();
	if (!kd.Black()) {
	    BxDF *diff = BSDF_ALLOC(arena, Lambertian)(kd);
	    bsdf->Add(diff);
	}

	Spectrum ks = op * Ks->Evaluate(dgs).Clamp();
	if (!ks.Black()) {
	    Fresnel *fresnel = BSDF_ALLOC(arena, FresnelDielectric)(1.5f, 1.f);
	    float rough = roughness->Evaluate(dgs);
	    BxDF *spec = BSDF_ALLOC(arena, Microfacet)(ks, fresnel, BSDF_ALLOC(arena, Blinn)(1.f / rough));
	    bsdf->Add(spec);
	}

	Spectrum kr = op * Kr->Evaluate(dgs).Clamp();
	if (!kr.Black()) {
		Fresnel *fresnel = BSDF_ALLOC(arena, FresnelDielectric)(1.5f, 1.f);
		bsdf->Add(BSDF_ALLOC(arena, SpecularReflection)(kr, fresnel));
	}

	return bsdf;
}
// UberMaterial Dynamic Creation Routine
Material* UberMaterial::CreateMaterial(const Transform &xform,
		const TextureParams &mp) {
	Reference<Texture<Spectrum> > Kd = mp.GetSpectrumTexture("Kd", Spectrum(1.f));
	Reference<Texture<Spectrum> > Ks = mp.GetSpectrumTexture("Ks", Spectrum(1.f));
	Reference<Texture<Spectrum> > Kr = mp.GetSpectrumTexture("Kr", Spectrum(0.f));
	Reference<Texture<float> > roughness = mp.GetFloatTexture("roughness", .1f);
	Reference<Texture<Spectrum> > opacity = mp.GetSpectrumTexture("opacity", 1.f);
	Reference<Texture<float> > bumpMap = mp.GetFloatTexture("bumpmap", 0.f);
	return new UberMaterial(Kd, Ks, Kr, roughness, opacity, bumpMap);
}
