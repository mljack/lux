/***************************************************************************
 *   Copyright (C) 1998-2007 by authors (see AUTHORS.txt )                 *
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

/*
 * FlexImage Film class
 *
 */

#include "fleximage.h"
#include "error.h"
#include "scene.h"		// for Scene
#include "filter.h"
#include "exrio.h"
#include "igiio.h"
#include "osfunc.h"
#include "dynload.h"

#include <iostream>
#include <fstream>

#include <boost/thread/xtime.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>

using namespace boost::iostreams;
using namespace lux;

// FlexImageFilm Method Definitions
FlexImageFilm::FlexImageFilm(int xres, int yres, Filter *filt, const float crop[4],
	const string &filename1, bool premult, int wI, int dI,
	bool w_tonemapped_EXR, bool w_untonemapped_EXR, bool w_tonemapped_IGI,
	bool w_untonemapped_IGI, bool w_tonemapped_TGA, bool w_resume_FLM, bool restart_resume_FLM, int haltspp,
	int p_TonemapKernel, bool p_ReinhardAutoYwa, float p_ReinhardYwa, float p_ReinhardPreScale, float p_ReinhardPostScale,
	float p_ReinhardBurn, float p_LinearSensitivity, float p_LinearExposure, float p_LinearFStop, float p_LinearGamma,
	float p_ContrastYwa, float p_Gamma,
	const float cs_red[2], const float cs_green[2], const float cs_blue[2], const float whitepoint[2],
	int reject_warmup, bool debugmode) :
	Film(xres, yres, haltspp), filter(filt), writeInterval(wI), displayInterval(dI),
	filename(filename1), premultiplyAlpha(premult), buffersInited(false),
	writeTmExr(w_tonemapped_EXR), writeUtmExr(w_untonemapped_EXR), writeTmIgi(w_tonemapped_IGI),
	writeUtmIgi(w_untonemapped_IGI), writeTmTga(w_tonemapped_TGA), writeResumeFlm(w_resume_FLM), restartResumeFlm(restart_resume_FLM),
	framebuffer(NULL), hdrframebuffer(NULL), debug_mode(debugmode),
	colorSpace(cs_red[0], cs_red[1], cs_green[0], cs_green[1], cs_blue[0], cs_blue[1], whitepoint[0], whitepoint[1], 1.f)
{
	// Compute film image extent
	memcpy(cropWindow, crop, 4 * sizeof(float));
	xPixelStart = Ceil2Int(xResolution * cropWindow[0]);
	xPixelCount = max(1, Ceil2Int(xResolution * cropWindow[1]) - xPixelStart);
	yPixelStart = Ceil2Int(yResolution * cropWindow[2]);
	yPixelCount = max(1, Ceil2Int(yResolution * cropWindow[3]) - yPixelStart);

	// Set use and default runtime changeable parameters
	m_TonemapKernel = d_TonemapKernel = p_TonemapKernel;

	m_ReinhardAutoYwa = d_ReinhardAutoYwa = p_ReinhardAutoYwa;
	m_ReinhardYwa = d_ReinhardYwa = p_ReinhardYwa;
	m_ReinhardPreScale = d_ReinhardPreScale = p_ReinhardPreScale;
	m_ReinhardPostScale = d_ReinhardPostScale = p_ReinhardPostScale;
	m_ReinhardBurn = d_ReinhardBurn = p_ReinhardBurn;

	m_LinearSensitivity = d_LinearSensitivity = p_LinearSensitivity;
	m_LinearExposure = d_LinearExposure = p_LinearExposure;
	m_LinearFStop = d_LinearFStop = p_LinearFStop;
	m_LinearGamma = d_LinearGamma = p_LinearGamma;

	m_ContrastYwa = d_ContrastYwa = p_ContrastYwa;

	m_RGB_X_White = d_RGB_X_White = whitepoint[0];
	m_RGB_Y_White = d_RGB_Y_White = whitepoint[1];
	m_RGB_X_Red = d_RGB_X_Red = cs_red[0];
	m_RGB_Y_Red = d_RGB_Y_Red = cs_red[1];
	m_RGB_X_Green = d_RGB_X_Green = cs_green[0];
	m_RGB_Y_Green = d_RGB_Y_Green = cs_green[1];
	m_RGB_X_Blue = d_RGB_X_Blue = cs_blue[0];
	m_RGB_Y_Blue = d_RGB_Y_Blue = cs_blue[1];
	m_Gamma = d_Gamma = p_Gamma;

	// init timer
	boost::xtime_get(&lastWriteImageTime, boost::TIME_UTC);

	// calculate reject warmup samples
	reject_warmup_samples = (float) (xResolution * yResolution * reject_warmup);

	// Precompute filter weight table
	#define FILTER_TABLE_SIZE 16
	filterTable = new float[FILTER_TABLE_SIZE * FILTER_TABLE_SIZE];
	float *ftp = filterTable;
	for (int y = 0; y < FILTER_TABLE_SIZE; ++y) {
		float fy = ((float)y + .5f) * filter->yWidth / FILTER_TABLE_SIZE;
		for (int x = 0; x < FILTER_TABLE_SIZE; ++x) {
			float fx = ((float)x + .5f) * filter->xWidth / FILTER_TABLE_SIZE;
			*ftp++ = filter->Evaluate(fx, fy);
		}
	}

	maxY = 0.f;
	warmupSamples = 0;
	warmupComplete = false;
}

// Parameter Access functions
void FlexImageFilm::SetParameterValue(luxComponentParameters param, double value){
	 switch (param) {
		 case LUX_FILM_TM_TONEMAPKERNEL: {
			 m_TonemapKernel = (int) value;
			} break;

		 case LUX_FILM_TM_REINHARD_AUTOYWA: {
			 m_ReinhardAutoYwa = bool(Floor2Int(value));
			} break;
		 case LUX_FILM_TM_REINHARD_YWA: {
			 m_ReinhardYwa = float(value);
			} break;
		 case LUX_FILM_TM_REINHARD_PRESCALE: {
			 m_ReinhardPreScale = float(value);
			} break;
		 case LUX_FILM_TM_REINHARD_POSTSCALE: {
			 m_ReinhardPostScale = float(value);
			} break;
		 case LUX_FILM_TM_REINHARD_BURN: {
			 m_ReinhardBurn = float(value);
			} break;

		 case LUX_FILM_TM_LINEAR_SENSITIVITY: {
			 m_LinearSensitivity = float(value);
			} break;
		 case LUX_FILM_TM_LINEAR_EXPOSURE: {
			 m_LinearExposure = float(value);
			} break;
		 case LUX_FILM_TM_LINEAR_FSTOP: {
			 m_LinearFStop = float(value);
			} break;
		 case LUX_FILM_TM_LINEAR_GAMMA: {
			 m_LinearGamma = float(value);
			} break;

		 case LUX_FILM_TM_CONTRAST_YWA: {
			 m_ContrastYwa = float(value);
			} break;

		 case LUX_FILM_TORGB_X_WHITE: {
			 m_RGB_X_White = float(value);
			} break;
		 case LUX_FILM_TORGB_Y_WHITE: {
			 m_RGB_Y_White = float(value);
			} break;
		 case LUX_FILM_TORGB_X_RED: {
			 m_RGB_X_Red = float(value);
			} break;
		 case LUX_FILM_TORGB_Y_RED: {
			 m_RGB_Y_Red = float(value);
			} break;
		 case LUX_FILM_TORGB_X_GREEN: {
			 m_RGB_X_Green = float(value);
			} break;
		 case LUX_FILM_TORGB_Y_GREEN: {
			 m_RGB_Y_Green = float(value);
			} break;
		 case LUX_FILM_TORGB_X_BLUE: {
			 m_RGB_X_Blue = float(value);
			} break;
		 case LUX_FILM_TORGB_Y_BLUE: {
			 m_RGB_Y_Blue = float(value);
			} break;
		 case LUX_FILM_TORGB_GAMMA: {
			 m_Gamma = float(value);
			} break;

		 default: {
			} break;
	 }
}
double FlexImageFilm::GetParameterValue(luxComponentParameters param) {
	 switch (param) {
		 case LUX_FILM_TM_TONEMAPKERNEL: {
			 return float(m_TonemapKernel);
			} break;

		 case LUX_FILM_TM_REINHARD_AUTOYWA: {
			 return float(int(m_ReinhardAutoYwa));
			} break;
		 case LUX_FILM_TM_REINHARD_YWA: {
			 return m_ReinhardYwa;
			} break;
		 case LUX_FILM_TM_REINHARD_PRESCALE: {
			 return m_ReinhardPreScale;
			} break;
		 case LUX_FILM_TM_REINHARD_POSTSCALE: {
			 return m_ReinhardPostScale;
			} break;
		 case LUX_FILM_TM_REINHARD_BURN: {
			 return m_ReinhardBurn;
			} break;

		 case LUX_FILM_TM_LINEAR_SENSITIVITY: {
			 return m_LinearSensitivity;
			} break;
		 case LUX_FILM_TM_LINEAR_EXPOSURE: {
			 return m_LinearExposure;
			} break;
		 case LUX_FILM_TM_LINEAR_FSTOP: {
			 return m_LinearFStop;
			} break;
		 case LUX_FILM_TM_LINEAR_GAMMA: {
			 return m_LinearGamma;
			} break;

		 case LUX_FILM_TM_CONTRAST_YWA: {
			 return m_ContrastYwa;
			} break;

		 case LUX_FILM_TORGB_X_WHITE: {
			 return m_RGB_X_White;
			} break;
		 case LUX_FILM_TORGB_Y_WHITE: {
			 return m_RGB_Y_White;
			} break;
		 case LUX_FILM_TORGB_X_RED: {
			 return m_RGB_X_Red;
			} break;
		 case LUX_FILM_TORGB_Y_RED: {
			 return m_RGB_Y_Red;
			} break;
		 case LUX_FILM_TORGB_X_GREEN: {
			 return m_RGB_X_Green;
			} break;
		 case LUX_FILM_TORGB_Y_GREEN: {
			 return m_RGB_Y_Green;
			} break;
		 case LUX_FILM_TORGB_X_BLUE: {
			 return m_RGB_X_Blue;
			} break;
		 case LUX_FILM_TORGB_Y_BLUE: {
			 return m_RGB_Y_Blue;
			} break;
		 case LUX_FILM_TORGB_GAMMA: {
			 return m_Gamma;
			} break;

		 default: {
			} break;
	 }
	 return 0.;
}
double FlexImageFilm::GetDefaultParameterValue(luxComponentParameters param) {
	 switch (param) {
		 case LUX_FILM_TM_TONEMAPKERNEL: {
			 return float(d_TonemapKernel);
			} break;

		 case LUX_FILM_TM_REINHARD_AUTOYWA: {
			 return float(int(d_ReinhardAutoYwa));
			} break;
		 case LUX_FILM_TM_REINHARD_YWA: {
			 return d_ReinhardYwa;
			} break;
		 case LUX_FILM_TM_REINHARD_PRESCALE: {
			 return d_ReinhardPreScale;
			} break;
		 case LUX_FILM_TM_REINHARD_POSTSCALE: {
			 return d_ReinhardPostScale;
			} break;
		 case LUX_FILM_TM_REINHARD_BURN: {
			 return d_ReinhardBurn;
			} break;

		 case LUX_FILM_TM_LINEAR_SENSITIVITY: {
			 return d_LinearSensitivity;
			} break;
		 case LUX_FILM_TM_LINEAR_EXPOSURE: {
			 return d_LinearExposure;
			} break;
		 case LUX_FILM_TM_LINEAR_FSTOP: {
			 return d_LinearFStop;
			} break;
		 case LUX_FILM_TM_LINEAR_GAMMA: {
			 return d_LinearGamma;
			} break;

		 case LUX_FILM_TM_CONTRAST_YWA: {
			 return d_ContrastYwa;
			} break;

		 case LUX_FILM_TORGB_X_WHITE: {
			 return d_RGB_X_White;
			} break;
		 case LUX_FILM_TORGB_Y_WHITE: {
			 return d_RGB_Y_White;
			} break;
		 case LUX_FILM_TORGB_X_RED: {
			 return d_RGB_X_Red;
			} break;
		 case LUX_FILM_TORGB_Y_RED: {
			 return d_RGB_Y_Red;
			} break;
		 case LUX_FILM_TORGB_X_GREEN: {
			 return d_RGB_X_Green;
			} break;
		 case LUX_FILM_TORGB_Y_GREEN: {
			 return d_RGB_Y_Green;
			} break;
		 case LUX_FILM_TORGB_X_BLUE: {
			 return d_RGB_X_Blue;
			} break;
		 case LUX_FILM_TORGB_Y_BLUE: {
			 return d_RGB_Y_Blue;
			} break;
		 case LUX_FILM_TORGB_GAMMA: {
			 return d_Gamma;
			} break;

		 default: {
			} break;
	 }
	 return 0.;
}

void FlexImageFilm::GetSampleExtent(int *xstart, int *xend,
	int *ystart, int *yend) const
{
	*xstart = Floor2Int(xPixelStart + .5f - filter->xWidth);
	*xend   = Floor2Int(xPixelStart + .5f + xPixelCount + filter->xWidth);
	*ystart = Floor2Int(yPixelStart + .5f - filter->yWidth);
	*yend   = Floor2Int(yPixelStart + .5f + yPixelCount + filter->yWidth);
}

void FlexImageFilm::RequestBufferGroups(const vector<string> &bg)
{
	for (u_int i = 0; i < bg.size(); ++i)
		bufferGroups.push_back(BufferGroup(bg[i]));
}

int FlexImageFilm::RequestBuffer(BufferType type, BufferOutputConfig output,
	const string& filePostfix)
{
	bufferConfigs.push_back(BufferConfig(type, output, filePostfix));
	return bufferConfigs.size() - 1;
}

void FlexImageFilm::CreateBuffers()
{
	if (bufferGroups.size() == 0)
		bufferGroups.push_back(BufferGroup("default"));
	for (u_int i = 0; i < bufferGroups.size(); ++i)
		bufferGroups[i].CreateBuffers(bufferConfigs,xPixelCount,yPixelCount);

    // Dade - check if we have to resume a rendering and restore the buffers
    if(writeResumeFlm && !restartResumeFlm) {
        // Dade - check if the film file exists
        string fname = filename+".flm";
		std::ifstream ifs(fname.c_str(), std::ios_base::in | std::ios_base::binary);

        if(ifs.good()) {
            // Dade - read the data
            luxError(LUX_NOERROR, LUX_INFO, (std::string("Reading film status from file ")+fname).c_str());
            UpdateFilm(NULL, ifs);
        }

        ifs.close();
    }
}

vector<string> FlexImageFilm::GetGroupsName() const
{
	vector<string> names;
	for (u_int i = 0; i < bufferGroups.size(); ++i)
		names.push_back(bufferGroups[i].name);
	return names;
}
void FlexImageFilm::SetGroupEnable(u_int index, bool status)
{
	if (index >= bufferGroups.size())
		return;
	bufferGroups[index].enable = status;
}
bool FlexImageFilm::GetGroupEnable(u_int index) const
{
	if (index >= bufferGroups.size())
		return false;
	return bufferGroups[index].enable;
}
void FlexImageFilm::SetGroupScale(u_int index, float value)
{
	if (index >= bufferGroups.size())
		return;
	bufferGroups[index].scale = value;
}
float FlexImageFilm::GetGroupScale(u_int index) const
{
	if (index >= bufferGroups.size())
		return 0.f;
	return bufferGroups[index].scale;
}

void FlexImageFilm::AddSampleCount(float count) {
	for (u_int i = 0; i < bufferGroups.size(); ++i) {
		bufferGroups[i].numberOfSamples += count;

		// Dade - check if we have enough samples per pixel
		if ((haltSamplePerPixel > 0) &&
			(bufferGroups[i].numberOfSamples  >= haltSamplePerPixel * samplePerPass))
			enoughSamplePerPixel = true;
	}
}

void FlexImageFilm::AddSample(Contribution *contrib) {
	const XYZColor xyz = contrib->color;
	const float alpha = contrib->alpha;

	// Issue warning if unexpected radiance value returned
	if (xyz.IsNaN() || xyz.y() < -1e-5f || isinf(xyz.y())) {
		if(debug_mode) {
			std::stringstream ss;
			ss << "Out of bound intensity in FlexImageFilm::AddSample: "
			   << xyz.y() << ", sample discarded";
			luxError(LUX_LIMIT, LUX_WARNING, ss.str().c_str());
		}
		return;
	}

	// Reject samples higher than max y() after warmup period
	if (warmupComplete) {
		if (xyz.y() > maxY)
			return;
	} else {
		if (debug_mode) {
			maxY = INFINITY;
			warmupComplete = true;
		} else {
		 	maxY = max(maxY, xyz.y());
			++warmupSamples;
		 	if (warmupSamples >= reject_warmup_samples)
				warmupComplete = true;
		}
	}

	BufferGroup &currentGroup = bufferGroups[contrib->bufferGroup];
	Buffer *buffer = currentGroup.getBuffer(contrib->buffer);

	// Compute sample's raster extent
	float dImageX = contrib->imageX - 0.5f;
	float dImageY = contrib->imageY - 0.5f;
	int x0 = Ceil2Int (dImageX - filter->xWidth);
	int x1 = Floor2Int(dImageX + filter->xWidth);
	int y0 = Ceil2Int (dImageY - filter->yWidth);
	int y1 = Floor2Int(dImageY + filter->yWidth);
	x0 = max(x0, xPixelStart);
	x1 = min(x1, xPixelStart + xPixelCount - 1);
	y0 = max(y0, yPixelStart);
	y1 = min(y1, yPixelStart + yPixelCount - 1);
	if (x1 < x0 || y1 < y0) return;
	// Loop over filter support and add sample to pixel arrays
	// Precompute $x$ and $y$ filter table offsets
//	int *ifx = (int *)alloca((x1-x0+1) * sizeof(int));				// TODO - radiance - pre alloc memory in constructor for speedup ?
	int ifx[32];
	for (int x = x0; x <= x1; ++x) {
		float fx = fabsf((x - dImageX) *
			filter->invXWidth * FILTER_TABLE_SIZE);
		ifx[x-x0] = min(Floor2Int(fx), FILTER_TABLE_SIZE-1);
	}
//	int *ify = (int *)alloca((y1-y0+1) * sizeof(int));				// TODO - radiance - pre alloc memory in constructor for speedup ?
	int ify[32];
	for (int y = y0; y <= y1; ++y) {
		float fy = fabsf((y - dImageY) *
			filter->invYWidth * FILTER_TABLE_SIZE);
		ify[y-y0] = min(Floor2Int(fy), FILTER_TABLE_SIZE-1);
	}

	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			// Evaluate filter value at $(x,y)$ pixel
			int offset = ify[y-y0]*FILTER_TABLE_SIZE + ifx[x-x0];
			float filterWt = filterTable[offset];
			// Update pixel values with filtered sample contribution
			buffer->Add(x - xPixelStart,y - yPixelStart,
				xyz, alpha, filterWt);
		}
	}

	// Check write output interval
	boost::xtime currentTime;
	boost::xtime_get(&currentTime, boost::TIME_UTC);
	bool timeToWriteImage = (currentTime.sec - lastWriteImageTime.sec > writeInterval);

	// Possibly write out in-progress image
	if (timeToWriteImage) {
		boost::xtime_get(&lastWriteImageTime, boost::TIME_UTC);
		WriteImage(IMAGE_FILEOUTPUT);
	}
}

void FlexImageFilm::WriteImage2(ImageType type, vector<Color> &color, vector<float> &alpha, string postfix)
{
	if (type & IMAGE_FILEOUTPUT) {
		// write out untonemapped EXR
		if (writeUtmExr) {
			// convert to rgb
			const u_int nPix = xPixelCount * yPixelCount;
			vector<Color> rgbColor(nPix);
			for ( u_int i = 0; i < nPix; i++ )
				rgbColor[i] = colorSpace.ToRGBConstrained( XYZColor(color[i].c) );

			WriteEXRImage(rgbColor, alpha, filename + postfix + "_untonemapped.exr");
		}
		// write out untonemapped IGI
		if (writeUtmIgi)
			WriteIGIImage(color, alpha, filename + postfix + "_untonemapped.igi"); // TODO add samples

		// Dade - save the current status of the film if required
		if (writeResumeFlm)
			WriteResumeFilm(filename + ".flm");
	}

	if ((type & IMAGE_HDRFRAMEBUFFER) && hdrframebuffer) {
		// Copy to framebuffer pixels
		const u_int nPix = xPixelCount * yPixelCount;
		for (u_int i = 0; i < nPix; i++) {
			hdrframebuffer[3 * i] = color[i].c[0];
			hdrframebuffer[3 * i + 1] = color[i].c[1];
			hdrframebuffer[3 * i + 2] = color[i].c[2];
		}
	}

	// Dade - check if I have to run ApplyImagingPipeline
	if (((type & IMAGE_FRAMEBUFFER) && framebuffer) ||
		((type & IMAGE_FILEOUTPUT) && (writeTmExr || writeTmIgi || writeTmTga))) {
		// Construct ColorSystem from values
		colorSpace = ColorSystem(m_RGB_X_Red, m_RGB_Y_Red,
			m_RGB_X_Green, m_RGB_Y_Green,
			m_RGB_X_Blue, m_RGB_Y_Blue,
			m_RGB_X_White, m_RGB_Y_White, 1.f);

		// Apply the imaging/tonemapping pipeline
		ParamSet toneParams;
		if(m_TonemapKernel == 0) {
			// Reinhard Tonemapper
			toneParams.AddBool("autoywa", &m_ReinhardAutoYwa, 1);
			toneParams.AddFloat("ywa", &m_ReinhardYwa, 1);
			toneParams.AddFloat("prescale", &m_ReinhardPreScale, 1);
			toneParams.AddFloat("postscale", &m_ReinhardPostScale, 1);
			toneParams.AddFloat("burn", &m_ReinhardBurn, 1);
			ApplyImagingPipeline(color, xPixelCount, yPixelCount, colorSpace,
				0., 0., "reinhard", &toneParams, m_Gamma, 0.);
		} else if(m_TonemapKernel == 1) {
			// Linear Tonemapper
			toneParams.AddFloat("sensitivity", &m_LinearSensitivity, 1);
			toneParams.AddFloat("exposure", &m_LinearExposure, 1);
			toneParams.AddFloat("fstop", &m_LinearFStop, 1);
			toneParams.AddFloat("gamma", &m_LinearGamma, 1);
			ApplyImagingPipeline(color, xPixelCount, yPixelCount, colorSpace,
				0., 0., "linear", &toneParams, m_Gamma, 0.);
		} else if(m_TonemapKernel == 2) {
			// Contrast Tonemapper
			toneParams.AddFloat("ywa", &m_ContrastYwa, 1);
			ApplyImagingPipeline(color, xPixelCount, yPixelCount, colorSpace,
				0., 0., "contrast", &toneParams, m_Gamma, 0.);
		} else {
			// MaxWhite Tonemapper
			ApplyImagingPipeline(color, xPixelCount, yPixelCount, colorSpace,
				0., 0., "maxwhite", &toneParams, m_Gamma, 0.);
		}

		if ((type & IMAGE_FRAMEBUFFER) && framebuffer) {
			// Copy to framebuffer pixels
			const u_int nPix = xPixelCount * yPixelCount;
			for (u_int i = 0; i < nPix; i++) {
				framebuffer[3 * i] = static_cast<unsigned char>(Clamp(256 * color[i].c[0], 0.f, 255.f));
				framebuffer[3 * i + 1] = static_cast<unsigned char>(Clamp(256 * color[i].c[1], 0.f, 255.f));
				framebuffer[3 * i + 2] = static_cast<unsigned char>(Clamp(256 * color[i].c[2], 0.f, 255.f));
			}
		}

		if (type & IMAGE_FILEOUTPUT) {
			// write out tonemapped TGA
			if (writeTmTga)
				WriteTGAImage(color, alpha, filename + postfix + ".tga");

			// write out tonemapped EXR
			if (writeTmExr)
				WriteEXRImage(color, alpha, filename + postfix + ".exr");

			// write out tonemapped IGI
			if (writeTmIgi) //TODO color should be XYZ values
				WriteIGIImage(color, alpha, filename + postfix + ".igi"); // TODO add samples
		}

	}
}

void FlexImageFilm::ScaleOutput(vector<Color> &pixel, vector<float> &alpha, float *scale)
{
	int offset = 0;
	for (int y = 0; y < yPixelCount; ++y) {
		for (int x = 0; x < xPixelCount; ++x,++offset) {
			pixel[offset] *= scale[offset];
			alpha[offset] *= scale[offset];
		}
	}
}
void FlexImageFilm::WriteImage(ImageType type)
{
	const int nPix = xPixelCount * yPixelCount;
	vector<Color> pixels(nPix), pixels0(nPix);
	vector<float> alpha(nPix), alpha0(nPix);

	float Y = 0.f;
	// in order to fix bug #360
	// ouside loop not to trash the complete picture
	// if there are several buffer groups
	fill(pixels0.begin(), pixels0.end(), XYZColor(0.f));
	for(u_int j = 0; j < bufferGroups.size(); ++j) {

		for(u_int i = 0; i < bufferConfigs.size(); ++i) {
			const Buffer &buffer = *(bufferGroups[j].buffers[i]);
			for (int offset = 0, y = 0; y < yPixelCount; ++y) {
				for (int x = 0; x < xPixelCount; ++x,++offset) {
					buffer.GetData(x, y, &(pixels[offset]), &(alpha[offset]));
					if (bufferConfigs[i].output & BUF_FRAMEBUFFER && bufferGroups[j].enable) {
						pixels0[offset] += pixels[offset] * bufferGroups[j].scale;
						alpha0[offset] += alpha[offset];
					}
				}
			}
			if (bufferConfigs[i].output & BUF_STANDALONE) {
				WriteImage2(type, pixels, alpha, bufferConfigs[i].postfix);
			}
		}
	}
	// outside loop in order to write complete image
	for (int pix = 0; pix < nPix; ++pix)
		Y += pixels0[pix].c[1];
	Y /= nPix;
	WriteImage2(type, pixels0, alpha0, "");
	EV = logf(Y * 10.f) / logf(2.f);
}

// GUI LDR framebuffer access methods
void FlexImageFilm::createFrameBuffer()
{
	// allocate pixels
	unsigned int nPix = xPixelCount * yPixelCount;
	framebuffer = new unsigned char[3*nPix];			// TODO delete data

	// zero it out
	memset(framebuffer,0,sizeof(*framebuffer)*3*nPix);
}
void FlexImageFilm::updateFrameBuffer()
{
	if(!framebuffer) {
		createFrameBuffer();
	}

	WriteImage(IMAGE_FRAMEBUFFER);
}
unsigned char* FlexImageFilm::getFrameBuffer()
{
	if(!framebuffer)
		createFrameBuffer();

	return framebuffer;
}

// GUI HDR (for OpenGL acceleration) framebuffer access methods
void FlexImageFilm::createHDRFrameBuffer()
{
	// allocate pixels
	unsigned int nPix = xPixelCount * yPixelCount;
	hdrframebuffer = new float[3*nPix];			// TODO delete data

	// zero it out
	memset(hdrframebuffer,0.,sizeof(*hdrframebuffer)*3*nPix);    
}
void FlexImageFilm::updateHDRFrameBuffer()
{
	if(!hdrframebuffer)
		createHDRFrameBuffer();

	WriteImage(IMAGE_HDRFRAMEBUFFER);
}
float* FlexImageFilm::getHDRFrameBuffer()
{
	if(!hdrframebuffer)
		createHDRFrameBuffer();

	return hdrframebuffer;
}


void FlexImageFilm::WriteResumeFilm(const string &filename)
{
	// Dade - save the status of the film to the file
	luxError(LUX_NOERROR, LUX_INFO, (std::string("Writing film status to file ") +
			filename).c_str());

    std::ofstream filestr(filename.c_str(), std::ios_base::out | std::ios_base::binary);
	if(!filestr) {
		std::stringstream ss;
	 	ss << "Cannot open file '" << filename << "' for writing resume film";
		luxError(LUX_SYSTEM, LUX_SEVERE, ss.str().c_str());

		return;
	}

    TransmitFilm(filestr,false);

    filestr.close();
}

void FlexImageFilm::WriteTGAImage(vector<Color> &rgb, vector<float> &alpha, const string &filename)
{
	luxError(LUX_NOERROR, LUX_INFO, (std::string("Writing Tonemapped TGA image to file ")+filename).c_str());

	// Open file
	FILE* tgaFile = fopen(filename.c_str(),"wb");
	if (!tgaFile) {
		std::stringstream ss;
		ss<< "Cannot open file '"<<filename<<"' for output";
		luxError(LUX_SYSTEM, LUX_SEVERE, ss.str().c_str());
		return;
	}

	// write the header
	// make sure its platform independent of little endian and big endian
	char header[18];
	memset(header, 0, sizeof(char) * 18);

	header[2] = 2;							// set the data type of the targa (2 = uncompressed)
	short xResShort = xResolution;			// set the resolution and make sure the bytes are in the right order
	header[13] = (char) (xResShort >> 8);
	header[12] = xResShort & 0xFF;
	short yResShort = yResolution;
	header[15] = (char) (yResShort >> 8);
	header[14] = yResShort & 0xFF;
	header[16] = 32;						// set the bitdepth

	// put the header data into the file
	for (int i = 0; i < 18; ++i)
		fputc(header[i], tgaFile);

	// write the bytes of data out
	for (int i = yPixelCount - 1;  i >= 0 ; --i) {
		for (int j = 0; j < xPixelCount; ++j) {
			fputc(static_cast<unsigned char>(Clamp(256 * rgb[i * xPixelCount + j].c[2], 0.f, 255.f)), tgaFile);
			fputc(static_cast<unsigned char>(Clamp(256 * rgb[i * xPixelCount + j].c[1], 0.f, 255.f)), tgaFile);
			fputc(static_cast<unsigned char>(Clamp(256 * rgb[i * xPixelCount + j].c[0], 0.f, 255.f)), tgaFile);
			// NOTE - radiance - removed alpha output in TGA files due to errors
			fputc(255, tgaFile);
			//fputc((int) (255.0*alpha[(i*xPixelCount+j)]), tgaFile);
		}
	}

	fclose(tgaFile);
}

void FlexImageFilm::WriteEXRImage(vector<Color> &rgb, vector<float> &alpha, const string &filename)
{
	// Write OpenEXR RGBA image
	luxError(LUX_NOERROR, LUX_INFO, (std::string("Writing OpenEXR image to file ")+filename).c_str());
	WriteRGBAImageFloat(filename, rgb, alpha,
		xPixelCount, yPixelCount,
		xResolution, yResolution,
		xPixelStart, yPixelStart);
}

void FlexImageFilm::WriteIGIImage(vector<Color> &rgb, vector<float> &alpha, const string &filename)
{
	// Write IGI image
	luxError(LUX_NOERROR, LUX_INFO, (std::string("Writing IGI image to file ")+filename).c_str());
	WriteIgiImage(filename, rgb, alpha,
		xPixelCount, yPixelCount,
		xResolution, yResolution,
		xPixelStart, yPixelStart);
}

void FlexImageFilm::TransmitFilm(
        std::basic_ostream<char> &stream,
        bool clearBuffers) 
{
    bool isLittleEndian = osIsLittleEndian();

    std::stringstream ss;
    ss << "Transmitting film (little endian=" <<(isLittleEndian ? "true" : "false") << ")";
    luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

    std::stringstream os;
	// Write #buffer groups and buffer configs for verification
	osWriteLittleEndianInt(isLittleEndian, os, (int)bufferGroups.size());
	osWriteLittleEndianInt(isLittleEndian, os, (int)bufferConfigs.size());
	for (u_int i = 0; i < bufferConfigs.size(); i++)
		osWriteLittleEndianInt(isLittleEndian, os, (int)bufferConfigs[i].type);
	// Write each buffer group
	for (u_int i = 0; i < bufferGroups.size(); i++) {
		BufferGroup& bufferGroup = bufferGroups[i];
		// Write number of samples
		osWriteLittleEndianFloat(isLittleEndian, os, bufferGroup.numberOfSamples);

		// Write each buffer
		for (u_int j = 0; j < bufferConfigs.size(); j++) {
			Buffer* buffer = bufferGroup.getBuffer(j);

			// Write buffer width/height
			osWriteLittleEndianInt(isLittleEndian, os, buffer->xPixelCount);
			osWriteLittleEndianInt(isLittleEndian, os, buffer->yPixelCount);

			// Write pixels
			const BlockedArray<Pixel>* pixelBuf = buffer->pixels;
			for (int y = 0; y < pixelBuf->vSize(); ++y) {
				for (int x = 0; x < pixelBuf->uSize(); ++x) {
					const Pixel &pixel = (*pixelBuf)(x, y);
					osWriteLittleEndianFloat(isLittleEndian, os, pixel.L.c[0]);
					osWriteLittleEndianFloat(isLittleEndian, os, pixel.L.c[1]);
					osWriteLittleEndianFloat(isLittleEndian, os, pixel.L.c[2]);
					osWriteLittleEndianFloat(isLittleEndian, os, pixel.alpha);
					osWriteLittleEndianFloat(isLittleEndian, os, pixel.weightSum);
				}
			}

			if(clearBuffers) {
				// Dade - reset the rendering buffer
				buffer->Clear();
			}
		}

		if(clearBuffers) {
			// Dade - reset the rendering buffer
			bufferGroup.numberOfSamples = 0;
		}
	}

	if(!os.good()) {
		luxError(LUX_SYSTEM, LUX_SEVERE, "Error while preparing film data for transmission");
		return;
	}

    filtering_streambuf<input> in;
    in.push(gzip_compressor(9));
    in.push(os);
    std::streamsize size = boost::iostreams::copy(in, stream);
	if(!stream.good()) {
		luxError(LUX_SYSTEM, LUX_SEVERE, "Error while transmitting film");
		return;
	}

    ss.str("");
    ss << "Film transmission done (" << (size / 1024.0f) << " Kbytes sent)";
    luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());
}

float FlexImageFilm::UpdateFilm(Scene *scene, std::basic_istream<char> &stream) {
	bool isLittleEndian = osIsLittleEndian();

    filtering_stream<input> in;
    in.push(gzip_decompressor());
    in.push(stream);

	std::stringstream ss;
	ss << "Receiving film (little endian=" << (isLittleEndian ? "true" : "false") << ")";
	luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

	// Read and verify #buffer groups and buffer configs
    int nBufferGroups;
    osReadLittleEndianInt(isLittleEndian, in, &nBufferGroups);
	if (!in.good()) {
		luxError(LUX_SYSTEM, LUX_ERROR, "Error while receiving film");
		return 0.0f;
	}
	else if(nBufferGroups != (int)bufferGroups.size()) {
		ss.str("");
		ss << "Invalid number of buffer groups (expected=" << (int)bufferGroups.size() 
			<< ",received=" << nBufferGroups << ")";
		luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());
		return 0.0f;
	}
	int nBuffers;
	osReadLittleEndianInt(isLittleEndian, in, &nBuffers);
	if (!in.good()) {
		luxError(LUX_SYSTEM, LUX_ERROR, "Error while receiving film");
		return 0.0f;
	}
	else if(nBuffers != (int)bufferConfigs.size()) {
		ss.str("");
		ss << "Invalid number of buffers (expected=" << (int)bufferConfigs.size() 
			<< ",received=" << nBuffers << ")";
		luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());
		return 0.0f;
	}
	for( u_int i = 0; i < bufferConfigs.size(); i++) {
		int type;
		osReadLittleEndianInt(isLittleEndian, in, &type);
		if (!in.good()) {
			luxError(LUX_SYSTEM, LUX_ERROR, "Error while receiving film");
			return 0.0f;
		}
		else if(type != (int)bufferConfigs[i].type) {
			ss.str("");
			ss << "Invalid buffer type for buffer " << i << " (expected=" << (int)bufferConfigs[i].type
				<< ",received=" << type << ")";
			luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());
			return 0.0f;
		}
	}

	// Read buffer groups
	bool err = false; // flag for data errors
	vector<float> bufferGroupNumSamples(bufferGroups.size());
	vector<BlockedArray<Pixel>*> tmpPixelArrays(bufferGroups.size() * bufferConfigs.size());
	for(u_int i = 0; i < bufferGroups.size(); i++) {
		float numberOfSamples;
		osReadLittleEndianFloat(isLittleEndian, in, &numberOfSamples);
		if(!in.good() || err) break;
		bufferGroupNumSamples[i] = numberOfSamples;

		// Read buffers
		for(u_int j = 0; j < bufferConfigs.size(); j++) {
			const Buffer* localBuffer = bufferGroups[i].getBuffer(j);

			int xRes, yRes;
			osReadLittleEndianInt(isLittleEndian, in, &xRes);
			osReadLittleEndianInt(isLittleEndian, in, &yRes);
			if(!in.good() || err) break;
			if(xRes != localBuffer->xPixelCount || yRes != localBuffer->yPixelCount) {
				ss.str("");
				ss << "Invalid resolution for buffer " << j << " in group " << i;
				ss << " (expected=" << localBuffer->xPixelCount << "x" << localBuffer->yPixelCount;
				ss << ",received=" << xRes << "x" << yRes << ")";
				luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());
				err = true;
				break;
			}

			// Read pixels
			BlockedArray<Pixel> *tmpPixelArr = new BlockedArray<Pixel>(
				localBuffer->xPixelCount, localBuffer->yPixelCount);
			tmpPixelArrays[i*bufferConfigs.size() + j] = tmpPixelArr;
			for (int y = 0; y < tmpPixelArr->vSize(); ++y) {
				for (int x = 0; x < tmpPixelArr->uSize(); ++x) {
					Pixel &pixel = (*tmpPixelArr)(x, y);
					osReadLittleEndianFloat(isLittleEndian, in, &pixel.L.c[0]);
					osReadLittleEndianFloat(isLittleEndian, in, &pixel.L.c[1]);
					osReadLittleEndianFloat(isLittleEndian, in, &pixel.L.c[2]);
					osReadLittleEndianFloat(isLittleEndian, in, &pixel.alpha);
					osReadLittleEndianFloat(isLittleEndian, in, &pixel.weightSum);
				}
			}
			if(!in.good() || err) break;
		}
		if(!in.good() || err) break;
	}

    // Dade - check for errors
	float totNumberOfSamples = 0.f;
    if (in.good() && !err) {
        // Dade - add all received data
		for(u_int i = 0; i < bufferGroups.size(); i++) {
			BufferGroup &currentGroup = bufferGroups[i];
			for(u_int j = 0; j < bufferConfigs.size(); j++) {
				const BlockedArray<Pixel> *receivedPixels = tmpPixelArrays[ i * bufferConfigs.size() + j ];
				Buffer *buffer = currentGroup.getBuffer(j);

				for (int y = 0; y < buffer->yPixelCount; ++y) {
					for (int x = 0; x < buffer->xPixelCount; ++x) {
						const Pixel &pixel = (*receivedPixels)(x, y);
						Pixel &pixelResult = (*buffer->pixels)(x, y);

						pixelResult.L.c[0] += pixel.L.c[0];
						pixelResult.L.c[1] += pixel.L.c[1];
						pixelResult.L.c[2] += pixel.L.c[2];
						pixelResult.alpha += pixel.alpha;
						pixelResult.weightSum += pixel.weightSum;
					}
				}
			}

			currentGroup.numberOfSamples += bufferGroupNumSamples[i];
			totNumberOfSamples += bufferGroupNumSamples[i];
		}

        if(scene != NULL)
            scene->numberOfSamplesFromNetwork += totNumberOfSamples;

		ss.str("");
		ss << "Received film with " << totNumberOfSamples << " samples";
		luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());
    } else if( err ) {
        luxError(LUX_SYSTEM, LUX_ERROR, "Data error while receiving film buffers");
    } else if( !in.good() ) {
        luxError(LUX_SYSTEM, LUX_ERROR, "IO error while receiving film buffers");
    }

	// Clean up
	for(u_int i = 0; i < tmpPixelArrays.size(); i++) {
		if( tmpPixelArrays[i] )
			delete tmpPixelArrays[i];
	}

	return totNumberOfSamples;
}

void FlexImageFilm::GetColorspaceParam(const ParamSet &params, const string name, float values[2]) {
	int i;
	const float *v = params.FindFloat(name, &i);
	if (v && i == 2) {
		values[0] = v[0];
		values[1] = v[1];
	}
}

// params / creation
Film* FlexImageFilm::CreateFilm(const ParamSet &params, Filter *filter)
{
	// General
	bool premultiplyAlpha = params.FindOneBool("premultiplyalpha", true);

	int xres = params.FindOneInt("xresolution", 800);
	int yres = params.FindOneInt("yresolution", 600);

	float crop[4] = { 0, 1, 0, 1 };
	int cwi;
	const float *cr = params.FindFloat("cropwindow", &cwi);
	if (cr && cwi == 4) {
		crop[0] = Clamp(min(cr[0], cr[1]), 0.f, 1.f);
		crop[1] = Clamp(max(cr[0], cr[1]), 0.f, 1.f);
		crop[2] = Clamp(min(cr[2], cr[3]), 0.f, 1.f);
		crop[3] = Clamp(max(cr[2], cr[3]), 0.f, 1.f);
	}

	// output fileformats
	bool w_tonemapped_EXR = params.FindOneBool("write_tonemapped_exr", true);
	bool w_untonemapped_EXR = params.FindOneBool("write_untonemapped_exr", false);
	bool w_tonemapped_IGI = params.FindOneBool("write_tonemapped_igi", false);
	bool w_untonemapped_IGI = params.FindOneBool("write_untonemapped_igi", false);
	bool w_tonemapped_TGA = params.FindOneBool("write_tonemapped_tga", false);

    bool w_resume_FLM = params.FindOneBool("write_resume_flm", false);
	bool restart_resume_FLM = params.FindOneBool("restart_resume_flm", false);

	// output filenames
	string filename = params.FindOneString("filename", "luxout");

	// intervals
	int writeInterval = params.FindOneInt("writeinterval", 60);
	int displayInterval = params.FindOneInt("displayinterval", 12);

	// Rejection mechanism
	int reject_warmup = params.FindOneInt("reject_warmup", 64); // minimum samples/px before rejecting

	// Debugging mode (display erratic sample values and disable rejection mechanism)
	bool debug_mode = params.FindOneBool("debug", false);

	int haltspp = params.FindOneInt("haltspp", -1);

	// Color space primaries and white point
	// default is SMPTE
	float red[2] = {0.63f, 0.34f};
	GetColorspaceParam(params, "colorspace_red", red);

	float green[2] = {0.31f, 0.595f};
	GetColorspaceParam(params, "colorspace_green", green);

	float blue[2] = {0.155f, 0.07f};
	GetColorspaceParam(params, "colorspace_blue", blue);

	float white[2] = {0.314275f, 0.329411f};
	GetColorspaceParam(params, "colorspace_white", white);

	// Tonemapping
	int s_TonemapKernel = 0;
	string tmkernelStr = params.FindOneString("tonemapkernel", "reinhard");
	if (tmkernelStr == "reinhard") s_TonemapKernel = 0;
	else if (tmkernelStr == "linear") s_TonemapKernel = 1;
	else if (tmkernelStr == "contrast") s_TonemapKernel = 2;
	else if (tmkernelStr == "maxwhite") s_TonemapKernel = 3;
	else {
		std::stringstream ss;
		ss << "Tonemap kernel  '" << tmkernelStr << "' unknown. Using \"reinhard\".";
		luxError(LUX_BADTOKEN,LUX_WARNING,ss.str().c_str());
		s_TonemapKernel = 0;
	}

	bool s_ReinhardAutoYwa = params.FindOneBool("reinhard_autoywa", true);
	float s_ReinhardYwa = params.FindOneFloat("reinhard_ywa", 1.f);
	float s_ReinhardPreScale = params.FindOneFloat("reinhard_prescale", 1.f);
	float s_ReinhardPostScale = params.FindOneFloat("reinhard_postscale", 1.f);
	float s_ReinhardBurn = params.FindOneFloat("reinhard_burn", 6.f);
	float s_LinearSensitivity = params.FindOneFloat("linear_sensitivity", 100.f);
	float s_LinearExposure = params.FindOneFloat("linear_exposure", 1.f/1000.f);
	float s_LinearFStop = params.FindOneFloat("linear_fstop", 2.8f);
	float s_LinearGamma = params.FindOneFloat("linear_gamma", 1.0f);
	float s_ContrastYwa = params.FindOneFloat("contrast_ywa", 100.f);
	float s_Gamma = params.FindOneFloat("gamma", 2.2f);

	return new FlexImageFilm(xres, yres, filter, crop,
		filename, premultiplyAlpha, writeInterval, displayInterval,
		w_tonemapped_EXR, w_untonemapped_EXR, w_tonemapped_IGI, w_untonemapped_IGI, w_tonemapped_TGA, w_resume_FLM, restart_resume_FLM, haltspp,

		s_TonemapKernel, s_ReinhardAutoYwa, s_ReinhardYwa, s_ReinhardPreScale, s_ReinhardPostScale, s_ReinhardBurn, s_LinearSensitivity,
		s_LinearExposure, s_LinearFStop, s_LinearGamma, s_ContrastYwa, s_Gamma,

		red, green, blue, white, reject_warmup, debug_mode);
}

static DynamicLoader::RegisterFilm<FlexImageFilm> r1("fleximage");
static DynamicLoader::RegisterFilm<FlexImageFilm> r2("multiimage");
