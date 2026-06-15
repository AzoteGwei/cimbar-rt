/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <opencv2/opencv.hpp>

class DistortionParameters
{
public:
	cv::Mat camera;
	cv::Mat distortion;

public:
	DistortionParameters()
	    : camera()
	    , distortion()
	{}

	DistortionParameters(const cv::Mat& camera, const cv::Mat& distortion)
	    : camera(camera)
	    , distortion(distortion)
	{}

	operator bool() const
	{
		return camera.cols > 0;
	}
};
