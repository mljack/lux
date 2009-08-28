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
 *   Lux Renderer website : http://www.luxrender.org                       *
 ***************************************************************************/

#ifndef LUX_IMAGEREADER_H
#define LUX_IMAGEREADER_H
#include "lux.h"
#include "texturecolor.h"
#include "mipmap.h"
#include "error.h"
using namespace std;

namespace lux
{

class ImageData {
public:
	enum PixelDataType {
		UNSIGNED_CHAR_TYPE,
		UNSIGNED_SHORT_TYPE,
		FLOAT_TYPE
	};

	ImageData(int width, int height, PixelDataType type, int noChannels, TextureColorBase* data) {
		width_ = width;
		height_ = height;
		pixel_type_ = type;
		noChannels_ = noChannels;
		// NOTE - Ratow - Not using auto_ptr here because of undefined behavior when deleting array data
		data_ = data;
		isExrImage_ = false;

	}

	~ImageData() {
		delete[] data_;
	}

	int getWidth() {
		return width_;
	}

	int getHeight() {
		return height_;
	}

	int getChannels() {
		return noChannels_;
	}

	PixelDataType getPixelDataType() {
		return pixel_type_;
	}

	TextureColorBase * getData() {
		return data_;
	}

	bool isExrImage() {
		return isExrImage_;
	}

	void setIsExrImage(bool exrImage) {
		isExrImage_ = exrImage;
	}

	template <class T> MIPMap<T> *createMIPMap(
			ImageTextureFilterType filterType = BILINEAR,
			float maxAniso = 8.f, ImageWrap wrapMode = TEXTURE_REPEAT,
			float gain = 1.0f, float gamma = 1.0f) {
		MIPMap<T> *mipmap = NULL;

		// Dade - added support for 1 channel maps
		if (noChannels_ == 1) {
			if (pixel_type_ == UNSIGNED_CHAR_TYPE) {
				if ((gain == 1.0f) && (gamma == 1.0f))
					mipmap = new MIPMapFastImpl<T, TextureColor<unsigned char, 1 > >(
							filterType, width_, height_,
							(TextureColor<unsigned char, 1 > *)data_,
							maxAniso, wrapMode);
				else
					mipmap = new MIPMapImpl<T, TextureColor<unsigned char, 1 > >(
							filterType, width_, height_,
							(TextureColor<unsigned char, 1 > *)data_,
							maxAniso, wrapMode, gain, gamma);
			} else if (pixel_type_ == FLOAT_TYPE) {
				if ((gain == 1.0f) && (gamma == 1.0f))
					mipmap = new MIPMapFastImpl<T, TextureColor<float, 1 > >(
							filterType, width_, height_,
							(TextureColor<float, 1 > *)data_,
							maxAniso, wrapMode);
				else
					mipmap = new MIPMapImpl<T, TextureColor<float, 1 > >(
							filterType, width_, height_,
							(TextureColor<float, 1 > *)data_,
							maxAniso, wrapMode, gain, gamma);
			} else if (pixel_type_ == UNSIGNED_SHORT_TYPE) {
				if ((gain == 1.0f) && (gamma == 1.0f))
					mipmap = new MIPMapFastImpl<T, TextureColor<unsigned short, 1 > >(
							filterType, width_, height_,
							(TextureColor<unsigned short, 1 > *)data_,
							maxAniso, wrapMode);
				else
					mipmap = new MIPMapImpl<T, TextureColor<unsigned short, 1 > >(
							filterType, width_, height_,
							(TextureColor<unsigned short, 1 > *)data_,
							maxAniso, wrapMode, gain, gamma);
			}
		} else if (noChannels_ == 3) {
			if (pixel_type_ == UNSIGNED_CHAR_TYPE) {
				if ((gain == 1.0f) && (gamma == 1.0f)) {
					mipmap = new MIPMapFastImpl<T, TextureColor<unsigned char, 3 > >(
							filterType, width_, height_,
							(TextureColor<unsigned char, 3 > *)data_,
							maxAniso, wrapMode);
				} else
					mipmap = new MIPMapImpl<T, TextureColor<unsigned char, 3 > >(
							filterType, width_, height_,
							(TextureColor<unsigned char, 3 > *)data_,
							maxAniso, wrapMode, gain, gamma);
			} else if (pixel_type_ == FLOAT_TYPE) {
				if ((gain == 1.0f) && (gamma == 1.0f))
					mipmap = new MIPMapFastImpl<T, TextureColor<float, 3 > >(
							filterType, width_, height_,
							(TextureColor<float, 3 > *)data_,
							maxAniso, wrapMode);
				else
					mipmap = new MIPMapImpl<T, TextureColor<float, 3 > >(
							filterType, width_, height_,
							(TextureColor<float, 3 > *)data_,
							maxAniso, wrapMode, gain, gamma);
			} else if (pixel_type_ == UNSIGNED_SHORT_TYPE) {
				if ((gain == 1.0f) && (gamma == 1.0f))
					mipmap = new MIPMapFastImpl<T, TextureColor<unsigned short, 3 > >(
							filterType, width_, height_,
							(TextureColor<unsigned short, 3 > *)data_,
							maxAniso, wrapMode);
				else
					mipmap = new MIPMapImpl<T, TextureColor<unsigned short, 3 > >(
							filterType, width_, height_,
							(TextureColor<unsigned short, 3 > *)data_,
							maxAniso, wrapMode, gain, gamma);
			}
		} else if (noChannels_ == 4) {
			if (pixel_type_ == UNSIGNED_CHAR_TYPE) {
				if ((gain == 1.0f) && (gamma == 1.0f))
					mipmap = new MIPMapFastImpl<T, TextureColor<unsigned char, 4 > >(
							filterType, width_, height_,
							(TextureColor<unsigned char, 4 > *)data_,
							maxAniso, wrapMode);
				else
					mipmap = new MIPMapImpl<T, TextureColor<unsigned char, 4 > >(
							filterType, width_, height_,
							(TextureColor<unsigned char, 4 > *)data_,
							maxAniso, wrapMode, gain, gamma);
			} else if (pixel_type_ == FLOAT_TYPE) {
				if ((gain == 1.0f) && (gamma == 1.0f))
					mipmap = new MIPMapFastImpl<T, TextureColor<float, 4 > >(
							filterType, width_, height_,
							(TextureColor<float, 4 > *)data_,
							maxAniso, wrapMode);
				else
					mipmap = new MIPMapImpl<T, TextureColor<float, 4 > >(
							filterType, width_, height_,
							(TextureColor<float, 4 > *)data_,
							maxAniso, wrapMode, gain, gamma);
			} else if (pixel_type_ == UNSIGNED_SHORT_TYPE) {
				if ((gain == 1.0f) && (gamma == 1.0f))
					mipmap = new MIPMapFastImpl<T, TextureColor<unsigned short, 4 > >(
							filterType, width_, height_,
							(TextureColor<unsigned short, 4 > *)data_,
							maxAniso, wrapMode);
				else
					mipmap = new MIPMapImpl<T, TextureColor<unsigned short, 4 > >(
							filterType, width_, height_,
							(TextureColor<unsigned short, 4 > *)data_,
							maxAniso, wrapMode, gain, gamma);
			}
		} else {
			luxError(LUX_SYSTEM, LUX_ERROR, "Unsupported channel count in ImageData::createMIPMap()");

			return NULL;
		}

		return mipmap;
	}

private:
	int width_;
	int height_;
	int noChannels_;
	TextureColorBase *data_;
	PixelDataType pixel_type_;
	bool isExrImage_;
};

class ImageReader {
public:

	virtual ~ImageReader() {
	}
	virtual ImageData* read(const string &name) = 0;
};

class ExrImageReader : public ImageReader {
public:

	virtual ~ExrImageReader() {
	}

	ExrImageReader() {
	};
	virtual ImageData* read(const string &name);
};

}
#endif // LUX_IMAGEREADER_H
