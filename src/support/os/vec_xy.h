/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace cimbar {

struct vec_xy
{
	unsigned x = 0;
	unsigned y = 0;

	unsigned width() const
	{
		return x;
	};

	unsigned height() const
	{
		return y;
	}
};

}
