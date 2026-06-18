/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"
#include "TestHelpers.h"

#include "DeskewerPlus.h"
#include "imgproc/hash/average_hash.h"
#include "support/image/cv_bridge.h"
#include <opencv2/opencv.hpp>
#include <string>

TEST_CASE( "DeskewerTest/testSimple", "[unit]" )
{
	Corners corners({312, 519}, {323, 2586}, {2405, 461}, {2425, 2594});
	DeskewerPlus de(0, {1024, 1024}, 30);

	Image actual = de.deskew(TestCimbar::getSample("6bit/4_30_f0_big.jpg"), corners);
	assertEquals(1024, (int)actual.cols);
	assertEquals(1024, (int)actual.rows);

	cv::Mat actual_mat;
	cv_bridge::image_to_mat(actual, &actual_mat);
	assertEquals( 0x6e483730782fee5c, image_hash::average_hash(actual_mat) );
}

TEST_CASE( "DeskewerTest/testPadded", "[unit]" )
{
	Corners corners({312, 519}, {323, 2586}, {2405, 461}, {2425, 2594});
	DeskewerPlus de(8, {1024, 1024}, 30);

	Image actual = de.deskew(TestCimbar::getSample("6bit/4_30_f0_big.jpg"), corners);
	assertEquals(1040, (int)actual.cols);
	assertEquals(1040, (int)actual.rows);

	Image innerGrid = actual.roi(8, 8, 1024, 1024);

	cv::Mat inner_mat;
	cv_bridge::image_to_mat(innerGrid, &inner_mat);
	assertEquals( 0x6e483730782fee5c, image_hash::average_hash(inner_mat) );
}
