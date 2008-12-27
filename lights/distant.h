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

// distant.cpp*
#include "lux.h"
#include "light.h"
#include "shape.h"
#include "scene.h"

namespace lux
{

// DistantLight Declarations
class DistantLight : public Light {
public:
	// DistantLight Public Methods
	DistantLight(const Transform &light2world, const RGBColor &radiance, float gain, const Vector &dir);
	~DistantLight();
	bool IsDeltaLight() const { return true; }
	SWCSpectrum Power(const TsPack *tspack, const Scene *scene) const {
		Point worldCenter;
		float worldRadius;
		scene->WorldBound().BoundingSphere(&worldCenter,
		                                   &worldRadius);
		return SWCSpectrum(tspack, LSPD) * M_PI * worldRadius * worldRadius;
	}
	SWCSpectrum Sample_L(const TsPack *tspack, const Point &P, float u1, float u2, float u3,
		Vector *wo, float *pdf, VisibilityTester *visibility) const;
	SWCSpectrum Sample_L(const TsPack *tspack, const Scene *scene, float u1, float u2,
		float u3, float u4, Ray *ray, float *pdf) const;
	float Pdf(const Point &, const Vector &) const;
	
	static Light *CreateLight(const Transform &light2world,
		const ParamSet &paramSet, const TextureParams &tp);
private:
	// DistantLight Private Data
	Vector lightDir;
	SPD *LSPD;
};

}//namespace lux
