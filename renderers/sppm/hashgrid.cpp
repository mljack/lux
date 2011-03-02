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

#include "hitpoints.h"
#include "lookupaccel.h"
#include "bxdf.h"

using namespace lux;

HashGrid::HashGrid(HitPoints *hps) {
	hitPoints = hps;
	grid = NULL;

	RefreshMutex();
}

HashGrid::~HashGrid() {
	for (unsigned int i = 0; i < gridSize; ++i)
		delete grid[i];
	delete[] grid;
}

void HashGrid::RefreshMutex() {
	const unsigned int hitPointsCount = hitPoints->GetSize();
	const BBox &hpBBox = hitPoints->GetBBox();

	// Calculate the size of the grid cell
	const float maxPhotonRadius2 = hitPoints->GetMaxPhotonRaidus2();
	const float cellSize = sqrtf(maxPhotonRadius2) * 2.f;
	LOG(LUX_INFO, LUX_NOERROR) << "Hash grid cell size: " << cellSize;
	invCellSize = 1.f / cellSize;

	// TODO: add a tunable parameter for hashgrid size
	gridSize = hitPointsCount;
	if (!grid) {
		grid = new std::list<HitPoint *>*[gridSize];

		for (unsigned int i = 0; i < gridSize; ++i)
			grid[i] = NULL;
	} else {
		for (unsigned int i = 0; i < gridSize; ++i) {
			delete grid[i];
			grid[i] = NULL;
		}
	}

	LOG(LUX_INFO, LUX_NOERROR) << "Building hit points hash grid.";
	//unsigned int maxPathCount = 0;
	unsigned long long entryCount = 0;
	for (unsigned int i = 0; i < hitPointsCount; ++i) {
		HitPoint *hp = hitPoints->GetHitPoint(i);

		if (hp->type == SURFACE) {
			const float photonRadius = sqrtf(hp->accumPhotonRadius2);
			const Vector rad(photonRadius, photonRadius, photonRadius);
			const Vector bMin = ((hp->position - rad) - hpBBox.pMin) * invCellSize;
			const Vector bMax = ((hp->position + rad) - hpBBox.pMin) * invCellSize;

			for (int iz = abs(int(bMin.z)); iz <= abs(int(bMax.z)); iz++) {
				for (int iy = abs(int(bMin.y)); iy <= abs(int(bMax.y)); iy++) {
					for (int ix = abs(int(bMin.x)); ix <= abs(int(bMax.x)); ix++) {
						int hv = Hash(ix, iy, iz);

						if (grid[hv] == NULL)
							grid[hv] = new std::list<HitPoint *>();

						grid[hv]->push_front(hp);
						++entryCount;

						/*// grid[hv]->size() is very slow to execute
						if (grid[hv]->size() > maxPathCount)
							maxPathCount = grid[hv]->size();*/
					}
				}
			}
		}
	}
	//std::cerr << "Max. hit points in a single hash grid entry: " << maxPathCount << std::endl;
	LOG(LUX_INFO, LUX_NOERROR) << "Total hash grid entry: " << entryCount;
	LOG(LUX_INFO, LUX_NOERROR) << "Avg. hit points in a single hash grid entry: " << entryCount / gridSize;

	// HashGrid debug code
	/*for (unsigned int i = 0; i < hashGridSize; ++i) {
		if (grid[i]) {
			if (grid[i]->size() > 10) {
				std::cerr << "HashGrid[" << i << "].size() = " <<grid[i]->size() << std::endl;
			}
		}
	}*/
}

void HashGrid::AddFlux(const Point &hitPoint, const Vector &wi,
		const SpectrumWavelengths &sw, const SWCSpectrum &photonFlux, const u_int light_group) {
	// Look for eye path hit points near the current hit point
	Vector hh = (hitPoint - hitPoints->GetBBox().pMin) * invCellSize;
	const int ix = abs(int(hh.x));
	const int iy = abs(int(hh.y));
	const int iz = abs(int(hh.z));

	std::list<HitPoint *> *hps = grid[Hash(ix, iy, iz)];
	if (hps) {
		std::list<HitPoint *>::iterator iter = hps->begin();
		while (iter != hps->end()) {
			HitPoint *hp = *iter++;

			AddFluxToHitPoint(hp, hitPoint, wi, sw, photonFlux, light_group);
		}
	}
}
