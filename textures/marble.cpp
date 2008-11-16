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

// marble.cpp*
#include "marble.h"
#include "dynload.h"

using namespace lux;

// MarbleTexture Method Definitions
Texture<float> * MarbleTexture::CreateFloatTexture(const Transform &tex2world,
		const TextureParams &tp) {
	return NULL;
}

Texture<SWCSpectrum> * MarbleTexture::CreateSWCSpectrumTexture(const Transform &tex2world,
		const TextureParams &tp) {
	// Initialize 3D texture mapping _map_ from _tp_
	TextureMapping3D *map = new IdentityMapping3D(tex2world);
	// Apply texture specified transformation option for 3D mapping
	IdentityMapping3D *imap = (IdentityMapping3D*) map;
	imap->Apply3DTextureMappingOptions(tp);
	return new MarbleTexture(tp.FindInt("octaves", 8),
		tp.FindFloat("roughness", .5f),
		tp.FindFloat("scale", 1.f),
		tp.FindFloat("variation", .2f),
		map);
}

static DynamicLoader::RegisterFloatTexture<MarbleTexture> r1("marble");
static DynamicLoader::RegisterSWCSpectrumTexture<MarbleTexture> r2("marble");
