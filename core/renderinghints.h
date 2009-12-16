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

#ifndef _RENDERINGHINTS_H
#define	_RENDERINGHINTS_H

#include "lux.h"
#include "paramset.h"
#include "scene.h"

namespace lux {

//******************************************************************************
// Strategies
//******************************************************************************

class Strategy {
public:
	virtual void InitParam(const ParamSet &params) = 0;
	virtual void Init(const Scene *scene) = 0;
};

//------------------------------------------------------------------------------
// Light Sampling Strategies
//------------------------------------------------------------------------------

class LightsSamplingStrategy : public Strategy {
public:
	enum LightStrategyType {
		NOT_SUPPORTED, // Used in the case LS strategies are not supported at all
		SAMPLE_ALL_UNIFORM, SAMPLE_ONE_UNIFORM,
		SAMPLE_AUTOMATIC, SAMPLE_ONE_IMPORTANCE,
		SAMPLE_ONE_POWER_IMPORTANCE, SAMPLE_ALL_POWER_IMPORTANCE,
		SAMPLE_ONE_LOG_POWER_IMPORTANCE
	};

	LightsSamplingStrategy() { }
	virtual void InitParam(const ParamSet &params) { }
	virtual void Init(const Scene *scene) { }

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const = 0;
	virtual u_int RequestSamplesCount(const Scene *scene) const = 0;

	// Note: results are added to L
	virtual u_int SampleLights(
		const TsPack *tspack, const Scene *scene, const u_int shadowRayCount,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const = 0;
};

class LSSAllUniform : public LightsSamplingStrategy {
public:
	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const;
	virtual u_int RequestSamplesCount(const Scene *scene) const {
		if (scene->sampler->IsMutating())
			return scene->lights.size() * 6;
		else
			return 6;
	}
	virtual u_int SampleLights(
		const TsPack *tspack, const Scene *scene, const u_int shadowRayCount,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;

};

class LSSOneUniform : public LightsSamplingStrategy {
public:
	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const;
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 6; }
	virtual u_int SampleLights(
		const TsPack *tspack, const Scene *scene, const u_int shadowRayCount,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;
};

class LSSAuto : public LightsSamplingStrategy {
public:
	LSSAuto() : strategy(NULL) { }
	virtual void Init(const Scene *scene) {
		// The check for mutating sampler is here for coherence with the
		// behavior of old code: otherwise LSSAllUniform would be used
		// instead of LSSOneUniform (i.e. the number of reported samples
		// per second would be a lot lower than in the past causing a lot
		// of complain for sure)
		if (scene->sampler->IsMutating() || (scene->lights.size() > 5))
			strategy = new LSSOneUniform();
		else
			strategy = new LSSAllUniform();

		strategy->Init(scene);
	}

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const {
		strategy->RequestSamples(scene, structure);
	}
	virtual u_int RequestSamplesCount(const Scene *scene) const {
		return strategy->RequestSamplesCount(scene);
	}

	virtual u_int SampleLights(
		const TsPack *tspack, const Scene *scene, const u_int shadowRayCount,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const {
		return strategy->SampleLights(tspack, scene, shadowRayCount,
				p, n, wo, bsdf, sample, sampleData, scale, L);
	}

private:
	LightsSamplingStrategy *strategy;
};

class LSSOneImportance : public LightsSamplingStrategy {
public:
	~LSSOneImportance() {
		delete lightImportance;
		delete lightCDF;
	}
	virtual void Init(const Scene *scene);

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const;
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 6; }
	virtual u_int SampleLights(
		const TsPack *tspack, const Scene *scene, const u_int shadowRayCount,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;

private:
	float *lightImportance;
	float totalImportance;
	float *lightCDF;
};

class LSSOnePowerImportance : public LightsSamplingStrategy {
public:
	~LSSOnePowerImportance() {
		delete lightPower;
		delete lightCDF;
	}
	virtual void Init(const Scene *scene);

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const;
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 6; }
	// Note: results are added to L
	virtual u_int SampleLights(
		const TsPack *tspack, const Scene *scene, const u_int shadowRayCount,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;

protected:
	float *lightPower;
	float totalPower;
	float *lightCDF;
};

class LSSAllPowerImportance : public LSSOnePowerImportance {
public:
	// Note: results are added to L
	virtual u_int SampleLights(
		const TsPack *tspack, const Scene *scene, const u_int shadowRayCount,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;
};

class LSSOneLogPowerImportance : public LSSOnePowerImportance {
public:
	virtual void Init(const Scene *scene);
};

//------------------------------------------------------------------------------
// Russian Roulette Strategies
//------------------------------------------------------------------------------

class RussianRouletteStrategy : public Strategy {
public:
	enum RRStrategyType {
		NOT_SUPPORTED, // Used in the case RR strategies are not supported at all
		NONE, EFFICIENCY, PROBABILITY, IMPORTANCE
	};

	RussianRouletteStrategy() { }

	virtual void InitParam(const ParamSet &params) { }
	virtual void Init(const Scene *scene) { }

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const = 0;
	virtual u_int RequestSamplesCount(const Scene *scene) const = 0;
	// pathLength = numeric_limits<u_int>::max() means this parameter should be
	// ignored by RR
	virtual float Continue(const float *sampleData,	const u_int pathLength,
		const float k1, const float k2) const = 0;
};

class RRNoneStrategy : public RussianRouletteStrategy {
public:
	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const { };
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 0; }
	virtual float Continue(const float *sampleData,
		const u_int pathLength, float k1, const float k2) const { return 1.0f; }
};

class RREfficiencyStrategy : public RussianRouletteStrategy {
public:
	RREfficiencyStrategy() : rrPathLength(3) { }
	virtual void InitParam(const ParamSet &params) {
		rrPathLength = max(params.FindOneInt("rrpathlength", 3), 0);
	}

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const { structure.push_back(1); }
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 1; }
	virtual float Continue(const float *sampleData,	const u_int pathLength,
		const float k1, const float k2) const {
		if (pathLength > rrPathLength) {
			const float q = min<float>(1.f, k1);
			if (q < sampleData[0])
				return 0.0f;
			else
				return q;
		} else
			return 1.0f;
	}

private:
	// After how many steps should RR start to work
	u_int rrPathLength;
};

class RRProbabilityStrategy : public RussianRouletteStrategy {
public:
	RRProbabilityStrategy() :
		rrPathLength(3), continueProbability(.65f) { }
	virtual void InitParam(const ParamSet &params) {
		rrPathLength = max(params.FindOneInt("rrpathlength", 3), 0);
		continueProbability = Clamp(params.FindOneFloat("rrcontinueprob", .65f), 0.f, 1.f);
	}

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const { structure.push_back(1); }
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 1; }
	virtual float Continue(const float *sampleData,	const u_int pathLength,
		const float k1, const float k2) const {
		if (pathLength > rrPathLength) {
			if (continueProbability < sampleData[0])
				return 0.f;
			else
				return continueProbability;
		} else
			return 1.0f;
	}

private:
	// After how many steps should RR start to work
	u_int rrPathLength;
	float continueProbability;
};

class RRImportanceStrategy : public RussianRouletteStrategy {
public:
	RRImportanceStrategy() : rrPathLength(1) { }
	virtual void InitParam(const ParamSet &params) {
		rrPathLength = max(params.FindOneInt("rrpathlength", 1), 0);
	}

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const { structure.push_back(1); }
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 1; }
	virtual float Continue(const float *sampleData,	const u_int pathLength,
		const float k1, const float k2) const {
		if (pathLength > rrPathLength) {
			const float q = min<float>(1.f, k2);
			if (q < sampleData[0])
				return 0.0f;
			else
				return q;
		} else
			return 1.0f;
	}

private:
	// After how many steps should RR start to work
	u_int rrPathLength;
};

//******************************************************************************
// Rendering Hints
//******************************************************************************

//------------------------------------------------------------------------------
// Light Rendering Hints
//------------------------------------------------------------------------------

class LightRenderingHints {
public:
	LightRenderingHints() {
		importance = 1.f;
	}
	void InitParam(const ParamSet &params);

	float GetImportance() const { return importance; }

private:
	float importance;
};

//------------------------------------------------------------------------------
// SurfaceIntegrator Rendering Hints
//------------------------------------------------------------------------------

class SurfaceIntegratorSupportedStrategies {
public:
	SurfaceIntegratorSupportedStrategies();

	void SetDefaultStrategy(LightsSamplingStrategy::LightStrategyType st) {
		defaultLSStrategies = st;
	}

	void SetDefaultStrategy(RussianRouletteStrategy::RRStrategyType st) {
		defaultRRStrategies = st;
	}

	LightsSamplingStrategy::LightStrategyType GetDefaultLightSamplingStrategy() {
		return defaultLSStrategies;
	}

	RussianRouletteStrategy::RRStrategyType GetDefaultRussianRouletteStrategy() {
		return defaultRRStrategies;
	}

	void AddStrategy(LightsSamplingStrategy::LightStrategyType st) {
		if (!this->IsSupported(st))
			supportedLSStrategies.push_back(st);
	}

	void AddStrategy(RussianRouletteStrategy::RRStrategyType st) {
		if (!this->IsSupported(st))
			supportedRRStrategies.push_back(st);
	}

	void RemoveStrategy(LightsSamplingStrategy::LightStrategyType st) {
		for (u_int i = 0; i < supportedLSStrategies.size(); i++) {
			if (st == supportedLSStrategies[i]) {
				supportedLSStrategies.erase(supportedLSStrategies.begin() + i);
				return;
			}
		}
	}

	void RemoveStrategy(RussianRouletteStrategy::RRStrategyType st) {
		for (u_int i = 0; i < supportedRRStrategies.size(); i++) {
			if (st == supportedRRStrategies[i]) {
				supportedRRStrategies.erase(supportedRRStrategies.begin() + i);
				return;
			}
		}
	}

	void RemoveAllLightsSamplingStrategy() {
		supportedLSStrategies.clear();
		this->SetDefaultStrategy(LightsSamplingStrategy::NOT_SUPPORTED);
	}

	void RemoveAllRussianRouletteStrategy() {
		supportedRRStrategies.clear();
		this->SetDefaultStrategy(RussianRouletteStrategy::NOT_SUPPORTED);
	}

	bool IsSupported(LightsSamplingStrategy::LightStrategyType st) {
		for (u_int i = 0; i < supportedLSStrategies.size(); i++) {
			if (st == supportedLSStrategies[i])
				return true;
		}

		return false;
	}

	bool IsSupported(RussianRouletteStrategy::RRStrategyType st) {
		for (u_int i = 0; i < supportedRRStrategies.size(); i++) {
			if (st == supportedRRStrategies[i])
				return true;
		}

		return false;
	}

private:
	LightsSamplingStrategy::LightStrategyType defaultLSStrategies;
	vector<LightsSamplingStrategy::LightStrategyType> supportedLSStrategies;

	RussianRouletteStrategy::RRStrategyType defaultRRStrategies;
	vector<RussianRouletteStrategy::RRStrategyType> supportedRRStrategies;
};

class SurfaceIntegratorRenderingHints {
public:
	SurfaceIntegratorRenderingHints() {
		shadowRayCount = 1;
		lightStrategyType = LightsSamplingStrategy::SAMPLE_AUTOMATIC;
		lsStrategy = NULL;

		rrStrategyType = RussianRouletteStrategy::EFFICIENCY;
		rrStrategy = NULL;
	}
	~SurfaceIntegratorRenderingHints() {
		if (lsStrategy)
			delete lsStrategy;
		if (rrStrategy)
			delete rrStrategy;
	}

	void InitParam(const ParamSet &params);

	SurfaceIntegratorSupportedStrategies& GetSupportedStrategies() { return supportedStrategies; }

	u_int GetShadowRaysCount() const { return shadowRayCount; }
	LightsSamplingStrategy::LightStrategyType GetLightStrategy() const { return lightStrategyType; }

	void InitStrategies(const Scene *scene);
	void RequestSamples(const Scene *scene, vector<u_int> &structure);

	// Note: results are added to L and optional parameter V content
	u_int SampleLights(
		const TsPack *tspack, const Scene *scene,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, vector<float> *V = NULL) const {
		const u_int nContribs = lsStrategy->SampleLights(tspack, scene,
				shadowRayCount, p, n, wo, bsdf, sample, &sampleData[lightSampleOffset], scale, L);

		if (V) {
			const float nLights = scene->lights.size();
			for (u_int i = 0; i < nLights; ++i)
				(*V)[i] += L[i].Filter(tspack);
		}

		return nContribs;
	}

	float RussianRouletteContinue(const float *sampleData,
		const u_int pathLength, const float k1, const float k2) const {
		return rrStrategy->Continue(&sampleData[rrSampleOffset], pathLength, k1, k2);
	}

private:
	SurfaceIntegratorSupportedStrategies supportedStrategies;

	// Light Strategies
	u_int shadowRayCount;
	LightsSamplingStrategy::LightStrategyType lightStrategyType;
	LightsSamplingStrategy *lsStrategy;
	u_int lightSampleOffset;

	// Russian Roulette Strategies
	RussianRouletteStrategy::RRStrategyType rrStrategyType;
	RussianRouletteStrategy *rrStrategy;
	u_int rrSampleOffset;
};

}

#endif	/* _RENDERINGHINTS_H */
