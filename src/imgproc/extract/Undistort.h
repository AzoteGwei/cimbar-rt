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
		_maps = cv_bridge::init_undistort_rectify_map(
			reinterpret_cast<const double*>(_params.camera.data),
			reinterpret_cast<const double*>(_params.distortion.data),
			width, height);
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
