/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "DistortionParameters.h"
#include "support/image/cv_bridge.h"

template <typename CAMERA_CALIBRATOR>
class Undistort
{
public:
	Undistort() {}

	Undistort(int width, int height, const DistortionParameters& params)
	{
		set_distortion_params(width, height, params);
	}

	template <typename MAT>
	static DistortionParameters get_distortion_parameters(const MAT& img)
	{
		return CAMERA_CALIBRATOR().scan(img);
	}

	bool undistort(const Image& img, Image& out)
	{
		if (_maps.empty())
		{
			if ( !set_distortion_params(img.width, img.height, get_distortion_parameters(img)) )
				return false;
		}

		cv_bridge::remap(img, out, _maps);
		return true;
	}

	bool set_distortion_params(int width, int height, const DistortionParameters& params)
	{
		if (!params)
			return false;

		_params = params;
		// DistortionParameters stores float, initUndistortRectifyMap needs double
		std::vector<double> camera_d(_params.camera.data.begin(), _params.camera.data.end());
		std::vector<double> dist_d(_params.distortion.data.begin(), _params.distortion.data.end());
		_maps = cv_bridge::init_undistort_rectify_map(camera_d.data(), dist_d.data(), width, height);
		return true;
	}

	void reset_distortion_params()
	{
		_params = {};
		_maps = {};
	}

protected:
	DistortionParameters _params;
	cv_bridge::DistortionMap _maps;
};
