/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"
#include "TestHelpers.h"

#include "Undistort.h"

#include "Extractor.h"
#include "SimpleCameraCalibration.h"
#include "imgproc/hash/average_hash.h"
#include "support/image/cv_bridge.h"
#include <string>
#include <vector>

TEST_CASE( "UndistortTest/testUndistort", "[unit]" )
{
	Image img = TestCimbar::loadSample("6bit/4_30_f0_627.jpg");
	Image out;

	Undistort<SimpleCameraCalibration> und;
	assertTrue( und.undistort(img, out) );

	assertEquals( 0x662450383e3c4c72, image_hash::average_hash(out) );
}

TEST_CASE( "UndistortTest/testUndistortAndExtract", "[unit]" )
{
	Image img = TestCimbar::loadSample("6bit/4_30_f0_627.jpg");
	Image out;

	Undistort<SimpleCameraCalibration> und;
	assertTrue( und.undistort(img, out) );

	Extractor ex(0, {1024, 1024}, 30);
	assertTrue( ex.extract(out, out) );

	assertEquals( 0x18f26faca7766794, image_hash::average_hash(out) );
}
