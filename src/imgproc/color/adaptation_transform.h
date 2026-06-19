/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "support/image/cv_bridge.h"

namespace adaptation_transform
{
	struct bradford
	{
		const cv_bridge::Mat3x3& operator()() const
		{
			static cv_bridge::Mat3x3 transform = {
			     0.8951000f,  0.2664000f, -0.1614000f,
			    -0.7502000f,  1.7135000f,  0.0367000f,
			     0.0389000f, -0.0685000f,  1.0296000f
			};
			return transform;
		}
	};

	struct von_kries
	{
		const cv_bridge::Mat3x3& operator()() const
		{
			static cv_bridge::Mat3x3 transform = {
			     0.4002400f,  0.7076000f, -0.0808100f,
			    -0.2263000f,  1.1653200f,  0.0457000f,
			     0.0000000f,  0.0000000f,  0.9182200f
			};
			return transform;
		}
	};
}
