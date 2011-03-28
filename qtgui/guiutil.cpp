/***************************************************************************
 *   Copyright (C) 1998-2011 by authors (see AUTHORS.txt )                 *
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

#include <algorithm>
#include <math.h>

#include <QFileInfo>
#include <QDateTime>
#include <QTextLayout>
#include <QPainter>

#include "guiutil.h"

using std::min;
using std::max;

int ValueToLogSliderVal(float value, const float logLowerBound, const float logUpperBound, const float slider_resolution)
{

	if (value <= 0)
		return 0;

	float logvalue = Clamp<float>(log10f(value), logLowerBound, logUpperBound);

	const int val = static_cast<int>((logvalue - logLowerBound) / 
		(logUpperBound - logLowerBound) * slider_resolution);
	return val;
}

float LogSliderValToValue(int sliderval, const float logLowerBound, const float logUpperBound, const float slider_resolution)
{

	float logvalue = (float)sliderval * (logUpperBound - logLowerBound) / 
		slider_resolution + logLowerBound;

	return powf(10.f, logvalue);
}

QString pathElidedText(const QFontMetrics &fm, const QString &text, int width, int flags) {

	const QString filename = "/" + QFileInfo(text).fileName();
	const QString path = QFileInfo(text).absolutePath();

	int fwidth = fm.width(filename);

	if (fwidth > width)
		// we're SOL, just do the default
		return fm.elidedText(text, Qt::ElideMiddle, width, flags);

	return fm.elidedText(path, Qt::ElideMiddle, width - fwidth, flags) + filename;
}

void overlayStatistics(QImage *image)
{
	QPainter p(image);

	QString stats;

	stats = "LuxRender " + QString::fromLatin1(luxVersion()) + " ";
	stats += "|Saved: " + QDateTime::currentDateTime().toString(Qt::DefaultLocaleShortDate) + " ";
	stats += "|Statistics: " + QString::fromLatin1(luxPrintableStatistics(true)) + " ";

	// convert regular spaces to non-breaking spaces, so that it will prefer to wrap
	// between segments
	stats = stats.replace(QChar(' '), QChar::Nbsp);
	stats = stats.replace("|", " |  ");

#if defined(__APPLE__)
	QFont font("Monaco");
#else
	QFont font("Helvetica");
#endif
	font.setStyleHint(QFont::SansSerif, static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality));
	
	int fontSize = (int)min(max(image->width() / 100.0f, 10.f), 18.f);
	font.setPixelSize(fontSize);

	QFontMetrics fontMetrics(font);
	int leading = fontMetrics.leading();

	QTextLayout textLayout(stats, font, image);

	QTextOption textOption;
	//textOption.setUseDesignMetrics(true);
	textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	textOption.setAlignment(Qt::AlignLeft);
	textLayout.setTextOption(textOption);

	textLayout.beginLayout();
	QTextLine line;
	qreal height = leading;
	qreal maxwidth = image->width() - 10;
	while ((line = textLayout.createLine()).isValid()) {
		line.setLineWidth(maxwidth);
		height += leading;
		line.setPosition(QPointF(0, height));
		height += line.height();
	}
	height += 2*leading;
	textLayout.endLayout();

	QRectF rect = textLayout.boundingRect();
	rect.setHeight(height);

	// align at bottom
	rect.moveLeft((image->width() - maxwidth) / 2.f);
	rect.moveTop(image->height() - rect.height());

	// darken background
	p.setOpacity(0.6);
	p.fillRect(0, rect.top(), image->width(), rect.height(), Qt::black);

	// draw text
	p.setOpacity(1.0);
	p.setPen(QColor(240, 240, 240));
	textLayout.draw(&p, rect.topLeft());

	p.end();
}

bool saveCurrentImageTonemapped(const QString &outFile, bool overlayStats, bool outputAlpha)
{
	// Saving as tonemapped image ...
	// Get width, height and pixel buffer
	int w = luxGetIntAttribute("film", "xResolution");
	int h = luxGetIntAttribute("film", "yResolution");
	// pointer needs to be const so QImage doesn't write to it
	const unsigned char* fb = luxFramebuffer();
	
	// If all looks okay, proceed
	if (!(w > 0 && h > 0 && fb))
		// Something was wrong with buffer, width or height
		return false;

	QImage image;
	
	if (outputAlpha) {
		float *alpha = luxAlphaBuffer();
		bool preMult = luxGetBoolAttribute("film", "premultiplyAlpha");

		image = QImage(w, h, (preMult ? QImage::Format_ARGB32_Premultiplied : QImage::Format_ARGB32));
		for (int y = 0; y < h; y++) {
			uchar *scanline = image.scanLine(y);
			for (int x = 0; x < w; x++) {
				scanline[4*x + 0] = fb[2];
				scanline[4*x + 1] = fb[1];
				scanline[4*x + 2] = fb[0];
				scanline[4*x + 3] = static_cast<uchar>(min(max(255.f * alpha[0], 0.f), 255.f));
				fb += 3;
				alpha++;
			}
		}
	} else {
		image = QImage(fb, w, h, w*3, QImage::Format_RGB888);
	}

	if (overlayStats)
		overlayStatistics(&image);

	return image.save(outFile);
}
