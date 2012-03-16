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

#include "renderers/statistics/hybridsamplerstatistics.h"
#include "camera.h"
#include "context.h"
#include "film.h"
#include "scene.h"

#include <limits>
#include <string>

#include <boost/format.hpp>

using namespace lux;

HSRStatistics::HSRStatistics(HybridSamplerRenderer* renderer)
	: renderer(renderer),
	windowSps(0.0), windowSampleCount(0.0),
	windowNetworkSps(0.0), windowNetworkStartTime(0.0), windowNetworkSampleCount(0.0)
{
	formatted = new HSRStatistics::Formatted(this);

	AddDoubleAttribute(*this, "haltSamplesPerPixel", "Average number of samples per pixel to complete before halting", &HSRStatistics::getHaltSpp);
	AddDoubleAttribute(*this, "remainingSamplesPerPixel", "Average number of samples per pixel remaining", &HSRStatistics::getRemainingSamplesPerPixel);
	AddDoubleAttribute(*this, "percentHaltSppComplete", "Percent of halt S/p completed", &HSRStatistics::getPercentHaltSppComplete);

	AddIntAttribute(*this, "gpuCount", "Number of GPUs in use", &HSRStatistics::getGpuCount);
	AddDoubleAttribute(*this, "gpuEfficiency", "Percent of time GPUs have rays available to trace", &HSRStatistics::getAverageGpuEfficiency);

	AddDoubleAttribute(*this, "samplesPerPixel", "Average number of samples per pixel by local node", &HSRStatistics::getAverageSamplesPerPixel);
	AddDoubleAttribute(*this, "samplesPerSecond", "Average number of samples per second by local node", &HSRStatistics::getAverageSamplesPerSecond);
	AddDoubleAttribute(*this, "samplesPerSecondWindow", "Average number of samples per second by local node in current time window", &HSRStatistics::getAverageSamplesPerSecondWindow);
	AddDoubleAttribute(*this, "contributionsPerSecond", "Average number of contributions per second by local node", &HSRStatistics::getAverageContributionsPerSecond);
	AddDoubleAttribute(*this, "contributionsPerSecondWindow", "Average number of contributions per second by local node in current time window", &HSRStatistics::getAverageContributionsPerSecondWindow);

	AddDoubleAttribute(*this, "netSamplesPerPixel", "Average number of samples per pixel by slave nodes", &HSRStatistics::getNetworkAverageSamplesPerPixel);
	AddDoubleAttribute(*this, "netSamplesPerSecond", "Average number of samples per second by slave nodes", &HSRStatistics::getNetworkAverageSamplesPerSecond);
	AddDoubleAttribute(*this, "netSamplesPerSecondWindow", "Average number of samples per second by slave nodes in current time window", &HSRStatistics::getNetworkAverageSamplesPerSecondWindow);

	AddDoubleAttribute(*this, "totalSamplesPerPixel", "Average number of samples per pixel", &HSRStatistics::getTotalAverageSamplesPerPixel);
	AddDoubleAttribute(*this, "totalSamplesPerSecond", "Average number of samples per second", &HSRStatistics::getTotalAverageSamplesPerSecond);
	AddDoubleAttribute(*this, "totalSamplesPerSecondWindow", "Average number of samples per second in current time window", &HSRStatistics::getTotalAverageSamplesPerSecondWindow);
}

void HSRStatistics::resetDerived() {
	windowSps = 0.0;
	windowSampleCount = 0.0;

	windowNetworkSps = 0.0;
	windowNetworkStartTime = 0.0;
	windowNetworkSampleCount = 0.0;
}

void HSRStatistics::updateStatisticsWindowDerived()
{
	// Get local sample count
	double sampleCount = getSampleCount();
	windowSps = (sampleCount - windowSampleCount) / (getElapsedTime() - windowStartTime);
	windowSampleCount = sampleCount;

	// Get network sample count
	double networkSampleCount = 0.0;
	Queryable* filmRegistry = Context::GetActive()->registry["film"];
	if (filmRegistry)
		networkSampleCount = (*filmRegistry)["numberOfSamplesFromNetwork"].DoubleValue();

	if (networkSampleCount != windowNetworkSampleCount)
	{
		windowNetworkSps = (networkSampleCount - windowNetworkSampleCount) / (getElapsedTime() - windowNetworkStartTime);
		windowNetworkSampleCount = networkSampleCount;
		windowNetworkStartTime = getElapsedTime();
	}
}

// Returns haltSamplesPerPixel if set, otherwise infinity
double HSRStatistics::getHaltSpp() {
	double haltSpp = 0;

	Queryable* filmRegistry = Context::GetActive()->registry["film"];
	if (filmRegistry)
		haltSpp = (*filmRegistry)["haltSamplesPerPixel"].IntValue();

	return haltSpp > 0.0 ? haltSpp : std::numeric_limits<double>::infinity();
}

double HSRStatistics::getEfficiency() {
	double sampleCount = 0.0;
	double blackSampleCount = 0.0;

	// Get the current counts from the renderthreads
	// Cannot just use getSampleCount() because the blackSampleCount is necessary
	boost::mutex::scoped_lock lock(renderer->classWideMutex);
	for (u_int i = 0; i < renderer->renderThreads.size(); ++i) {
		fast_mutex::scoped_lock lockStats(renderer->renderThreads[i]->statLock);
		sampleCount += renderer->renderThreads[i]->samples;
		blackSampleCount += renderer->renderThreads[i]->blackSamples;
	}

	return sampleCount ? (100.0 * blackSampleCount) / sampleCount : 0.0;
}

// Returns percent of haltSamplesPerPixel completed, zero if haltSamplesPerPixel is not set
double HSRStatistics::getPercentHaltSppComplete() {
	return ((getAverageSamplesPerPixel() + getNetworkAverageSamplesPerPixel()) / getHaltSpp()) * 100.0;
}

// Returns percent of GPU efficiency, zero if no GPUs
double HSRStatistics::getAverageGpuEfficiency() {
	double eff = 0.0;
	u_int gpuCount = getGpuCount();

	if (renderer->virtualIM2ODevice || renderer->virtualIM2MDevice)
		for (size_t i = 0; i < gpuCount; ++i)
			eff += renderer->hardwareDevices[i]->GetLoad();

	return (gpuCount == 0) ? 0.0 : (eff / gpuCount) * 100.0;
}

double HSRStatistics::getAverageSamplesPerSecond() {
	double et = getElapsedTime();
	return (et == 0.0) ? 0.0 : getSampleCount() / et;
}

double HSRStatistics::getNetworkAverageSamplesPerSecond() {
	double et = getElapsedTime();
	return (et == 0.0) ? 0.0 : getNetworkSampleCount() / et;
}

u_int HSRStatistics::getPixelCount() {
	int xstart, xend, ystart, yend;

	renderer->scene->camera->film->GetSampleExtent(&xstart, &xend, &ystart, &yend);

	return ((xend - xstart) * (yend - ystart));
}

double HSRStatistics::getSampleCount() {
	double sampleCount = 0.0;

	Queryable* filmRegistry = Context::GetActive()->registry["film"];
	if (filmRegistry)
		sampleCount = (*filmRegistry)["numberOfLocalSamples"].DoubleValue();

	return sampleCount;
}

double HSRStatistics::getNetworkSampleCount(bool estimate) {
	double networkSampleCount = 0.0;

	Queryable* filmRegistry = Context::GetActive()->registry["film"];
	if (filmRegistry)
		networkSampleCount = (*filmRegistry)["numberOfSamplesFromNetwork"].DoubleValue();

	// Add estimated network sample count
	if (estimate && networkSampleCount == windowNetworkSampleCount)
		networkSampleCount += ((getElapsedTime() - windowNetworkStartTime) * windowNetworkSps);

	return networkSampleCount;
}

HSRStatistics::Formatted::Formatted(HSRStatistics* rs)
	: RendererStatistics::Formatted(rs), rs(rs)
{
	AddStringAttribute(*this, "haltSamplesPerPixel", "Average number of samples per pixel to complete before halting", &HSRStatistics::Formatted::getHaltSpp);
	AddStringAttribute(*this, "remainingSamplesPerPixel", "Average number of samples per pixel remaining", &HSRStatistics::Formatted::getRemainingSamplesPerPixel);
	AddStringAttribute(*this, "percentHaltSppComplete", "Percent of halt S/p completed", &HSRStatistics::Formatted::getPercentHaltSppComplete);
	AddStringAttribute(*this, "_percentHaltSppComplete_short", "Percent of halt S/p completed", &HSRStatistics::Formatted::getPercentHaltSppCompleteShort);

	AddStringAttribute(*this, "gpuCount", "Number of GPUs in use", &HSRStatistics::Formatted::getGpuCount);
	AddStringAttribute(*this, "_gpuCount_short", "Number of GPUs in use", &HSRStatistics::Formatted::getGpuCountShort);
	AddStringAttribute(*this, "gpuEfficiency", "Percent of time GPUs have rays available to trace", &HSRStatistics::Formatted::getAverageGpuEfficiency);
	AddStringAttribute(*this, "_gpuEfficiency_short", "Percent of time GPUs have rays available to trace", &HSRStatistics::Formatted::getAverageGpuEfficiencyShort);

	AddStringAttribute(*this, "samplesPerPixel", "Average number of samples per pixel by local node", &HSRStatistics::Formatted::getAverageSamplesPerPixel);
	AddStringAttribute(*this, "samplesPerSecond", "Average number of samples per second by local node", &HSRStatistics::Formatted::getAverageSamplesPerSecond);
	AddStringAttribute(*this, "samplesPerSecondWindow", "Average number of samples per second by local node in current time window", &HSRStatistics::Formatted::getAverageSamplesPerSecondWindow);
	AddStringAttribute(*this, "contributionsPerSecond", "Average number of contributions per second by local node", &HSRStatistics::Formatted::getAverageContributionsPerSecond);
	AddStringAttribute(*this, "contributionsPerSecondWindow", "Average number of contributions per second by local node in current time window", &HSRStatistics::Formatted::getAverageContributionsPerSecondWindow);

	AddStringAttribute(*this, "netSamplesPerPixel", "Average number of samples per pixel by slave nodes", &HSRStatistics::Formatted::getNetworkAverageSamplesPerPixel);
	AddStringAttribute(*this, "netSamplesPerSecond", "Average number of samples per second by slave nodes", &HSRStatistics::Formatted::getNetworkAverageSamplesPerSecond);
	AddStringAttribute(*this, "netSamplesPerSecondWindow", "Average number of samples per second by slave nodes in current time window", &HSRStatistics::Formatted::getNetworkAverageSamplesPerSecondWindow);

	AddStringAttribute(*this, "totalSamplesPerPixel", "Average number of samples per pixel", &HSRStatistics::Formatted::getTotalAverageSamplesPerPixel);
	AddStringAttribute(*this, "totalSamplesPerSecond", "Average number of samples per second", &HSRStatistics::Formatted::getTotalAverageSamplesPerSecond);
	AddStringAttribute(*this, "totalSamplesPerSecondWindow", "Average number of samples per second in current time window", &HSRStatistics::Formatted::getTotalAverageSamplesPerSecondWindow);
}

std::string HSRStatistics::Formatted::getRecommendedStringTemplate()
{
	std::string stringTemplate = RendererStatistics::Formatted::getRecommendedStringTemplate();
	if (rs->getGpuCount() != 0)
		stringTemplate += " %gpuCount%";
	stringTemplate += ": %samplesPerPixel%";
	if (rs->getHaltSpp() != std::numeric_limits<double>::infinity())
		stringTemplate += " (%percentHaltSppComplete%)";
	stringTemplate += " %samplesPerSecondWindow% %contributionsPerSecondWindow% %efficiency%";
	if (rs->getGpuCount() != 0)
		stringTemplate += " %gpuEfficiency%";
	if (rs->getSlaveNodeCount() != 0 && rs->getNetworkAverageSamplesPerPixel() != 0.0)
		stringTemplate += " Net: ~%netSamplesPerPixel% ~%netSamplesPerSecondWindow% Tot: ~%totalSamplesPerPixel% ~%totalSamplesPerSecondWindow%";

	return stringTemplate;
}

std::string HSRStatistics::Formatted::getHaltSpp() {
	return boost::str(boost::format("%1% S/p") % rs->getHaltSpp());
}

std::string HSRStatistics::Formatted::getRemainingSamplesPerPixel() {
	double rspp = rs->getRemainingSamplesPerPixel();
	return boost::str(boost::format("%1$0.2f%2% S/p") % MagnitudeReduce(rspp) % MagnitudePrefix(rspp));
}

std::string HSRStatistics::Formatted::getPercentHaltSppComplete() {
	double haltp = rs->getPercentHaltSppComplete();
	return boost::str(boost::format("%1$0.0f%% S/p Complete") % haltp);
}

std::string HSRStatistics::Formatted::getPercentHaltSppCompleteShort() {
	double haltp = rs->getPercentHaltSppComplete();
	return boost::str(boost::format("%1$0.0f%% S/p Cmplt") % haltp);
}

std::string HSRStatistics::Formatted::getGpuCount() {
	u_int gc = rs->getGpuCount();
	return boost::str(boost::format("%1% %2%") % gc % pluralize("GPU", gc));
}

std::string HSRStatistics::Formatted::getGpuCountShort() {
	return boost::str(boost::format("%1% G") % rs->getGpuCount());
}

std::string HSRStatistics::Formatted::getAverageGpuEfficiency() {
	return boost::str(boost::format("%1$0.0f%% GPU Efficiency") % rs->getAverageGpuEfficiency());
}

std::string HSRStatistics::Formatted::getAverageGpuEfficiencyShort() {
	return boost::str(boost::format("%1$0.0f%% GEff") % rs->getAverageGpuEfficiency());
}

std::string HSRStatistics::Formatted::getAverageSamplesPerPixel() {
	double spp = rs->getAverageSamplesPerPixel();
	return boost::str(boost::format("%1$0.2f%2% S/p") % MagnitudeReduce(spp) % MagnitudePrefix(spp));
}

std::string HSRStatistics::Formatted::getAverageSamplesPerSecond() {
	double sps = rs->getAverageSamplesPerSecond();
	return boost::str(boost::format("%1$0.2f%2% S/s") % MagnitudeReduce(sps) % MagnitudePrefix(sps));
}

std::string HSRStatistics::Formatted::getAverageSamplesPerSecondWindow() {
	double spsw = rs->getAverageSamplesPerSecondWindow();
	return boost::str(boost::format("%1$0.2f%2% S/s") % MagnitudeReduce(spsw) % MagnitudePrefix(spsw));
}

std::string HSRStatistics::Formatted::getAverageContributionsPerSecond() {
	double cps = rs->getAverageContributionsPerSecond();
	return boost::str(boost::format("%1$0.2f%2% C/s") % MagnitudeReduce(cps) % MagnitudePrefix(cps));
}

std::string HSRStatistics::Formatted::getAverageContributionsPerSecondWindow() {
	double cpsw = rs->getAverageContributionsPerSecondWindow();
	return boost::str(boost::format("%1$0.2f%2% C/s") % MagnitudeReduce(cpsw) % MagnitudePrefix(cpsw));
}

std::string HSRStatistics::Formatted::getNetworkAverageSamplesPerPixel() {
	double spp = rs->getNetworkAverageSamplesPerPixel();
	return boost::str(boost::format("%1$0.2f%2% S/p") % MagnitudeReduce(spp) % MagnitudePrefix(spp));
}

std::string HSRStatistics::Formatted::getNetworkAverageSamplesPerSecond() {
	double sps = rs->getNetworkAverageSamplesPerSecond();
	return boost::str(boost::format("%1$0.2f%2% S/s") % MagnitudeReduce(sps) % MagnitudePrefix(sps));
}

std::string HSRStatistics::Formatted::getNetworkAverageSamplesPerSecondWindow() {
	double spsw = rs->getNetworkAverageSamplesPerSecondWindow();
	return boost::str(boost::format("%1$0.2f%2% S/s") % MagnitudeReduce(spsw) % MagnitudePrefix(spsw));
}

std::string HSRStatistics::Formatted::getTotalAverageSamplesPerPixel() {
	double spp = rs->getTotalAverageSamplesPerPixel();
	return boost::str(boost::format("%1$0.2f%2% S/p") % MagnitudeReduce(spp) % MagnitudePrefix(spp));
}

std::string HSRStatistics::Formatted::getTotalAverageSamplesPerSecond() {
	double sps = rs->getTotalAverageSamplesPerSecond();
	return boost::str(boost::format("%1$0.2f%2% S/s") % MagnitudeReduce(sps) % MagnitudePrefix(sps));
}

std::string HSRStatistics::Formatted::getTotalAverageSamplesPerSecondWindow() {
	double spsw = rs->getTotalAverageSamplesPerSecondWindow();
	return boost::str(boost::format("%1$0.2f%2% S/s") % MagnitudeReduce(spsw) % MagnitudePrefix(spsw));
}
