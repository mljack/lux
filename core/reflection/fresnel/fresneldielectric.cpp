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

// fresneldielectric.cpp*
#include "fresneldielectric.h"
#include "color.h"
#include "spectrum.h"
#include "spectrumwavelengths.h"
#include "mc.h"
#include "sampling.h"
#include <stdarg.h>

#include <boost/thread/tss.hpp>

using namespace lux;

extern boost::thread_specific_ptr<SpectrumWavelengths> thread_wavelengths;

SWCSpectrum FresnelDielectric::Evaluate(float cosi) const {
	// Compute Fresnel reflectance for dielectric
	cosi = Clamp(cosi, -1.f, 1.f);
	// Compute indices of refraction for dielectric
	bool entering = cosi > 0.;
	float ei = eta_i, et = eta_t;

	if(cb != 0.) {
		// Handle dispersion using cauchy formula
		float w = thread_wavelengths->SampleSingle();
		et = eta_t + (cb * 1000000) / (w*w);
	}

	if (!entering)
		swap(ei, et);
	// Compute _sint_ using Snell's law
	float sint = ei/et * sqrtf(max(0.f, 1.f - cosi*cosi));
	if (sint > 1.) {
		// Handle total internal reflection
		return 1.;
	}
	else {
		float cost = sqrtf(max(0.f, 1.f - sint*sint));
		return FrDiel(fabsf(cosi), cost, ei, et);
	}
}

