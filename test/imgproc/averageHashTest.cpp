/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"

#include "average_hash.h"

#include "core/codec/Common.h"
#include "support/image/Image.h"
#include "support/image/cv_bridge.h"
#include <opencv2/opencv.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::string;

TEST_CASE( "averageHashTest/test5x5.Light", "[unit]" )
{
	Image tile = cimbar::getTile(2, 0, false);
	cv::Mat tileMat(tile.rows, tile.cols, CV_8UC(tile.channels()), tile.data, tile.stride);
	assertEquals(0x8cef, image_hash::average_hash(tileMat));

	tile = cimbar::getTile(2, 1, false);
	cv::Mat tileMat2(tile.rows, tile.cols, CV_8UC(tile.channels()), tile.data, tile.stride);
	assertEquals(0xf38c20, image_hash::average_hash(tileMat2));
}

TEST_CASE( "averageHashTest/test5x5.Dark", "[unit]" )
{
	Image tile = cimbar::getTile(2, 0, true);
	cv::Mat tileMat(tile.rows, tile.cols, CV_8UC(tile.channels()), tile.data, tile.stride);
	assertEquals(0x1ff7310, image_hash::average_hash(tileMat));

	tile = cimbar::getTile(2, 1, true);
	cv::Mat tileMat2(tile.rows, tile.cols, CV_8UC(tile.channels()), tile.data, tile.stride);
	assertEquals(0x10c73df, image_hash::average_hash(tileMat2));
}

TEST_CASE( "averageHashTest/test8x8.Light", "[unit]" )
{
	Image tile = cimbar::getTile(4, 0, false);
	cv::Mat tileMat(tile.rows, tile.cols, CV_8UC(tile.channels()), tile.data, tile.stride);
	assertEquals(0x103070f1f3f7f, image_hash::average_hash(tileMat));

	tile = cimbar::getTile(4, 1, false);
	cv::Mat tileMat2(tile.rows, tile.cols, CV_8UC(tile.channels()), tile.data, tile.stride);
	assertEquals(0x7f3f1f0f07030100, image_hash::average_hash(tileMat2));
}

TEST_CASE( "averageHashTest/test8x8.Dark", "[unit]" )
{
	Image tile = cimbar::getTile(4, 0, true);
	cv::Mat tileMat(tile.rows, tile.cols, CV_8UC(tile.channels()), tile.data, tile.stride);
	assertEquals(0xfffefcf8f0e0c080, image_hash::average_hash(tileMat));

	tile = cimbar::getTile(4, 1, true);
	cv::Mat tileMat2(tile.rows, tile.cols, CV_8UC(tile.channels()), tile.data, tile.stride);
	assertEquals(0x80c0e0f0f8fcfeff, image_hash::average_hash(tileMat2));
}

TEST_CASE( "averageHashTest/test8x8.Resize", "[unit]" )
{
	Image tile = cimbar::getTile(4, 0, true);

	Image big;
	cv_bridge::resize(tile, big, 32, 32);
	cv::Mat bigMat(big.rows, big.cols, CV_8UC(big.channels()), big.data, big.stride);
	assertEquals(0xfffefcf8f0e0c080, image_hash::average_hash(bigMat));
}

