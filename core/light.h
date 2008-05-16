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

#ifndef LUX_LIGHT_H
#define LUX_LIGHT_H
// light.h*
#include "lux.h"
#include "geometry.h"
#include "spectrum.h"
#include "error.h"
#include "rgbillum.h"
// Light Declarations

namespace lux
{

class  Light {
public:
	// Light Interface
	virtual ~Light();
	Light(const Transform &l2w, int ns = 1)
		: nSamples(max(1, ns)), LightToWorld(l2w),
		  WorldToLight(l2w.GetInverse()) {
		if (WorldToLight.HasScale())
			luxError(LUX_UNIMPLEMENT,LUX_WARNING,"Scaling detected in world-to-light transformation!\nThe system has numerous assumptions, implicit and explicit,\nthat this transform will have no scale factors in it.\nProceed at your own risk; your image may have errors or\nthe system may crash as a result of this.");
		havePortalShape = false;
		nrPortalShapes = 0;
		PortalArea = 0;
	}
	virtual SWCSpectrum Sample_L(const Point &p,
		Vector *wi, VisibilityTester *vis) const = 0;
	virtual SWCSpectrum Power(const Scene *) const = 0;
	virtual bool IsDeltaLight() const = 0;
	virtual SWCSpectrum Le(const RayDifferential &r) const;
	virtual SWCSpectrum Sample_L(const Point &p, float u1,
		float u2, float u3, Vector *wi, float *pdf,
		VisibilityTester *vis) const = 0;
	virtual float Pdf(const Point &p,
	                  const Vector &wi) const = 0;
	virtual SWCSpectrum Sample_L(const Point &p, const Normal &n,
			float u1, float u2, float u3, Vector *wi, float *pdf,
			VisibilityTester *visibility) const {
		return Sample_L(p, u1, u2, u3, wi, pdf, visibility);
	}
	virtual float Pdf(const Point &p, const Normal &n,
			const Vector &wi) const {
		return Pdf(p, wi);
	}
	virtual SWCSpectrum Sample_L(const Scene *scene, float u1,
		float u2, float u3, float u4,
		Ray *ray, float *pdf) const = 0;
	virtual SWCSpectrum Sample_L(const Scene *scene, float u1, float u2, BSDF **bsdf, float *pdf) const {luxError(LUX_BUG, LUX_SEVERE, "Unimplemented Light::Sample_L"); return 0.f;}
	virtual SWCSpectrum Sample_L(const Scene *scene, const Point &p, const Normal &n, float u1, float u2, float u3, BSDF **bsdf, float *pdf, VisibilityTester *visibility) const {luxError(LUX_BUG, LUX_SEVERE, "Unimplemented Light::Sample_L"); return 0.f;}
	virtual float Pdf(const Scene *scene, const Point &p) const {luxError(LUX_BUG, LUX_SEVERE, "Unimplemented Light::Pdf"); return 0.f;}

	void AddPortalShape(boost::shared_ptr<Shape> shape);

	virtual void SamplePosition(float u1, float u2, Point *p, Normal *n, float *pdf) const
	{
		luxError(LUX_BUG,LUX_SEVERE,"Unimplemented Light::SamplePosition() method called");
	}
	virtual void SampleDirection(float u1, float u2,const Normal &nn, Vector *wo, float *pdf) const
	{
		luxError(LUX_BUG,LUX_SEVERE,"Unimplemented Light::SampleDirection() method called");
	}
	virtual float EvalPositionPdf(const Point p, const Normal &n, const Vector &w) const
	{
		luxError(LUX_BUG,LUX_SEVERE,"Unimplemented Light::EvalPositionPdf() method called");
		return 0;
	}
	virtual float EvalDirectionPdf(const Point p, const Normal &n, const Vector &w) const
	{
		luxError(LUX_BUG,LUX_SEVERE,"Unimplemented Light::EvalDirectionPdf() method called");
		return 0;
	}
	virtual SWCSpectrum Eval(const Normal &n,	const Vector &w) const
	{
		luxError(LUX_BUG,LUX_SEVERE,"Unimplemented Light::Eval() method called");
		return SWCSpectrum(0.);
	}

	// Light Public Data
	const int nSamples;
	bool havePortalShape;
	int nrPortalShapes;
	vector<boost::shared_ptr<Shape> > PortalShapes;
	float PortalArea;
protected:
	// Light Protected Data
	const Transform LightToWorld, WorldToLight;
};
struct  VisibilityTester {
	// VisibilityTester Public Methods
	void SetSegment(const Point &p1, const Point &p2) {
		r = Ray(p1, p2-p1, RAY_EPSILON, 1.f - RAY_EPSILON);
	}
	void SetRay(const Point &p, const Vector &w) {
		r = Ray(p, w, RAY_EPSILON);
	}
	bool Unoccluded(const Scene *scene) const;
	SWCSpectrum Transmittance(const Scene *scene) const;
	Ray r;
};
class AreaLight : public Light {
public:
	// AreaLight Interface
	AreaLight(const Transform &light2world,
		const Spectrum &power, float g, int ns, const boost::shared_ptr<Shape> &shape);
	virtual SWCSpectrum L(const Point &p, const Normal &n,
			const Vector &w) const {
		return Dot(n, w) > 0 ? SWCSpectrum(LSPD) : 0.;
	}
	virtual SWCSpectrum L(const Ray &ray, const DifferentialGeometry &dg, const Normal &n, BSDF **bsdf, float *pdf, float *pdfDirect) const;
	SWCSpectrum Power(const Scene *) const {
		return SWCSpectrum(LSPD) * area * M_PI;
	}
	bool IsDeltaLight() const { return false; }
	float Pdf(const Point &, const Vector &) const;
	float Pdf(const Point &, const Normal &, const Vector &) const;
	SWCSpectrum Sample_L(const Point &P, Vector *w, VisibilityTester *visibility) const;
	virtual SWCSpectrum Sample_L(const Point &P, const Normal &N,
		float u1, float u2, float u3, Vector *wo, float *pdf,
		VisibilityTester *visibility) const;
	virtual SWCSpectrum Sample_L(const Point &P, float u1, float u2, float u3,
		Vector *wo, float *pdf, VisibilityTester *visibility) const;
	SWCSpectrum Sample_L(const Scene *scene, float u1, float u2,
			float u3, float u4, Ray *ray, float *pdf) const;
	virtual SWCSpectrum Sample_L(const Scene *scene, float u1, float u2, BSDF **bsdf, float *pdf) const;
	virtual SWCSpectrum Sample_L(const Scene *scene, const Point &p, const Normal &n, float u1, float u2, float u3, BSDF **bsdf, float *pdf, VisibilityTester *visibility) const;
	virtual float Pdf(const Scene *scene, const Point &p) const;
	void SamplePosition(float u1, float u2, Point *p, Normal *n, float *pdf) const;
	void SampleDirection(float u1, float u2,const Normal &nn, Vector *wo, float *pdf) const;
	float EvalPositionPdf(const Point p, const Normal &n, const Vector &w) const;
	float EvalDirectionPdf(const Point p, const Normal &n, const Vector &w) const;
	SWCSpectrum Eval(const Normal &n,	const Vector &w) const;
			
	static AreaLight *CreateAreaLight(const Transform &light2world, const ParamSet &paramSet,
		const boost::shared_ptr<Shape> &shape);
protected:
	// AreaLight Protected Data
	SPD *LSPD;
	boost::shared_ptr<Shape> shape;
	float area;
};

}//namespace lux

#endif // LUX_LIGHT_H
