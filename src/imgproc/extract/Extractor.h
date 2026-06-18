/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Deskewer.h"
#include "Scanner.h"
#include "support/image/Image.h"
#include "support/os/vec_xy.h"

#include <vector>

class Extractor
{
public:
	static constexpr int FAILURE = 0;
	static constexpr int SUCCESS = 1;
	static constexpr int NEEDS_SHARPEN = 2;

public:
	Extractor(unsigned padding=0, cimbar::vec_xy image_size={}, unsigned anchor_size=0);

	int extract(const Image& img, Image& out);

protected:
	cimbar::vec_xy _imageSize;
	unsigned _anchorSize;
	unsigned _padding;
};

inline int Extractor::extract(const Image& img, Image& out)
{
	Scanner scanner(img);
	std::vector<Anchor> points = scanner.scan();
	if (points.size() < 4)
		return FAILURE;

	Corners corners(points);
	Deskewer de(_padding, _imageSize, _anchorSize);
	out = de.deskew(img, corners);

	if ( !corners.is_granular_scale(_imageSize) )
		return NEEDS_SHARPEN;
	return SUCCESS;
}
