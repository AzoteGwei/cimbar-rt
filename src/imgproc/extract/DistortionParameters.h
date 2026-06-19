/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "support/image/cv_bridge.h"

class DistortionParameters
{
public:
	cv_bridge::FloatMatrix camera;
	cv_bridge::FloatMatrix distortion;

public:
	DistortionParameters()
	    : camera()
	    , distortion()
	{}

	DistortionParameters(const cv_bridge::FloatMatrix& camera, const cv_bridge::FloatMatrix& distortion)
	    : camera(camera)
	    , distortion(distortion)
	{}

	operator bool() const
	{
		return camera.rows > 0;
	}
};
