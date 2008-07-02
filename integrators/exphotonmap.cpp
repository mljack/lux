/***************************************************************************
 *   Copyright (C) 1998-2008 by authors (see AUTHORS.txt )                 *
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

// exphotonmap.cpp*
#include "exphotonmap.h"
#include "bxdf.h"
#include "light.h"
#include "paramset.h"
#include "spectrumwavelengths.h"

#include <boost/thread/xtime.hpp>

using namespace lux;

// thread specific wavelengths
extern boost::thread_specific_ptr<SpectrumWavelengths> thread_wavelengths;

// Lux (copy) constructor
ExPhotonIntegrator* ExPhotonIntegrator::clone() const {
    return new ExPhotonIntegrator(*this);
}

// ExPhotonIntegrator Method Definitions
EPhotonProcess::EPhotonProcess(u_int mp, const Point &P)
: p(P) {
    photons = 0;
    nLookup = mp;
    foundPhotons = 0;
}

Spectrum ExPhotonIntegrator::estimateE(
        KdTree<EPhoton, EPhotonProcess> *map, int count,
        const Point &p, const Normal &n) const {
    if (!map) return 0.f;
    // Lookup nearby photons at irradiance computation point
    EPhotonProcess proc(nLookup, p);
    proc.photons = (EClosePhoton *) alloca(nLookup *
            sizeof (EClosePhoton));
    float md2 = maxDistSquared;
    map->Lookup(p, proc, md2);

    // Accumulate irradiance value from nearby photons
    EClosePhoton *photons = proc.photons;
    Spectrum E(0.);
	for (u_int i = 0; i < proc.foundPhotons; ++i) {
		if (Dot(n, photons[i].photon->wi) > 0.)
            E += photons[i].photon->alpha;
	}

    return E / (float(count) * md2 * M_PI);
}

void EPhotonProcess::operator()(const EPhoton &photon,
        float distSquared, float &maxDistSquared) const {
    // Do usual photon heap management
    // radiance - disabled for threading // static StatsPercentage discarded("Photon Map", "Discarded photons"); // NOBOOK
    // radiance - disabled for threading // discarded.Add(0, 1); // NOBOOK
    if (foundPhotons < nLookup) {
        // Add photon to unordered array of photons
        photons[foundPhotons++] = EClosePhoton(&photon, distSquared);
        if (foundPhotons == nLookup) {
            std::make_heap(&photons[0], &photons[nLookup]);
            maxDistSquared = photons[0].distanceSquared;
        }
    } else {
        // Remove most distant photon from heap and add new photon
        // radiance - disabled for threading // discarded.Add(1, 0); // NOBOOK
        std::pop_heap(&photons[0], &photons[nLookup]);
        photons[nLookup - 1] = EClosePhoton(&photon, distSquared);
        std::push_heap(&photons[0], &photons[nLookup]);
        maxDistSquared = photons[0].distanceSquared;
    }
}

Spectrum ExPhotonIntegrator::LPhoton(
        KdTree<EPhoton, EPhotonProcess> *map,
        int nPaths, int nLookup, BSDF *bsdf,
        const Intersection &isect, const Vector &wo,
        float maxDistSquared) const {
    Spectrum L(0.);
	if ((nPaths <= 0) || (!map)) return L;

    BxDFType diffuse = BxDFType(BSDF_REFLECTION |
            BSDF_TRANSMISSION | BSDF_DIFFUSE);
    if (bsdf->NumComponents(diffuse) == 0)
        return L;
    // radiance - disabled for threading // static StatsCounter lookups("Photon Map", "Total lookups"); // NOBOOK
    // Initialize _PhotonProcess_ object, _proc_, for photon map lookups
    EPhotonProcess proc(nLookup, isect.dg.p);
    proc.photons =
            (EClosePhoton *) alloca(nLookup * sizeof (EClosePhoton));
    // Do photon map lookup
    // radiance - disabled for threading // ++lookups;  // NOBOOK
    map->Lookup(isect.dg.p, proc, maxDistSquared);
    // Accumulate light from nearby photons
    // radiance - disabled for threading // static StatsRatio foundRate("Photon Map", "Photons found per lookup"); // NOBOOK
    // radiance - disabled for threading // foundRate.Add(proc.foundPhotons, 1); // NOBOOK
    // Estimate reflected light from photons
    EClosePhoton *photons = proc.photons;
    int nFound = proc.foundPhotons;
    Normal Nf = Dot(wo, bsdf->dgShading.nn) < 0 ? -bsdf->dgShading.nn :
            bsdf->dgShading.nn;

	// Dade - Glossy reflection/transmition is not done anymore with
	// photonmap
    /*if (bsdf->NumComponents(BxDFType(BSDF_REFLECTION |
            BSDF_TRANSMISSION)) > 0) {
        // Compute exitant radiance from photons for glossy surface
        for (int i = 0; i < nFound; ++i) {
            const EPhoton *p = photons[i].photon;
            BxDFType flag = Dot(Nf, p->wi) > 0.f ?
                    BSDF_ALL_REFLECTION : BSDF_ALL_TRANSMISSION;
            float k = Ekernel(p, isect.dg.p, maxDistSquared);

			Spectrum alpha = p->alpha;

            L += (k / nPaths) * bsdf->f(wo, p->wi, flag) * alpha;
        }
    } else */{
        // Compute exitant radiance from photons for diffuse surface
        Spectrum Lr(0.), Lt(0.);

        for (int i = 0; i < nFound; ++i) {			
			Spectrum alpha = photons[i].photon->alpha;

            if (Dot(Nf, photons[i].photon->wi) > 0.f) {
                float k = Ekernel(photons[i].photon, isect.dg.p,
                        maxDistSquared);
                Lr += (k / nPaths) * alpha;
            } else {
                float k = Ekernel(photons[i].photon, isect.dg.p,
                        maxDistSquared);
                Lt += (k / nPaths) * alpha;
            }
        }

        L += Lr * FromXYZ(bsdf->rho(wo, BSDF_ALL_REFLECTION).ToXYZ()) * INV_PI +
                Lt * FromXYZ(bsdf->rho(wo, BSDF_ALL_TRANSMISSION).ToXYZ()) * INV_PI;
    }

    return L;
}

ExPhotonIntegrator::ExPhotonIntegrator(
		RenderingMode rm,
		LightStrategy st,
		int ncaus, int nind, int maxDirPhotons,
        int nl, int mdepth, float mdist, bool fg,
        int gs, float ga,
		RRStrategy rrstrategy, float rrcontprob,
		bool dbgEnableDirect, bool dbgEnableCaustic,
		bool dbgEnableIndirect, bool dbgEnableSpecular) {
	renderingMode = rm;
	lightStrategy = st;

    nCausticPhotons = ncaus;
    nIndirectPhotons = nind;
	maxDirectPhotons = maxDirPhotons;

    nLookup = nl;
    maxDistSquared = mdist * mdist;
    maxDepth = mdepth;
    causticMap = indirectMap = NULL;
    radianceMap = NULL;
    finalGather = fg;
    gatherSamples = gs;
    cosGatherAngle = cos(Radians(ga));

	rrStrategy = rrstrategy;
	rrContinueProbability = rrcontprob;

	debugEnableDirect = dbgEnableDirect;
	debugEnableCaustic = dbgEnableCaustic;
	debugEnableIndirect = dbgEnableIndirect;
	debugEnableSpecular = dbgEnableSpecular;
}

ExPhotonIntegrator::~ExPhotonIntegrator() {
    delete causticMap;
    delete indirectMap;
    delete radianceMap;
}

void ExPhotonIntegrator::RequestSamples(Sample *sample,
        const Scene *scene) {
	if (lightStrategy == SAMPLE_AUTOMATIC) {
		if (scene->lights.size() > 5)
			lightStrategy = SAMPLE_ONE_UNIFORM;
		else
			lightStrategy = SAMPLE_ALL_UNIFORM;
	}

	// Dade - allocate and request samples

	if (renderingMode == RM_DIRECTLIGHTING) {
		vector<u_int> structure;

		structure.push_back(2);	// light position sample
		structure.push_back(1);	// light number/portal sample
		structure.push_back(2);	// bsdf direction sample for light
		structure.push_back(1);	// bsdf component sample for light

		structure.push_back(2);	// reflection bsdf direction sample
		structure.push_back(1);	// reflection bsdf component sample
		structure.push_back(2);	// transmission bsdf direction sample
		structure.push_back(1);	// transmission bsdf component sample

		sampleOffset = sample->AddxD(structure, maxDepth + 1);

		if (finalGather) {
			// Dade - use half samples for sampling along the BSDF and the other
			// half to sample along photon incoming direction
			gatherSamples = gatherSamples / 2;

			// Dade - request n samples for the final gather step
			structure.clear();
			structure.push_back(2);	// gather bsdf direction sample 1
			structure.push_back(1);	// gather bsdf component sample 1
			if (rrStrategy != RR_NONE)
				structure.push_back(1); // RR
			sampleFinalGather1Offset = sample->AddxD(structure, gatherSamples);

			structure.clear();
			structure.push_back(2);	// gather bsdf direction sample 2
			structure.push_back(1);	// gather bsdf component sample 2
			if (rrStrategy != RR_NONE)
				structure.push_back(1); // RR
			sampleFinalGather2Offset = sample->AddxD(structure, gatherSamples);
		}
	} else if (renderingMode == RM_PATH) {
		vector<u_int> structure;
		structure.push_back(2);	// light position sample
		structure.push_back(1);	// light number/portal sample
		structure.push_back(2);	// bsdf direction sample for light
		structure.push_back(1);	// bsdf component sample for light
		structure.push_back(2);	// bsdf direction sample for path
		structure.push_back(1);	// bsdf component sample for path
		structure.push_back(2);	// bsdf direction sample for indirect light
		structure.push_back(1);	// bsdf component sample for indirect light

		if (rrStrategy != RR_NONE)
			structure.push_back(1);	// continue sample
		
		sampleOffset = sample->AddxD(structure, maxDepth + 1);
	} else
		BOOST_ASSERT(false);
}

void ExPhotonIntegrator::Preprocess(const Scene *scene) {
    if (scene->lights.size() == 0) return;

    // Dade - shoot photons
    std::stringstream ss;
    ss << "Shooting photons: " << (nCausticPhotons + nIndirectPhotons);
    luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

	vector<EPhoton> causticPhotons;
    vector<EPhoton> indirectPhotons;
    vector<EPhoton> directPhotons;
    vector<ERadiancePhoton> radiancePhotons;
    causticPhotons.reserve(nCausticPhotons); // NOBOOK
    indirectPhotons.reserve(nIndirectPhotons); // NOBOOK

	// Dade - initialize SpectrumWavelengths
    SpectrumWavelengths *thr_wl = thread_wavelengths.get();

	bool causticDone = (nCausticPhotons == 0);
    bool indirectDone = (nIndirectPhotons == 0);
	bool directDone = false;

	// Compute light power CDF for photon shooting
	int nLights = int(scene->lights.size());
	float *lightPower = (float *)alloca(nLights * sizeof(float));
	float *lightCDF = (float *)alloca((nLights+1) * sizeof(float));

	// Dade - avarge the light power
	const int spectrumSamples = 128;
	for (int i = 0; i < nLights; ++i)
		lightPower[i] +=0.0f;
	for (int j = 0; j < spectrumSamples; j++) {
		thr_wl->Sample((float)RadicalInverse(j, 2),
			(float)RadicalInverse(j, 3));

		for (int i = 0; i < nLights; ++i)
			lightPower[i] += scene->lights[i]->Power(scene).y();
	}
	for (int i = 0; i < nLights; ++i)
		lightPower[i] *= 1.0f / spectrumSamples;

	float totalPower;
	ComputeStep1dCDF(lightPower, nLights, &totalPower, lightCDF);

    // Declare radiance photon reflectance arrays
    vector<Spectrum> rpReflectances, rpTransmittances;

	boost::xtime lastUpdateTime;
	boost::xtime_get(&lastUpdateTime, boost::TIME_UTC);
	int nDirectPaths = 0;
	int nshot = 0;
    while (!causticDone || !indirectDone) {
		// Dade - print some progress information
		boost::xtime currentTime;
		boost::xtime_get(&currentTime, boost::TIME_UTC);
		if (currentTime.sec - lastUpdateTime.sec > 5) {
			ss.str("");
			ss << "Direct photon count: " << directPhotons.size();
			if (maxDirectPhotons > 0)
				ss << " (" << (100 * directPhotons.size() / maxDirectPhotons) << "% limit)";
			else
				ss << " (100% limit)";
			ss << " Caustic photonmap size: " << causticPhotons.size();
			if (nCausticPhotons > 0)
				ss << " (" << (100 * causticPhotons.size() / nCausticPhotons) << "%)";
			else
				ss << " (100%)";
			ss << " Indirect photonmap size: " << indirectPhotons.size();
			if (nIndirectPhotons > 0)
				ss << " (" << (100 * indirectPhotons.size() / nIndirectPhotons) << "%)";
			else
				ss << " (100%)";
			luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

			lastUpdateTime = currentTime;
		}
		
		++nshot;

        // Give up if we're not storing enough photons
		if (nshot > 500000) {
			if (indirectDone && unsuccessful(nCausticPhotons, causticPhotons.size(), nshot)) {
				// Dade - disable castic photon map: we ar eunable to store
				// enough photons
				luxError(LUX_CONSISTENCY, LUX_WARNING, "Unable to store enough photons in the caustic photonmap.  Giving up and disabling the map.");

				causticPhotons.clear();
				causticDone = true;
				nCausticPhotons = 0;
			}

			if(unsuccessful(nIndirectPhotons, indirectPhotons.size(), nshot)) {
				luxError(LUX_CONSISTENCY, LUX_ERROR, "Unable to store enough photons in the indirect photonmap.  Giving up. I will unable to reder the image.");
				return;
			}
        }

        // Trace a photon path and store contribution
        // Choose 4D sample values for photon
        float u[4];
        u[0] = RadicalInverse(nshot, 2);
        u[1] = RadicalInverse(nshot, 3);
        u[2] = RadicalInverse(nshot, 5);
        u[3] = RadicalInverse(nshot, 7);

        // Dade - for SpectrumWavelengths
        thr_wl->Sample(RadicalInverse(nshot, 23),
				RadicalInverse(nshot, 29));

        // Choose light to shoot photon from
		float lightPdf;
		float uln = RadicalInverse(nshot, 11);
		int lightNum = Floor2Int(SampleStep1d(lightPower, lightCDF,
				totalPower, nLights, uln, &lightPdf) * nLights);
		lightNum = min(lightNum, nLights - 1);
		const Light *light = scene->lights[lightNum];

        // Generate _photonRay_ from light source and initialize _alpha_
        RayDifferential photonRay;
        float pdf;
        SWCSpectrum alpha = light->Sample_L(scene, u[0], u[1], u[2], u[3],
                &photonRay, &pdf);
        if (pdf == 0.f || alpha.Black()) continue;
        alpha /= pdf * lightPdf;

        if (!alpha.Black()) {
            // Follow photon path through scene and record intersections
            bool specularPath = false;
            Intersection photonIsect;
            int nIntersections = 0;
            while (scene->Intersect(photonRay, &photonIsect)) {
                ++nIntersections;

                // Handle photon/surface intersection
                alpha *= scene->Transmittance(photonRay);
                Vector wo = -photonRay.d;

                BSDF *photonBSDF = photonIsect.GetBSDF(photonRay, lux::random::floatValue());
                BxDFType specularType = BxDFType(BSDF_REFLECTION |
                        BSDF_TRANSMISSION | BSDF_SPECULAR | BSDF_GLOSSY);
                bool hasNonSpecularGlossy = (photonBSDF->NumComponents() >
                        photonBSDF->NumComponents(specularType));
                if (hasNonSpecularGlossy) {
                    // Deposit photon at surface
                    EPhoton photon(photonIsect.dg.p, alpha, wo);
                    if (nIntersections == 1) {
						if (finalGather && (!directDone)) {
							// Deposit direct photon
							directPhotons.push_back(photon);

							// Dade - check if we have enough direct photons
							if (directPhotons.size() == maxDirectPhotons) {
								directDone = true;
								nDirectPaths = nshot;
							}
						}
                    } else {
                        // Deposit either caustic or indirect photon
                        if (specularPath) {
                            // Process caustic photon intersection
                            if (!causticDone) {
                                causticPhotons.push_back(photon);

                                if (causticPhotons.size() == nCausticPhotons) {
                                    causticDone = true;
                                    nCausticPaths = nshot;
                                    causticMap = new KdTree<EPhoton, EPhotonProcess > (causticPhotons);
                                }
                            }
                        } else {
                            // Process indirect lighting photon intersection
                            if (!indirectDone) {
                                indirectPhotons.push_back(photon);

                                if (indirectPhotons.size() == nIndirectPhotons) {
                                    indirectDone = true;
                                    nIndirectPaths = nshot;
                                    indirectMap = new KdTree<EPhoton, EPhotonProcess > (indirectPhotons);
                                }
                            }
                        }
                    }

                    if (finalGather && (!directDone) &&
							(lux::random::floatValue() < .125f)) {
                        // Store data for radiance photon
                        // radiance - disabled for threading // static StatsCounter rp("Photon Map", "Radiance photons created"); // NOBOOK ++rp; // NOBOOK
                        Normal n = photonIsect.dg.nn;
                        if (Dot(n, photonRay.d) > 0.f) n = -n;
                        radiancePhotons.push_back(ERadiancePhoton(photonIsect.dg.p, n));
                        Spectrum rho_r = FromXYZ(photonBSDF->rho(BSDF_ALL_REFLECTION).ToXYZ());
                        rpReflectances.push_back(rho_r);
                        Spectrum rho_t = FromXYZ(photonBSDF->rho(BSDF_ALL_TRANSMISSION).ToXYZ());
                        rpTransmittances.push_back(rho_t);
                    }
                }

                // Sample new photon ray direction
                Vector wi;
                float pdf;
                BxDFType flags;
                // Get random numbers for sampling outgoing photon direction
                float u1, u2, u3;
                if (nIntersections == 1) {
                    u1 = RadicalInverse(nshot, 13);
                    u2 = RadicalInverse(nshot, 17);
                    u3 = RadicalInverse(nshot, 19);
                } else {
                    u1 = lux::random::floatValue();
                    u2 = lux::random::floatValue();
                    u3 = lux::random::floatValue();
                }

                // Compute new photon weight and possibly terminate with RR
                SWCSpectrum fr = photonBSDF->Sample_f(wo, &wi, u1, u2, u3,
                        &pdf, BSDF_ALL, &flags);
                if (fr.Black() || pdf == 0.f)
                    break;
                SWCSpectrum anew = alpha * fr *
                        AbsDot(wi, photonBSDF->dgShading.nn) / pdf;
                float continueProb = min<float>(1.0f, anew.filter() / alpha.filter());
                if (lux::random::floatValue() > continueProb || nIntersections > 10)
                    break;
				alpha = anew / continueProb;
                specularPath = (nIntersections == 1 || specularPath) &&
                        ((flags & BSDF_SPECULAR) != 0);
                photonRay = RayDifferential(photonIsect.dg.p, wi);
            }
        }

        BSDF::FreeAll();
    }

    if (finalGather) {
		luxError(LUX_NOERROR, LUX_INFO, "Precompute radiance");

		// Precompute radiance at a subset of the photons
		KdTree<EPhoton, EPhotonProcess> directMap(directPhotons);
		if (!directDone)
			nDirectPaths = nshot;

        for (u_int i = 0; i < radiancePhotons.size(); ++i) {
			// Dade - print some progress info
			boost::xtime currentTime;
			boost::xtime_get(&currentTime, boost::TIME_UTC);
			if (currentTime.sec - lastUpdateTime.sec > 5) {
				ss.str("");
				ss << "Photon: " << i << " (" << (100 * i / radiancePhotons.size()) << "%)" ;
				luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

				lastUpdateTime = currentTime;
			}

            // Compute radiance for radiance photon _i_
            ERadiancePhoton &rp = radiancePhotons[i];
            const Spectrum &rho_r = rpReflectances[i];
            const Spectrum &rho_t = rpTransmittances[i];
            Spectrum E;
            Point p = rp.p;
            Normal n = rp.n;

            if (!rho_r.Black()) {
                E = estimateE(&directMap, nDirectPaths, p, n);
				E += estimateE(indirectMap, nIndirectPaths, p, n);
				E += estimateE(causticMap, nCausticPaths, p, n);

                rp.Lo += E * INV_PI * rho_r;
            }

            if (!rho_t.Black()) {
                E = estimateE(&directMap, nDirectPaths, p, -n);
				E += estimateE(indirectMap, nIndirectPaths, p, -n);
				E += estimateE(causticMap, nCausticPaths, p, -n);

                rp.Lo += E * INV_PI * rho_t;
            }
        }

        radianceMap = new KdTree<ERadiancePhoton,
                ERadiancePhotonProcess > (radiancePhotons);
    }

	luxError(LUX_NOERROR, LUX_INFO, "Photon shooting done");
}

SWCSpectrum ExPhotonIntegrator::Li(const Scene *scene,
        const RayDifferential &ray, const Sample *sample,
        float *alpha) const {
    SampleGuard guard(sample->sampler, sample);

	switch(renderingMode) {
		case RM_DIRECTLIGHTING: {
			SWCSpectrum L = LiDirectLigthtingMode(0, scene, ray, sample, alpha);
			sample->AddContribution(sample->imageX, sample->imageY,
					L.ToXYZ(), alpha ? *alpha : 1.0f);
		}
			break;
		case RM_PATH:
			LiPathMode(scene, ray, sample, alpha);
			break;
		default:
			BOOST_ASSERT(false);
	}

    return SWCSpectrum(-1.f);
}

SWCSpectrum ExPhotonIntegrator::LiDirectLigthtingMode(
        const int specularDepth,
        const Scene *scene,
        const RayDifferential &ray, const Sample *sample,
        float *alpha) const {
    // Compute reflected radiance with photon map
    SWCSpectrum L(0.0f);

	if (!indirectMap)
		return L;

    Intersection isect;
    if (scene->Intersect(ray, &isect)) {
		// Dade - collect samples
		float *sampleData = sample->sampler->GetLazyValues(const_cast<Sample *>(sample), sampleOffset, specularDepth);
		float *lightSample = &sampleData[0];
		float *bsdfSample = &sampleData[3];
		float *bsdfComponent = &sampleData[5];

		float *reflectionSample = &sampleData[6];
		float *reflectionComponent = &sampleData[8];
		float *transmissionSample = &sampleData[9];
		float *transmissionComponent = &sampleData[11];

		float *lightNum;
		if (lightStrategy == SAMPLE_ONE_UNIFORM)
			lightNum = &sampleData[2];
		else
			lightNum = NULL;

        if (alpha) *alpha = 1.0f;
        Vector wo = -ray.d;

        // Compute emitted light if ray hit an area light source
        L += isect.Le(wo);

        // Evaluate BSDF at hit point
        BSDF *bsdf = isect.GetBSDF(ray, fabsf(2.f *
				bsdfComponent[0]) - 1.0f);
        const Point &p = bsdf->dgShading.p;
        const Normal &n = bsdf->dgShading.nn;

		// Dade - a sanity check
		if (isnan(p.x) || isnan(p.y) || isnan(p.z)) {
			// Dade - something wrong here
			std::stringstream ss;
			ss << "Internal error in photonmap, received a NaN point in differential shading geometry: point = (" << p << ")";
			luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());
			return L;
		}

		if (debugEnableDirect) {
			// Apply direct lighting strategy
			switch (lightStrategy) {
				case SAMPLE_ALL_UNIFORM:
					L += UniformSampleAllLights(scene, p, n,
							wo, bsdf, sample,
							lightSample, bsdfSample, bsdfComponent);
					break;
				case SAMPLE_ONE_UNIFORM:
					L += UniformSampleOneLight(scene, p, n,
							wo, bsdf, sample,
							lightSample, lightNum, bsdfSample, bsdfComponent);
					break;
				default:
					break;
			}
		}

		if (debugEnableCaustic) {
			// Compute indirect lighting for photon map integrator
			L += LPhoton(causticMap, nCausticPaths, nLookup, bsdf,
					isect, wo, maxDistSquared);
		}

		if (debugEnableIndirect) {
			if (finalGather) {
				// Do one-bounce final gather for photon map
				BxDFType nonSpecular = BxDFType(BSDF_REFLECTION |
						BSDF_TRANSMISSION | BSDF_DIFFUSE);
				if (bsdf->NumComponents(nonSpecular) > 0) {
					// Find indirect photons around point for importance sampling
					u_int nIndirSamplePhotons = 50;
					EPhotonProcess proc(nIndirSamplePhotons, p);
					proc.photons = (EClosePhoton *) alloca(nIndirSamplePhotons *
							sizeof (EClosePhoton));
					float searchDist2 = maxDistSquared;

					int sanityCheckIndex = 0;
					while (proc.foundPhotons < nIndirSamplePhotons) {
						float md2 = searchDist2;
						proc.foundPhotons = 0;
						indirectMap->Lookup(p, proc, md2);

						searchDist2 *= 2.f;

						if(sanityCheckIndex++ > 32) {
							// Dade - something wrong here
							std::stringstream ss;
							ss << "Internal error in photonmap: point = (" <<
									p << ") searchDist2 = " << searchDist2;
							luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());
							break;
						}
					}

					// Copy photon directions to local array
					Vector *photonDirs = (Vector *) alloca(nIndirSamplePhotons *
							sizeof (Vector));
					for (u_int i = 0; i < nIndirSamplePhotons; ++i)
						photonDirs[i] = proc.photons[i].photon->wi;

					// Use BSDF to do final gathering
					SWCSpectrum Li = 0.;
					// radiance - disabled for threading // static StatsCounter gatherRays("Photon Map", "Final gather rays traced"); // NOBOOK
					for (int i = 0; i < gatherSamples; ++i) {
						float *sampleFGData = sample->sampler->GetLazyValues(
							const_cast<Sample *>(sample), sampleFinalGather1Offset, i);

						// Sample random direction from BSDF for final gather ray
						Vector wi;
						float u1 = sampleFGData[0];
						float u2 = sampleFGData[1];
						float u3 = sampleFGData[2];
						float pdf;
						SWCSpectrum fr = bsdf->Sample_f(wo, &wi, u1, u2, u3,
								&pdf, BxDFType(BSDF_ALL & (~BSDF_SPECULAR)));
						if (fr.Black() || pdf == 0.f) continue;

						// Dade - russian roulette
						if (rrStrategy == RR_EFFICIENCY) { // use efficiency optimized RR
							const float dp = AbsDot(wi, n) / pdf;
							const float q = min<float>(1.0f, fr.filter() * dp);
							if (q < sampleFGData[3])
								continue;

							// increase contribution
							fr /= q;
						} else if (rrStrategy == RR_PROBABILITY) { // use normal/probability RR
							if (rrContinueProbability < sampleFGData[3])
								continue;

							// increase path contribution
							fr /= rrContinueProbability;
						}

						// Trace BSDF final gather ray and accumulate radiance
						RayDifferential bounceRay(p, wi);
						// radiance - disabled for threading // ++gatherRays; // NOBOOK
						Intersection gatherIsect;
						if (scene->Intersect(bounceRay, &gatherIsect)) {
							// Compute exitant radiance using precomputed irradiance
							SWCSpectrum Lindir = 0.f;
							Normal nGather = gatherIsect.dg.nn;
							if (Dot(nGather, bounceRay.d) > 0) nGather = -nGather;
							ERadiancePhotonProcess proc(gatherIsect.dg.p, nGather);
							float md2 = INFINITY;

							radianceMap->Lookup(gatherIsect.dg.p, proc, md2);
							if (proc.photon) {
								Lindir = proc.photon->Lo;

								Lindir *= scene->Transmittance(bounceRay);
								// Compute MIS weight for BSDF-sampled gather ray
								// Compute PDF for photon-sampling of direction _wi_
								float photonPdf = 0.f;
								float conePdf = UniformConePdf(cosGatherAngle);
								for (u_int j = 0; j < nIndirSamplePhotons; ++j)
									if (Dot(photonDirs[j], wi) > .999f * cosGatherAngle)
										photonPdf += conePdf;
								photonPdf /= nIndirSamplePhotons;
								float wt = PowerHeuristic(gatherSamples, pdf,
										gatherSamples, photonPdf);
								Li += fr * Lindir * AbsDot(wi, n) * wt / pdf;
							}
						}
					}
					L += Li / gatherSamples;

					// Use nearby photons to do final gathering
					Li = 0.;
					for (int i = 0; i < gatherSamples; ++i) {
						float *sampleFGData = sample->sampler->GetLazyValues(
							const_cast<Sample *>(sample), sampleFinalGather2Offset, i);

						// Sample random direction using photons for final gather ray
						float u1 = sampleFGData[2];
						float u2 = sampleFGData[0];
						float u3 = sampleFGData[1];
						int photonNum = min((int) nIndirSamplePhotons - 1,
								Floor2Int(u1 * nIndirSamplePhotons));
						// Sample gather ray direction from _photonNum_
						Vector vx, vy;
						CoordinateSystem(photonDirs[photonNum], &vx, &vy);
						Vector wi = UniformSampleCone(u2, u3, cosGatherAngle, vx, vy,
								photonDirs[photonNum]);
						// Trace photon-sampled final gather ray and accumulate radiance
						SWCSpectrum fr = bsdf->f(wo, wi);
						if (fr.Black()) continue;

						// Compute PDF for photon-sampling of direction _wi_
						float photonPdf = 0.f;
						float conePdf = UniformConePdf(cosGatherAngle);
						for (u_int j = 0; j < nIndirSamplePhotons; ++j)
							if (Dot(photonDirs[j], wi) > .999f * cosGatherAngle)
								photonPdf += conePdf;
						photonPdf /= nIndirSamplePhotons;
						
						// Dade - russian roulette
						if (rrStrategy == RR_EFFICIENCY) { // use efficiency optimized RR
							const float dp = 1.0f / photonPdf;
							const float q = min<float>(1.0f, fr.filter() * dp);
							if (q < sampleFGData[3])
								continue;

							// increase contribution
							fr /= q;
						} else if (rrStrategy == RR_PROBABILITY) { // use normal/probability RR
							if (rrContinueProbability < sampleFGData[3])
								continue;

							// increase path contribution
							fr /= rrContinueProbability;
						}

						RayDifferential bounceRay(p, wi);
						// radiance - disabled for threading // ++gatherRays; // NOBOOK
						Intersection gatherIsect;
						if (scene->Intersect(bounceRay, &gatherIsect)) {
							// Compute exitant radiance using precomputed irradiance
							SWCSpectrum Lindir = 0.f;
							Normal nGather = gatherIsect.dg.nn;
							if (Dot(nGather, bounceRay.d) > 0) nGather = -nGather;
							ERadiancePhotonProcess proc(gatherIsect.dg.p, nGather);
							float md2 = INFINITY;

							radianceMap->Lookup(gatherIsect.dg.p, proc, md2);
							if (proc.photon) {
								Lindir = proc.photon->Lo;

								Lindir *= scene->Transmittance(bounceRay);
								// Compute MIS weight for photon-sampled gather ray
								float bsdfPdf = bsdf->Pdf(wo, wi);
								float wt = PowerHeuristic(gatherSamples, photonPdf,
										gatherSamples, bsdfPdf);
								Li += fr * Lindir * AbsDot(wi, n) * wt / photonPdf;
							}
						}
					}

					L += Li / gatherSamples;
				}
			} else {
				L += LPhoton(indirectMap, nIndirectPaths, nLookup,
					bsdf, isect, wo, maxDistSquared);
			}
		}

        if (debugEnableSpecular && (specularDepth < maxDepth)) {
			float u1 = reflectionSample[0];
			float u2 = reflectionSample[1];
			float u3 = reflectionComponent[0];

            Vector wi;
			float pdf;
            // Trace rays for specular reflection and refraction
            SWCSpectrum f = bsdf->Sample_f(wo, &wi, u1, u2, u3,
                    &pdf, BxDFType(BSDF_REFLECTION | BSDF_SPECULAR | BSDF_GLOSSY));
            if ((!f.Black()) || (pdf > 0.0f)) {
                // Compute ray differential _rd_ for specular reflection
                RayDifferential rd(p, wi);
                rd.hasDifferentials = true;
                rd.rx.o = p + isect.dg.dpdx;
                rd.ry.o = p + isect.dg.dpdy;
                // Compute differential reflected directions
                Normal dndx = bsdf->dgShading.dndu * bsdf->dgShading.dudx +
                        bsdf->dgShading.dndv * bsdf->dgShading.dvdx;
                Normal dndy = bsdf->dgShading.dndu * bsdf->dgShading.dudy +
                        bsdf->dgShading.dndv * bsdf->dgShading.dvdy;
                Vector dwodx = -ray.rx.d - wo, dwody = -ray.ry.d - wo;
                float dDNdx = Dot(dwodx, n) + Dot(wo, dndx);
                float dDNdy = Dot(dwody, n) + Dot(wo, dndy);
                rd.rx.d = wi -
                        dwodx + 2 * Vector(Dot(wo, n) * dndx +
                        dDNdx * n);
                rd.ry.d = wi -
                        dwody + 2 * Vector(Dot(wo, n) * dndy +
                        dDNdy * n);
                L += LiDirectLigthtingMode(specularDepth + 1, scene, rd, sample, alpha) *
					(f * (1.0f / pdf)) * AbsDot(wi, n);
            }

			u1 = transmissionSample[0];
			u2 = transmissionSample[1];
			u3 = transmissionComponent[0];

            f = bsdf->Sample_f(wo, &wi, u1, u2, u3,
                    &pdf, BxDFType(BSDF_TRANSMISSION | BSDF_SPECULAR | BSDF_GLOSSY));
            if ((!f.Black()) || (pdf > 0.0f)) {
                // Compute ray differential _rd_ for specular transmission
                RayDifferential rd(p, wi);
                rd.hasDifferentials = true;
                rd.rx.o = p + isect.dg.dpdx;
                rd.ry.o = p + isect.dg.dpdy;

                float eta = bsdf->eta;
                Vector w = -wo;
                if (Dot(wo, n) < 0) eta = 1.f / eta;

                Normal dndx = bsdf->dgShading.dndu * bsdf->dgShading.dudx + bsdf->dgShading.dndv * bsdf->dgShading.dvdx;
                Normal dndy = bsdf->dgShading.dndu * bsdf->dgShading.dudy + bsdf->dgShading.dndv * bsdf->dgShading.dvdy;

                Vector dwodx = -ray.rx.d - wo, dwody = -ray.ry.d - wo;
                float dDNdx = Dot(dwodx, n) + Dot(wo, dndx);
                float dDNdy = Dot(dwody, n) + Dot(wo, dndy);

                float mu = eta * Dot(w, n) - Dot(wi, n);
                float dmudx = (eta - (eta * eta * Dot(w, n)) / Dot(wi, n)) * dDNdx;
                float dmudy = (eta - (eta * eta * Dot(w, n)) / Dot(wi, n)) * dDNdy;

                rd.rx.d = wi + eta * dwodx - Vector(mu * dndx + dmudx * n);
                rd.ry.d = wi + eta * dwody - Vector(mu * dndy + dmudy * n);
                L += LiDirectLigthtingMode(specularDepth + 1, scene, rd, sample, alpha) *
					(f * (1.0f / pdf)) * AbsDot(wi, n);
            }
        }
    } else {
        // Handle ray with no intersection
        if (alpha) *alpha = 0.0f;
        for (u_int i = 0; i < scene->lights.size(); ++i)
            L += scene->lights[i]->Le(ray);
        if (alpha && !L.Black()) *alpha = 1.0f;
    }

    return L * scene->volumeIntegrator->Transmittance(scene, ray, sample, alpha) +
			scene->volumeIntegrator->Li(scene, ray, sample, alpha);
}

void ExPhotonIntegrator::LiPathMode(const Scene *scene,
		const RayDifferential &r, const Sample *sample,
		float *alpha) const
{
	SampleGuard guard(sample->sampler, sample);
	// Declare common path integration variables
	RayDifferential ray(r);
	SWCSpectrum pathThroughput(1.0f);
	XYZColor color;
	float V = .1f;
	bool specularBounce = true, specular = true;
	if (alpha) *alpha = 1.;
	for (int pathLength = 0; ; ++pathLength) {
		// Find next vertex of path
		Intersection isect;
		if (!scene->Intersect(ray, &isect)) {
			if (pathLength == 0) {
				// Dade - now I know ray.maxt and I can call volumeIntegrator
				SWCSpectrum L = scene->volumeIntegrator->Li(scene, ray, sample, alpha);
				color = L.ToXYZ();
				if (color.y() > 0.f)
					sample->AddContribution(sample->imageX, sample->imageY,
						color, alpha ? *alpha : 1.f, V);
				pathThroughput = scene->volumeIntegrator->Transmittance(scene, ray, sample, alpha);
			}

			// Stop path sampling since no intersection was found
			// Possibly add emitted light
			// NOTE - Added by radiance - adds horizon in render & reflections
			if (specularBounce) {
				SWCSpectrum Le(0.f);
				for (u_int i = 0; i < scene->lights.size(); ++i)
					Le += scene->lights[i]->Le(ray);
				Le *= pathThroughput;
				color = Le.ToXYZ();
			}
			// Set alpha channel
			if (pathLength == 0 && alpha && !(color.y() > 0.f))
				*alpha = 0.;
			if (color.y() > 0.f)
				sample->AddContribution(sample->imageX, sample->imageY,
					color, alpha ? *alpha : 1.f, V);
			break;
		}
		if (pathLength == 0)
			r.maxt = ray.maxt;

		SWCSpectrum Lv(scene->volumeIntegrator->Li(scene, ray, sample, alpha));
		Lv *= pathThroughput;
		color = Lv.ToXYZ();
		if (color.y() > 0.f)
			sample->AddContribution(sample->imageX, sample->imageY,
				color, alpha ? *alpha : 1.f, V);
		pathThroughput *= scene->volumeIntegrator->Transmittance(scene, ray, sample, alpha);

		// Possibly add emitted light at path vertex
		Vector wo(-ray.d);
		if (specularBounce) {
			SWCSpectrum Le(isect.Le(wo));
			Le *= pathThroughput;
			color = Le.ToXYZ();
			if (color.y() > 0.f)
				sample->AddContribution(sample->imageX, sample->imageY,
					color, alpha ? *alpha : 1.f, V);
		}
		if (pathLength == maxDepth)
			break;

		// Dade - collect samples
		float *sampleData = sample->sampler->GetLazyValues(const_cast<Sample *>(sample), sampleOffset, pathLength);
		float *lightSample = &sampleData[0];
		float *bsdfSample = &sampleData[3];
		float *bsdfComponent = &sampleData[5];
		float *pathSample = &sampleData[6];
		float *pathComponent = &sampleData[8];
		float *indirectSample = &sampleData[9];
		float *indirectComponent = &sampleData[11];

		float *lightNum;
		if (lightStrategy == SAMPLE_ONE_UNIFORM) {
			lightNum = &sampleData[2];
		} else
			lightNum = NULL;

		float *rrSample;
		if (rrStrategy != RR_NONE)
			rrSample = &sampleData[12];
		else
			rrSample = NULL;
		
		// Evaluate BSDF at hit point
		BSDF *bsdf = isect.GetBSDF(ray, fabsf(2.f * bsdfComponent[0] - 1.f));
		// Sample illumination from lights to find path contribution
		const Point &p = bsdf->dgShading.p;
		const Normal &n = bsdf->dgShading.nn;

		// Dade - direct lighting
		if (debugEnableDirect) {
			SWCSpectrum Ll;
			switch (lightStrategy) {
				case SAMPLE_ALL_UNIFORM:
					Ll = UniformSampleAllLights(scene, p, n,
						wo, bsdf, sample,
						lightSample, bsdfSample, bsdfComponent);
					break;
				case SAMPLE_ONE_UNIFORM:
					Ll = UniformSampleOneLight(scene, p, n,
						wo, bsdf, sample,
						lightSample, lightNum, bsdfSample, bsdfComponent);
					break;
				default:
					Ll = 0.f;
			}
			Ll *= pathThroughput;
			color = Ll.ToXYZ();
			if (color.y() > 0.f)
				sample->AddContribution(sample->imageX, sample->imageY,
					color, alpha ? *alpha : 1.f, V);
		}

		// Dade - add caustic		
		if (debugEnableCaustic) {
			// Compute indirect lighting for photon map integrator
			SWCSpectrum Lc = LPhoton(causticMap, nCausticPaths, nLookup, bsdf,
					isect, wo, maxDistSquared);
			Lc *= pathThroughput;
			color = Lc.ToXYZ();
			if (color.y() > 0.f)
				sample->AddContribution(sample->imageX, sample->imageY,
					color, alpha ? *alpha : 1.f, V);
		}

		// Dade - add indirect lighting
		if (debugEnableIndirect) {
			BxDFType nonSpecularGlossy = BxDFType(BSDF_REFLECTION |
					BSDF_TRANSMISSION | BSDF_DIFFUSE);
			if (bsdf->NumComponents(nonSpecularGlossy) > 0) {

				Vector wi;
				float u1 = indirectSample[0];
				float u2 = indirectSample[1];
				float u3 = indirectComponent[0];
				float pdf;
				SWCSpectrum fr = bsdf->Sample_f(wo, &wi, u1, u2, u3,
						&pdf, nonSpecularGlossy);
				if (!fr.Black() && (pdf != 0.f)) {
					RayDifferential bounceRay(p, wi);

					Intersection gatherIsect;
					if (scene->Intersect(bounceRay, &gatherIsect)) {
						// Compute exitant radiance using precomputed irradiance
						SWCSpectrum Lindir = 0.f;
						Normal nGather = gatherIsect.dg.nn;
						if (Dot(nGather, bounceRay.d) > 0) nGather = -nGather;
						ERadiancePhotonProcess proc(gatherIsect.dg.p, nGather);
						float md2 = INFINITY;

						radianceMap->Lookup(gatherIsect.dg.p, proc, md2);
						if (proc.photon) {
							Lindir = proc.photon->Lo;

							Lindir *= scene->Transmittance(bounceRay);
							SWCSpectrum Li = fr * Lindir * (AbsDot(wi, n) / pdf);

							Li *= pathThroughput;
							color = Li.ToXYZ();
							if (color.y() > 0.f)
								sample->AddContribution(sample->imageX, sample->imageY,
									color, alpha ? *alpha : 1.f, V);
						}
					}
				}
			}
		}

		BxDFType specularGlossy = BxDFType(BSDF_ALL & (~BSDF_DIFFUSE));
		if (bsdf->NumComponents(specularGlossy) <= 0)
			break;

		// Sample BSDF to get new path direction
		Vector wi;
		float pdf;
		BxDFType flags;
		SWCSpectrum f = bsdf->Sample_f(wo, &wi, pathSample[0], pathSample[1], pathComponent[0],
			&pdf, specularGlossy, &flags);
		if (pdf == .0f || f.Black())
			break;

		const float dp = AbsDot(wi, n) / pdf;

		// Possibly terminate the path
		if (pathLength > 3) {
			if (rrStrategy == RR_EFFICIENCY) { // use efficiency optimized RR
				const float q = min<float>(1.f, f.filter() * dp);
				if (q < rrSample[0])
					break;
				// increase path contribution
				pathThroughput /= q;
			} else if (rrStrategy == RR_PROBABILITY) { // use normal/probability RR
				if (rrContinueProbability < rrSample[0])
					break;
				// increase path contribution
				pathThroughput /= rrContinueProbability;
			}
		}

		specularBounce = (flags & BSDF_SPECULAR) != 0;
		specular = specular && specularBounce;
		pathThroughput *= f;
		pathThroughput *= dp;
		if (!specular)
			V += dp;

		ray = RayDifferential(p, wi);
	}
}

SurfaceIntegrator* ExPhotonIntegrator::CreateSurfaceIntegrator(const ParamSet &params) {
	LightStrategy estrategy;
	string st = params.FindOneString("strategy", "auto");
	if (st == "one") estrategy = SAMPLE_ONE_UNIFORM;
	else if (st == "all") estrategy = SAMPLE_ALL_UNIFORM;
	else if (st == "auto") estrategy = SAMPLE_AUTOMATIC;
	else {
		std::stringstream ss;
		ss<<"Strategy  '"<<st<<"' for direct lighting unknown. Using \"auto\".";
		luxError(LUX_BADTOKEN,LUX_WARNING,ss.str().c_str());
		estrategy = SAMPLE_AUTOMATIC;
	}

    int nCaustic = params.FindOneInt("causticphotons", 20000);
    int nIndirect = params.FindOneInt("indirectphotons", 200000);
	int maxDirect = params.FindOneInt("maxdirectphotons", 1000000);

	int nUsed = params.FindOneInt("nused", 50);
    int maxDepth = params.FindOneInt("maxdepth", 6);

	bool finalGather = params.FindOneBool("finalgather", true);
    int gatherSamples = params.FindOneInt("finalgathersamples", 32);
	string smode =  params.FindOneString("renderingmode", "directlighting");

	RenderingMode renderingMode;
	if (smode == "directlighting") renderingMode = RM_DIRECTLIGHTING;
	else if (smode == "path") {
		renderingMode = RM_PATH;
		finalGather = true;
	} else {
		std::stringstream ss;
		ss<<"Strategy  '" << smode << "' for rendering mode unknown. Using \"directlighting\".";
		luxError(LUX_BADTOKEN,LUX_WARNING,ss.str().c_str());
		renderingMode = RM_DIRECTLIGHTING;
	}

	float maxDist = params.FindOneFloat("maxdist", 0.5f);
    float gatherAngle = params.FindOneFloat("gatherangle", 10.0f);

	RRStrategy rstrategy;
	string rst = params.FindOneString("gatherrrstrategy", "efficiency");
	if (rst == "efficiency") rstrategy = RR_EFFICIENCY;
	else if (rst == "probability") rstrategy = RR_PROBABILITY;
	else if (rst == "none") rstrategy = RR_NONE;
	else {
		std::stringstream ss;
		ss<<"Strategy  '"<<st<<"' for russian roulette path termination unknown. Using \"efficiency\".";
		luxError(LUX_BADTOKEN,LUX_WARNING,ss.str().c_str());
		rstrategy = RR_EFFICIENCY;
	}
	// continueprobability for plain RR (0.0-1.0)
	float rrcontinueProb = params.FindOneFloat("gatherrrcontinueprob", 0.65f);

	bool debugEnableDirect = params.FindOneBool("dbg_enabledirect", true);
	bool debugEnableCaustic = params.FindOneBool("dbg_enablecaustic", true);
	bool debugEnableIndirect = params.FindOneBool("dbg_enableindirect", true);
	bool debugEnableSpecular = params.FindOneBool("dbg_enablespecular", true);

    return new ExPhotonIntegrator(renderingMode, estrategy, nCaustic, nIndirect, maxDirect,
            nUsed, maxDepth, maxDist, finalGather, gatherSamples, gatherAngle,
			rstrategy, rrcontinueProb,
			debugEnableDirect, debugEnableCaustic, debugEnableIndirect, debugEnableSpecular);
}
