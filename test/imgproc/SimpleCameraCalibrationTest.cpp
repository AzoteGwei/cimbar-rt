/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"
#include "TestHelpers.h"

#include "SimpleCameraCalibration.h"

#include "DistortionParameters.h"
#include "support/image/cv_bridge.h"

TEST_CASE( "SimpleCameraCalibrationTest/testGetParams", "[unit]" )
{
	Image img = TestCimbar::loadSample("6bit/4_30_f0_627.jpg");

	SimpleCameraCalibration scc;
	DistortionParameters dp = scc.scan(img);

	// camera matrix 3x3
	assertEquals(3, dp.camera.rows);
	assertEquals(3, dp.camera.cols);
	assertEquals(320.0f, dp.camera.at(0, 0));
	assertEquals(0.0f, dp.camera.at(0, 1));
	assertEquals(640.0f, dp.camera.at(0, 2));
	assertEquals(0.0f, dp.camera.at(1, 0));
	assertEquals(240.0f, dp.camera.at(1, 1));
	assertEquals(480.0f, dp.camera.at(1, 2));
	assertEquals(0.0f, dp.camera.at(2, 0));
	assertEquals(0.0f, dp.camera.at(2, 1));
	assertEquals(1.0f, dp.camera.at(2, 2));

	// distortion 1x4
	assertEquals(1, dp.distortion.rows);
	assertEquals(4, dp.distortion.cols);
	REQUIRE(dp.distortion.at(0, 0) == Approx(-0.001308300426007405f).epsilon(1e-6));
	assertEquals(0.0f, dp.distortion.at(0, 1));
	assertEquals(0.0f, dp.distortion.at(0, 2));
	assertEquals(0.0f, dp.distortion.at(0, 3));
}
