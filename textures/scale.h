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


// scale.cpp*
#include "lux.h"
#include "spectrum.h"
#include "texture.h"
#include "color.h"
#include "paramset.h"

namespace lux
{

// ScaleTexture Declarations
template <class T1, class T2>
class ScaleTexture : public Texture<T2> {
public:
	// ScaleTexture Public Methods
	ScaleTexture(boost::shared_ptr<Texture<T1> > &t1,
		boost::shared_ptr<Texture<T2> > &t2) : tex1(t1), tex2(t2) { }
	virtual ~ScaleTexture() { }
	virtual T2 Evaluate(const TsPack *tspack, const DifferentialGeometry &dg) const {
		return tex1->Evaluate(tspack, dg) * tex2->Evaluate(tspack, dg);
	}
	// In Y() one of the textures must use Filter to avoid double W->lm conv
	virtual float Y() const { return tex1->Filter() * tex2->Y(); }
	virtual float Filter() const { return tex1->Filter() * tex2->Filter(); }
	virtual void SetIlluminant() {
		// Update sub-textures
		tex1->SetIlluminant();
		tex2->SetIlluminant();
	}
	static Texture<float> * CreateFloatTexture(const Transform &tex2world, const ParamSet &tp);
	static Texture<SWCSpectrum> * CreateSWCSpectrumTexture(const Transform &tex2world, const ParamSet &tp);
private:
	boost::shared_ptr<Texture<T1> > tex1;
	boost::shared_ptr<Texture<T2> > tex2;
};

// ScaleTexture Method Definitions
template <class T, class U> inline Texture<float> * ScaleTexture<T,U>::CreateFloatTexture(const Transform &tex2world,
	const ParamSet &tp)
{
	boost::shared_ptr<Texture<float> > tex1(tp.GetFloatTexture("tex1", 1.f)),
		tex2(tp.GetFloatTexture("tex2", 1.f));
	return new ScaleTexture<float, float>(tex1,tex2);
}

template <class T,class U> inline Texture<SWCSpectrum> * ScaleTexture<T,U>::CreateSWCSpectrumTexture(const Transform &tex2world,
	const ParamSet &tp)
{
	boost::shared_ptr<Texture<SWCSpectrum> > tex2(tp.GetSWCSpectrumTexture("tex2", RGBColor(1.f)));
	boost::shared_ptr<Texture<float> > ftex1(tp.GetFloatTexture("tex1"));
	if (ftex1)
		return new ScaleTexture<float, SWCSpectrum>(ftex1, tex2);
	else {
		boost::shared_ptr<Texture<SWCSpectrum> > tex1(tp.GetSWCSpectrumTexture("tex1", RGBColor(1.f)));
		return new ScaleTexture<SWCSpectrum, SWCSpectrum>(tex1, tex2);
	}
}

}//namespace lux

