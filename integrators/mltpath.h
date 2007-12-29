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

// mltpath.cpp*
#include "lux.h"
#include "transport.h"
#include "metropolis.h"
#include "scene.h"

namespace lux
{

// MLTPathIntegrator Declarations
class MLTPathIntegrator : public SurfaceIntegrator {
public:
	// MLTPathIntegrator Public Methods
	Spectrum Li(MemoryArena &arena, const Scene *scene, const RayDifferential &ray, const Sample *sample, float *alpha) const;
	MLTPathIntegrator(int md, float cp, int maxreject, float plarge) { 
			maxDepth = md; continueProbability = cp; 
			maxReject = maxreject; pLarge = plarge; }
	Spectrum Next(MemoryArena &arena, RayDifferential ray, const Scene *scene, int pathLength) const;
	virtual MLTPathIntegrator* clone() const; // Lux (copy) constructor for multithreading
	IntegrationSampler* HasIntegrationSampler(IntegrationSampler *isa);
	static SurfaceIntegrator *CreateSurfaceIntegrator(const ParamSet &params);
private:
	// MLTPathIntegrator Private Data
	int maxDepth;
	float continueProbability;
	int maxReject;
	float pLarge;
	IntegrationSampler *mltIntegrationSampler;
};

}//namespace lux

