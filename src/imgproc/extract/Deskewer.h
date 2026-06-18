/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Corners.h"

#include "support/image/cv_bridge.h"
#include "support/os/vec_xy.h"

#include <vector>

class Deskewer
{
public:
	Deskewer(unsigned padding=0, cimbar::vec_xy image_size={}, unsigned anchor_size=0);

	template <typename MAT>
	MAT deskew(const MAT& img, const Corners& corners);

protected:
	cimbar::vec_xy _imageSize;
	unsigned _anchorSize;
	unsigned _padding;
};

template <typename MAT>
inline MAT Deskewer::deskew(const MAT& img, const Corners& corners)
{
	std::vector<cv_bridge::Point2f> outputPoints;
	outputPoints.push_back(cv_bridge::Point2f(_anchorSize+_padding, _anchorSize+_padding));
	outputPoints.push_back(cv_bridge::Point2f(_imageSize.width() - _anchorSize+_padding, _anchorSize+_padding));
	outputPoints.push_back(cv_bridge::Point2f(_anchorSize+_padding, _imageSize.height() - _anchorSize+_padding));
	outputPoints.push_back(cv_bridge::Point2f(_imageSize.width() - _anchorSize+_padding, _imageSize.height() - _anchorSize+_padding));

	unsigned out_w = _imageSize.width() + (_padding*2);
	unsigned out_h = _imageSize.height() + (_padding*2);
	MAT output = cv_bridge::create(out_w, out_h, img.channels());

	std::vector<cv_bridge::Point2f> src_pts = corners.all();
	cv_bridge::warp_perspective(img, output, src_pts.data(), outputPoints.data(), out_w, out_h);
	return output;
}
