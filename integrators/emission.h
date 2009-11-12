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
#include "volume.h"
#include "transport.h"
#include "scene.h"

namespace lux
{

// EmissionIntegrator Declarations
class EmissionIntegrator : public VolumeIntegrator {
public:
	// EmissionIntegrator Public Methods
	EmissionIntegrator(float ss, u_int g) : group(g) { stepSize = ss; }
	virtual ~EmissionIntegrator() { }
	virtual void RequestSamples(Sample *sample, const Scene *scene);
	virtual void Transmittance(const TsPack *tspack, const Scene *, const Ray &ray,
		const Sample *sample, float *alpha, SWCSpectrum *const L) const;
	virtual u_int Li(const TsPack *tspack, const Scene *, const RayDifferential &ray, const Sample *sample, SWCSpectrum *L, float *alpha) const;
	static VolumeIntegrator *CreateVolumeIntegrator(const ParamSet &params);
private:
	// EmissionIntegrator Private Data
	float stepSize;
	const u_int group;
	u_int tauSampleOffset, scatterSampleOffset;
};

}//namespace lux

