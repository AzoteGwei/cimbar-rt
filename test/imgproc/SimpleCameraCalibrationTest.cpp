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
#include <opencv2/opencv.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

TEST_CASE( "SimpleCameraCalibrationTest/testGetParams", "[unit]" )
{
	cv::Mat img = TestCimbar::loadSample("6bit/4_30_f0_627.jpg");

	SimpleCameraCalibration scc;
	DistortionParameters dp = scc.scan(img);

	std::stringstream cam;
	cam << dp.camera;

	std::stringstream dis;
	dis << dp.distortion;

	assertEquals( "[320, 0, 640;\n"
	              " 0, 240, 480;\n"
	              " 0, 0, 1]", cam.str() );
	assertEquals( "[-0.001308300426007405, 0, 0, 0]", dis.str() );
}
