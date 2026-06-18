/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"

#include "CimbWriter.h"
#include "imgproc/hash/average_hash.h"

#include "support/image/Image.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <vector>

TEST_CASE( "CimbWriterTest/testSimple", "[unit]" )
{
	CimbWriter cw(4, 2);

	while (1)
	{
		if (!cw.write(0))
			break;
	}

	const Image& img = cw.image();
	assertEquals(1024, img.cols);
	assertEquals(1024, img.rows);
	cv::Mat imgMat(img.rows, img.cols, CV_8UC(img.channels()), img.data, img.stride);
	assertEquals( 0xeecc8800efce8c08, image_hash::average_hash(imgMat) );
}

TEST_CASE( "CimbWriterTest/testCustomSize", "[unit]" )
{
	CimbWriter cw(4, 2, true, 1, {1040, 1040});

	while (1)
	{
		if (!cw.write(0))
			break;
	}

	const Image& img = cw.image();
	assertEquals(1040, img.cols);
	assertEquals(1040, img.rows);
	cv::Mat imgMat(img.rows, img.cols, CV_8UC(img.channels()), img.data, img.stride);
	assertEquals( 0xab00ab02af0abfab, image_hash::average_hash(imgMat) );
}

TEST_CASE( "CimbWriterTest/testCustomSize.2", "[unit]" )
{
	CimbWriter cw(4, 2, true, 1, {1040, 1080});

	while (1)
	{
		if (!cw.write(0))
			break;
	}

	const Image& img = cw.image();
	assertEquals(1040, img.cols);
	assertEquals(1080, img.rows);
	cv::Mat imgMat(img.rows, img.cols, CV_8UC(img.channels()), img.data, img.stride);
	assertEquals( 0xab2a2a2a2a2a2aab, image_hash::average_hash(imgMat) );
}
