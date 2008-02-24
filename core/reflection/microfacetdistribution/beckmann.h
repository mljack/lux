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

#ifndef LUX_BECKMANN_H
#define LUX_BECKMANN_H
// beckmann.h*
#include "lux.h"
#include "bxdf.h"
#include "geometry.h"
#include "microfacetdistribution.h"

namespace lux
{

class  Beckmann : public MicrofacetDistribution {
public:

  Beckmann (float rms);

  // Beckmann Public Methods
  float D (const Vector &wh) const;
  virtual void Sample_f (const Vector &wi, Vector *sampled_f, float u1, float u2, float *pdf) const;
  virtual float Pdf (const Vector &wi, const Vector &wo) const;

private:

  float r;
};

}//namespace lux

#endif // LUX_BECKMANN_H

