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
#include <opencv2/opencv.hpp>

#include <iostream>
#include <string>
#include <vector>
using std::string;

TEST_CASE( "CimbEncoderTest/testSimple", "[unit]" )
{
	CimbEncoder cw(4, 0);
	cv::Mat res = cw.encode(14);

	cv::Mat expected = cimbar::getTile(4, 14, true);
	REQUIRE(cv::sum(expected != res) == cv::Scalar(0,0,0,0));
}

TEST_CASE( "CimbEncoderTest/testColor", "[unit]" )
{
	CimbEncoder cw(4, 3);
	cv::Mat res = cw.encode(55);

	cv::Mat expected = cimbar::getTile(4, 7, true, 8, 3); // 3*16 + 7 == 55
	REQUIRE(cv::sum(expected != res) == cv::Scalar(0,0,0,0));
}

