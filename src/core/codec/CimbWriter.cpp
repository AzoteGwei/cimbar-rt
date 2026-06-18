/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "CimbWriter.h"

#include "Common.h"
#include "Config.h"
#include "support/text/format.h"
#include "support/image/cv_bridge.h"
#include <string>
#include <iostream>
using std::string;

using namespace cimbar;

namespace {
	Image getAnchor(bool dark)
	{
		string name = dark? "anchor-dark" : "anchor-light";
		return cimbar::load_img(fmt::format("bitmap/{}.png", name));
	}

	Image getSecondaryAnchor(bool dark)
	{
		string name = dark? "anchor-secondary-dark" : "anchor-secondary-light";
		return cimbar::load_img(fmt::format("bitmap/{}.png", name));
	}

	Image getHorizontalGuide(bool dark)
	{
		string name = dark? "guide-horizontal-dark" : "guide-horizontal-light";
		return cimbar::load_img(fmt::format("bitmap/{}.png", name));
	}

	Image getVerticalGuide(bool dark)
	{
		string name = dark? "guide-vertical-dark" : "guide-vertical-light";
		return cimbar::load_img(fmt::format("bitmap/{}.png", name));
	}
}

CimbWriter::CimbWriter(unsigned symbol_bits, unsigned color_bits, bool dark, unsigned color_mode, vec_xy size)
	: _positions(
		  cimbar::vec_xy{Config::cell_spacing_x(), Config::cell_spacing_y()},
		  cimbar::vec_xy{Config::cells_per_col_x(), Config::cells_per_col_y()},
		  Config::cell_offset(), cimbar::vec_xy{Config::corner_padding_x(), Config::corner_padding_y()},
		  Config::interleave_blocks(), Config::interleave_partitions())
	, _encoder(symbol_bits, color_bits, dark, color_mode)
{
	unsigned height = std::max(size.height(), cimbar::Config::image_size_y());
	unsigned width = std::max(size.width(), cimbar::Config::image_size_x());

	_offsetX = (width - cimbar::Config::image_size_x()) / 2;
	_offsetY = (height - cimbar::Config::image_size_y()) / 2;

	uint8_t fill = dark ? 0 : 0xFF;
	_image = cv_bridge::create(width, height, 3, fill);

	// from here on, we only care about the internal size
	width = cimbar::Config::image_size_x();
	height = cimbar::Config::image_size_y();

	Image anchor = getAnchor(dark);
	paste(anchor, 0, 0);
	paste(anchor, 0, height - anchor.height);
	paste(anchor, width - anchor.width, 0);

	Image secondaryAnchor = getSecondaryAnchor(dark);
	paste(secondaryAnchor, width - anchor.width, height - anchor.height);

	Image hg = getHorizontalGuide(dark);
	paste(hg, (width/2) - (hg.width/2), 2);
	paste(hg, (width/2) - (hg.width/2), height-4);
	paste(hg, (width/2) - (hg.width/2) - hg.width, height-4);
	paste(hg, (width/2) - (hg.width/2) + hg.width, height-4);

	Image vg = getVerticalGuide(dark);
	paste(vg, 2, (height/2) - (vg.height/2));
	paste(vg, width-4, (height/2) - (vg.height/2));
}

void CimbWriter::paste(const Image& img, int x, int y)
{
	cv_bridge::copy_to(img, _image, x + _offsetX, y + _offsetY);
}

bool CimbWriter::write(unsigned bits)
{
	// check with _encoder for tile, then place it in template according to mapping
	// mapping will track current index/location?
	if (done())
		return false;

	CellPositions::coordinate xy = _positions.next();
	const Image& cell = _encoder.encode(bits);
	paste(cell, xy.first, xy.second);
	return true;
}

bool CimbWriter::done() const
{
	return _positions.done();
}

const Image& CimbWriter::image() const
{
	return _image;
}

unsigned CimbWriter::num_cells() const
{
	return _positions.count();
}
