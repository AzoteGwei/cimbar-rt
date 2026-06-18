/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"

#include "CimbEncoder.h"

#include "core/codec/Common.h"
#include "support/image/Image.h"
#include <opencv2/opencv.hpp>

#include <iostream>
#include <string>
#include <vector>
using std::string;

TEST_CASE( "CimbEncoderTest/testSimple", "[unit]" )
{
	CimbEncoder cw(4, 0);
	const Image& res = cw.encode(14);

	Image expected = cimbar::getTile(4, 14, true);
	cv::Mat resMat(res.rows, res.cols, CV_8UC(res.channels()), res.data, res.stride);
	cv::Mat expMat(expected.rows, expected.cols, CV_8UC(expected.channels()), expected.data, expected.stride);
	REQUIRE(cv::sum(expMat != resMat) == cv::Scalar(0,0,0,0));
}

TEST_CASE( "CimbEncoderTest/testColor", "[unit]" )
{
	CimbEncoder cw(4, 3);
	const Image& res = cw.encode(55);

	Image expected = cimbar::getTile(4, 7, true, 8, 3); // 3*16 + 7 == 55
	cv::Mat resMat(res.rows, res.cols, CV_8UC(res.channels()), res.data, res.stride);
	cv::Mat expMat(expected.rows, expected.cols, CV_8UC(expected.channels()), expected.data, expected.stride);
	REQUIRE(cv::sum(expMat != resMat) == cv::Scalar(0,0,0,0));
}

