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
 
// lowdiscrepancypx.cpp*
#include "sampling.h"
#include "paramset.h"
#include "film.h"

namespace lux
{

// LowdiscrepancyPixelSampler Declarations
class LowdiscrepancyPixelSampler : public PixelSampler {
public:
	// LowdiscrepancyPixelSampler Public Methods
	LowdiscrepancyPixelSampler(int xstart, int xend,
	          int ystart, int yend);
	~LowdiscrepancyPixelSampler() {
	}

	u_int GetTotalPixels();
	bool GetNextPixel(int &xPos, int &yPos, u_int *use_pos);

private:
	u_int TotalPx;
    // Dade - number of pixel ralready returned by GetNextPixel()
    u_int pixelCounter;

	// LowdiscrepancyPixelSampler Private Data
	int xPixelStart, yPixelStart, xPixelEnd, yPixelEnd;
	u_int xSeed, ySeed;
};

}//namespace lux
