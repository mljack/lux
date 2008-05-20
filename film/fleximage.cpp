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
#include "camera.h"		// for Camera
#include "transport.h"	// for SurfaceIntegrator::IsFluxBased()
#include "filter.h"

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
	bool w_untonemapped_IGI, bool w_tonemapped_TGA, bool w_resume_FLM,
	float reinhard_prescale, float reinhard_postscale, float reinhard_burn, float g, int reject_warmup, bool debugmode ) :
	Film(xres, yres), filter(filt), writeInterval(wI), displayInterval(dI), filename(filename1),
	premultiplyAlpha(premult), buffersInited(false), gamma(g),
	writeTmExr(w_tonemapped_EXR), writeUtmExr(w_untonemapped_EXR), writeTmIgi(w_tonemapped_IGI),
	writeUtmIgi(w_untonemapped_IGI), writeTmTga(w_tonemapped_TGA), writeResumeFlm(w_resume_FLM),
    framebuffer(NULL), imageLock(false), debug_mode(debugmode), factor(NULL)
{
	// Compute film image extent
	memcpy(cropWindow, crop, 4 * sizeof(float));
	xPixelStart = Ceil2Int(xResolution * cropWindow[0]);
	xPixelCount = max(1, Ceil2Int(xResolution * cropWindow[1]) - xPixelStart);
	yPixelStart = Ceil2Int(yResolution * cropWindow[2]);
	yPixelCount = max(1, Ceil2Int(yResolution * cropWindow[3]) - yPixelStart);

	// Set tonemapper params
	toneParams.AddFloat("prescale", &reinhard_prescale, 1);
	toneParams.AddFloat("postscale", &reinhard_postscale, 1);
	toneParams.AddFloat("burn", &reinhard_burn, 1);

	// init timer
	timer.restart();

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

	// array allocation
    // Dade - 2048 was a too small value for QuadCores, there was a memory fault
    // because the second buffer was filled while we were still merging the first
    // one. Bug #140.

	int arrsize = 128*1024;
	SampleArrptr = new ArrSample[arrsize];
	SampleArr2ptr = new ArrSample[arrsize];

	curSampleArrId = 0;
	maxSampleArrId = arrsize-1;

	maxY = 0.f;
	warmupSamples = 0;
	warmupComplete = false;
}

void FlexImageFilm::GetSampleExtent(int *xstart, int *xend,
	int *ystart, int *yend) const
{
	*xstart = Floor2Int(xPixelStart + .5f - filter->xWidth);
	*xend   = Floor2Int(xPixelStart + .5f + xPixelCount + filter->xWidth);
	*ystart = Floor2Int(yPixelStart + .5f - filter->yWidth);
	*yend   = Floor2Int(yPixelStart + .5f + yPixelCount + filter->yWidth);
}

int FlexImageFilm::RequestBuffer(BufferType type, BufferOutputConfig output,
	const string& filePostfix)
{
	bufferConfigs.push_back(BufferConfig(type, output, filePostfix));
	return bufferConfigs.size() - 1;
}

void FlexImageFilm::CreateBuffers()
{
	// TODO: more groups for multilight
	bufferGroups.push_back(BufferGroup());
	bufferGroups.back().CreateBuffers(bufferConfigs,xPixelCount,yPixelCount);

    // Dade - check if we have to resume a rendering and restore the buffers
    if(writeResumeFlm) {
        // Dade - check if the film file exists

        string fname = filename+".flm";
        std::ifstream ifs(fname.c_str());

        if(ifs.good()) {
            // Dade - read the data

            luxError(LUX_NOERROR, LUX_INFO, (std::string("Reading film status from file ")+fname).c_str());
            
            UpdateFilm(NULL, ifs, 0, 0);
        }

        ifs.close();
    }
}

void FlexImageFilm::AddSample(float sX, float sY, const XYZColor &xyz, float alpha, int buf_id, int bufferGroup)
{
	bool SampleArrmerge = false;
	int arr_id;
	// retrieve array position and increment for next AddSample()
	{
		boost::mutex::scoped_lock lock(addSampleMutex);

		// fetch arr_id and increment
		arr_id = curSampleArrId;
		curSampleArrId++;

		// check for end
		if(curSampleArrId == maxSampleArrId) {
			// swap SampleArrptrs
			ArrSample *tmpptr = SampleArrptr; 
			SampleArrptr = SampleArr2ptr; SampleArr2ptr = tmpptr;
			// enable merge and reset counter
			SampleArrmerge = true;
			curSampleArrId = 0;
		}
	}

	// Copy to array location
	SampleArrptr[arr_id].sX = sX;
	SampleArrptr[arr_id].sY = sY;
	SampleArrptr[arr_id].xyz = xyz;
	SampleArrptr[arr_id].alpha = alpha;
	SampleArrptr[arr_id].buf_id = buf_id;
	SampleArrptr[arr_id].bufferGroup = bufferGroup;

	if(SampleArrmerge)
		MergeSampleArray();
}

void FlexImageFilm::MergeSampleArray() {

    // TODO: Find a group
    if (bufferGroups.empty())
    {
        RequestBuffer(BUF_TYPE_PER_SCREEN, BUF_FRAMEBUFFER, "");
        CreateBuffers();
    }

	for(int sArrId = 0; sArrId < maxSampleArrId-1; sArrId++) {
		float sX = SampleArr2ptr[sArrId].sX;
		float sY = SampleArr2ptr[sArrId].sY;
		XYZColor xyz = SampleArr2ptr[sArrId].xyz;
		float alpha = SampleArr2ptr[sArrId].alpha;
		int buf_id = SampleArr2ptr[sArrId].buf_id;
		int bufferGroup = SampleArr2ptr[sArrId].bufferGroup;

		// Issue warning if unexpected radiance value returned
		if (xyz.IsNaN() || xyz.y() < -1e-5f || isinf(xyz.y())) {
			if(debug_mode) {
				std::stringstream ss;
				ss << "Out of bound intensity in FlexImageFilm::AddSample: "
				   << xyz.y() << ", sample discarded";
				luxError(LUX_LIMIT, LUX_WARNING, ss.str().c_str());
			}
			continue;
		}

		// Reject samples higher than max y() after warmup period
		if(warmupComplete) {
			if(xyz.y() > maxY)
				continue;
		} else {
			if(debug_mode) {
				maxY = INFINITY;
				warmupComplete = true;
			}
			 else if(warmupSamples < reject_warmup_samples) {
				if(xyz.y() > maxY)
					maxY = xyz.y();
				warmupSamples++;
			} else
				warmupComplete = true;
		}

		BufferGroup &currentGroup = bufferGroups[bufferGroup];
		Buffer *buffer = currentGroup.getBuffer(buf_id);

		// Compute sample's raster extent
		float dImageX = sX - 0.5f;
		float dImageY = sY - 0.5f;
		int x0 = Ceil2Int (dImageX - filter->xWidth);
		int x1 = Floor2Int(dImageX + filter->xWidth);
		int y0 = Ceil2Int (dImageY - filter->yWidth);
		int y1 = Floor2Int(dImageY + filter->yWidth);
		x0 = max(x0, xPixelStart);
		x1 = min(x1, xPixelStart + xPixelCount - 1);
		y0 = max(y0, yPixelStart);
		y1 = min(y1, yPixelStart + yPixelCount - 1);
		if (x1 < x0 || y1 < y0) continue;
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

	}

	// Possibly write out in-progress image
	if( !imageLock && Floor2Int(timer.elapsed()) > writeInterval)
	{
		imageLock = true;
		WriteImage((ImageType)(IMAGE_FILEOUTPUT));
		timer.restart();
		imageLock = false;
	}
}

void FlexImageFilm::WriteImage2(ImageType type, float* rgb, float* alpha, string postfix)
{
	if (type & IMAGE_FRAMEBUFFER && framebuffer) {
		// Apply the imagine/tonemapping pipeline
		ApplyImagingPipeline(rgb,xPixelCount,yPixelCount,NULL,
			0.,0.,"reinhard",
			&toneParams,gamma,0.,255);

		// Copy to framebuffer pixels
			u_int nPix = xPixelCount * yPixelCount;
			for (u_int i=0;  i < nPix*3 ; i++)
				framebuffer[i] = (unsigned char) rgb[i];

	} else if(type & IMAGE_FILEOUTPUT) {
		// write out untonemapped EXR
		if(writeUtmExr)
			WriteEXRImage(rgb, alpha, filename+postfix+"_untonemapped.exr");
		// write out untonemapped IGI
		if(writeUtmIgi)
			WriteIGIImage(rgb, alpha, filename+postfix+"_untonemapped.igi"); // TODO add samples

        // Dade - save the current status of the film if required
        if(writeResumeFlm)
            WriteResumeFilm(filename + ".flm");

		if(writeTmExr || writeTmIgi || writeTmTga) {
			// Apply the imagine/tonemapping pipeline
			ApplyImagingPipeline(rgb,xPixelCount,yPixelCount,NULL,
				0.,0.,"reinhard",
				&toneParams,gamma,0.,255);

			// write out tonemapped TGA
			if(writeTmTga)
				WriteTGAImage(rgb, alpha, filename+postfix+".tga");

			if(writeTmExr || writeTmIgi) {
				// Map display range 0.-255. back to float range 0.-1.
				// and reverse the gamma correction
				// necessary for EXR/IGI formats
				int nPix = xResolution * yResolution ;
				for (int i = 0; i < 3*nPix; ++i) {
					rgb[i] /= 255;
					if (gamma != 1.f)
						rgb[i] = powf(rgb[i], gamma);
				}

				// write out tonemapped EXR
				if(writeTmExr)
					WriteEXRImage(rgb, alpha, filename+postfix+".exr");

				// write out tonemapped IGI
				if(writeTmIgi)
					WriteIGIImage(rgb, alpha, filename+postfix+".igi"); // TODO add samples
			}
		}
	}
}

void FlexImageFilm::ScaleOutput(float *rgb, float *alpha, float *scale)
{
	int x,y;
	int offset = 0;
	for (y = 0; y < yPixelCount; ++y)
	{
		for (x = 0; x < xPixelCount; ++x,++offset)
		{
			rgb[3*offset  ] *= scale[offset];
			rgb[3*offset+1] *= scale[offset];
			rgb[3*offset+2] *= scale[offset];
			alpha[offset] *= scale[offset];
		}
	}
}
void FlexImageFilm::WriteImage(ImageType type)
{
	int x,y,offset;
	int nPix = xPixelCount * yPixelCount;
	float *rgb0 = new float[3*nPix], *alpha0 = new float[nPix];
	float *rgb = new float[3*nPix], *alpha = new float[nPix];
	if (factor==NULL)
	{
		factor = new float[nPix];
		scene->camera->GetFlux2RadianceFactors(this,factor,xPixelCount,yPixelCount);
	}
	memset(rgb0,0,nPix*3*sizeof(*rgb0));
	memset(alpha0,0,nPix*sizeof(*alpha0));

	for(u_int j=0;j<bufferGroups.size();++j)
	{
		for(u_int i=0;i<bufferConfigs.size();++i)
		{
			bufferGroups[j].buffers[i]->GetData(rgb, alpha);
			if (bufferConfigs[i].output & BUF_FRAMEBUFFER)
			{
				for (offset = 0, y = 0; y < yPixelCount; ++y)
				{
					for (x = 0; x < xPixelCount; ++x,++offset)
					{
						rgb0[3*offset  ] += rgb[3*offset  ];
						rgb0[3*offset+1] += rgb[3*offset+1];
						rgb0[3*offset+2] += rgb[3*offset+2];
						alpha0[offset] += alpha[offset];
					}
				}
			}
			if (bufferConfigs[i].output & BUF_STANDALONE)
			{
				if (!(bufferConfigs[i].output & BUF_RAWDATA) && scene->surfaceIntegrator->IsFluxBased())
					ScaleOutput(rgb,alpha,factor);
				WriteImage2(type, rgb, alpha, bufferConfigs[i].postfix);
			}
		}

		if (scene->surfaceIntegrator->IsFluxBased())
		{
			ScaleOutput(rgb0,alpha0,factor);
		}
		WriteImage2(type, rgb0, alpha0, "");
	}

	delete[] rgb;
	delete[] alpha;
	delete[] rgb0;
	delete[] alpha0;
}

// GUI display methods
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
	if(!framebuffer)
		createFrameBuffer();

	WriteImage(IMAGE_FRAMEBUFFER);
}

unsigned char* FlexImageFilm::getFrameBuffer()
{
	if(!framebuffer)
		createFrameBuffer();

	return framebuffer;
}

void FlexImageFilm::WriteResumeFilm(const string &filename)
{
	// Dade - save the status of the film to the file

	luxError(LUX_NOERROR, LUX_INFO, (std::string("Writing film status to file ")+filename).c_str());

    std::ofstream filestr(filename.c_str());

    TransmitFilm(filestr,0,0,false);

    filestr.close();
}

void FlexImageFilm::WriteTGAImage(float *rgb, float *alpha, const string &filename)
{
	//printf("\nWriting Tonemapped TGA image to file \"%s\"...\n", filename.c_str());
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
	memset(header, 0,sizeof(char)*18);

	header[2] = 2;							// set the data type of the targa (2 = uncompressed)
	short xResShort = xResolution;			// set the resolution and make sure the bytes are in the right order
	header[13] = (char) (xResShort >> 8);
	header[12] = xResShort;
	short yResShort = yResolution;
	header[15] = (char) (yResShort >> 8);
	header[14] = yResShort;
	header[16] = 32;						// set the bitdepth

	// put the header data into the file
	for (int i=0; i < 18; i++)
		fputc(header[i],tgaFile);

	// write the bytes of data out
	for (int i=yPixelCount-1;  i >= 0 ; i--) {
		for (int j=0;  j < xPixelCount; j++) {
			fputc((int) rgb[(i*xPixelCount+j)*3+2], tgaFile);
			fputc((int) rgb[(i*xPixelCount+j)*3+1], tgaFile);
			fputc((int) rgb[(i*xPixelCount+j)*3+0], tgaFile);
			// NOTE - radiance - removed alpha output in TGA files due to errors
			fputc((int) 255.0, tgaFile);
			//fputc((int) (255.0*alpha[(i*xPixelCount+j)]), tgaFile);
		}
	}

	fclose(tgaFile);
	//printf("Done.\n");
}

void FlexImageFilm::WriteEXRImage(float *rgb, float *alpha, const string &filename)
{
	// Write OpenEXR RGBA image
	luxError(LUX_NOERROR, LUX_INFO, (std::string("Writing OpenEXR image to file ")+filename).c_str());
	WriteRGBAImageFloat(filename, rgb, alpha,
		xPixelCount, yPixelCount,
		xResolution, yResolution,
		xPixelStart, yPixelStart);
}

void FlexImageFilm::WriteIGIImage(float *rgb, float *alpha, const string &filename)
{
	// Write IGI image
	luxError(LUX_NOERROR, LUX_INFO, (std::string("Writing IGI image to file ")+filename).c_str());
	WriteIgiImage(filename, rgb, alpha,
		xPixelCount, yPixelCount,
		xResolution, yResolution,
		xPixelStart, yPixelStart);
}

// Dade - used to check and swap bytes in the network rendering code

bool IsLittleEndian() {
    union ShortBytes {
        short shortValue;
        unsigned char bytes[2];
    };

    ShortBytes shortTest;
    shortTest.shortValue = 1;

    return (shortTest.bytes[0] == 1);
}

void writeLittleEndianFloat(bool isLittleEndian, std::stringstream &os, float value) {
    if(isLittleEndian)
        os.write((char *)&value, sizeof(float));
    else {
        union FloatBytes {
            float floatValue;
            unsigned char bytes[4];
        };

        FloatBytes f;
        f.floatValue = value;

        for(unsigned int i = 0; i < sizeof(float); i++)
            os.write((char *)&f.bytes[sizeof(float) - i -1], 1);
    }
}

void readLittleEndianFloat(bool isLittleEndian, filtering_stream<input> &in, float *value) {
    if(isLittleEndian)
        in.read((char *)value, sizeof(float));
    else {
        union FloatBytes {
            float floatValue;
            unsigned char bytes[4];
        };

        FloatBytes f;
        
        for(unsigned int i = 0; i < sizeof(float); i++)
            in.read((char *)&f.bytes[sizeof(float) - i -1], 1);

        (*value) = f.floatValue;
    }
}

void FlexImageFilm::TransmitFilm(
        std::basic_ostream<char> &stream,
        int buf_id, int bufferGroup,
        bool clearGroup) {
    BlockedArray<Pixel> *pixelBuf;
    float numberOfSamples;
    {
        // Dade - collect data to transmit
        boost::mutex::scoped_lock lock(addSampleMutex);

        BufferGroup &currentGroup = bufferGroups[buf_id];
		Buffer *buffer = currentGroup.getBuffer(bufferGroup);

        pixelBuf = new BlockedArray<Pixel>(*buffer->pixels);
        numberOfSamples = currentGroup.numberOfSamples;

        if(clearGroup) {
            // Dede - reset the rendering buffer
            buffer->Clear();
            currentGroup.numberOfSamples = 0;
        }
    }

    bool isLittleEndian = IsLittleEndian();

    std::stringstream ss;
    ss << "Transfering " << (pixelBuf->uSize() * pixelBuf->uSize()) << 
            " pixels (" <<numberOfSamples << " samples, little endian " <<
            (isLittleEndian ? "true" : "false") << ")";
    luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

    // Dade - TODO: check stream i/o for errors

    std::stringstream os;
    writeLittleEndianFloat(isLittleEndian, os, numberOfSamples);

    for (int y = 0; y < pixelBuf->vSize(); ++y) {
        for (int x = 0; x < pixelBuf->uSize(); ++x) {
            Pixel &pixel = (*pixelBuf)(x, y);
            writeLittleEndianFloat(isLittleEndian, os, pixel.L.c[0]);
            writeLittleEndianFloat(isLittleEndian, os, pixel.L.c[1]);
            writeLittleEndianFloat(isLittleEndian, os, pixel.L.c[2]);
            writeLittleEndianFloat(isLittleEndian, os, pixel.alpha);
            writeLittleEndianFloat(isLittleEndian, os, pixel.weightSum);
        }
    }
    

    filtering_streambuf<input> in;
    in.push(gzip_compressor(9));
    in.push(os);
    std::streamsize size = boost::iostreams::copy(in, stream);

    ss.str("");
    ss << "Pixels transmition done (" << (size / 1024.0f) << " Kbytes sent)";
    luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

    delete pixelBuf;
}

void  FlexImageFilm::UpdateFilm(Scene *scene, std::basic_istream<char> &stream,
        int buf_id, int bufferGroup) {
    BlockedArray<Pixel> *pixelBuf;
    {
        // Dade - prepare buffer to receive data
        boost::mutex::scoped_lock lock(addSampleMutex);

        BufferGroup &currentGroup = bufferGroups[buf_id];
		Buffer *buffer = currentGroup.getBuffer(bufferGroup);

        pixelBuf = new BlockedArray<Pixel>(buffer->xPixelCount, buffer->yPixelCount);
    }

    filtering_stream<input> in;
    in.push(gzip_decompressor());
    in.push(stream);

    bool isLittleEndian = IsLittleEndian();

    float numberOfSamples;
    readLittleEndianFloat(isLittleEndian, in, &numberOfSamples);
    if (in.good()) {
        std::stringstream ss;
        ss << "Receiving " <<  numberOfSamples << " samples (little endian " <<
                (isLittleEndian ? "true" : "false") << ")";
        luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

        for (int y = 0; y < pixelBuf->vSize(); ++y) {
            for (int x = 0; x < pixelBuf->uSize(); ++x) {
                Pixel &pixel = (*pixelBuf)(x, y);
                readLittleEndianFloat(isLittleEndian, in, &pixel.L.c[0]);
                readLittleEndianFloat(isLittleEndian, in, &pixel.L.c[1]);
                readLittleEndianFloat(isLittleEndian, in, &pixel.L.c[2]);
                readLittleEndianFloat(isLittleEndian, in, &pixel.alpha);
                readLittleEndianFloat(isLittleEndian, in, &pixel.weightSum);
            }
        }

        // Dade - check stream i/o for errors
        if (in.good()) {
            // Dade - add all received pixels
            boost::mutex::scoped_lock lock(addSampleMutex);

            BufferGroup &currentGroup = bufferGroups[buf_id];
            Buffer *buffer = currentGroup.getBuffer(bufferGroup);

            for (int y = 0; y < buffer->yPixelCount; ++y) {
                for (int x = 0; x < buffer->xPixelCount; ++x) {
                    Pixel &pixel = (*pixelBuf)(x, y);
                    Pixel &pixelResult = (*buffer->pixels)(x, y);

                    pixelResult.L.c[0] += pixel.L.c[0];
                    pixelResult.L.c[1] += pixel.L.c[1];
                    pixelResult.L.c[2] += pixel.L.c[2];
                    pixelResult.alpha += pixel.alpha;
                    pixelResult.weightSum += pixel.weightSum;
                }
            }

            currentGroup.numberOfSamples += numberOfSamples;

            if(scene != NULL)
                scene->numberOfSamplesFromNetwork += numberOfSamples;
        } else {
            luxError(LUX_SYSTEM, LUX_ERROR, "Error while reading samples");
        }
    } else {
        luxError(LUX_SYSTEM, LUX_ERROR, "Error while reading samples");
    }

    delete pixelBuf;
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

	// output filenames
	string filename = params.FindOneString("filename", "luxout");

	// intervals
	int writeInterval = params.FindOneInt("writeinterval", 60);
	int displayInterval = params.FindOneInt("displayinterval", 12);

	// Default tonemapping options (reinhard)
	float reinhard_prescale = params.FindOneFloat("reinhard_prescale", 1.0f);
	float reinhard_postscale = params.FindOneFloat("reinhard_postscale", 1.0f);
	float reinhard_burn = params.FindOneFloat("reinhard_burn", 6.0f);
	// Gamma correction
	float gamma = params.FindOneFloat("gamma", 2.2f);

	// Rejection mechanism
	int reject_warmup = params.FindOneInt("reject_warmup", 3); // minimum samples/px before rejecting

	// Debugging mode (display erratic sample values and disable rejection mechanism)
	bool debug_mode = params.FindOneBool("debug", false);

	return new FlexImageFilm(xres, yres, filter, crop,
		filename, premultiplyAlpha, writeInterval, displayInterval,
		w_tonemapped_EXR, w_untonemapped_EXR, w_tonemapped_IGI, w_untonemapped_IGI, w_tonemapped_TGA, w_resume_FLM,
		reinhard_prescale, reinhard_postscale, reinhard_burn, gamma, reject_warmup, debug_mode);
}
