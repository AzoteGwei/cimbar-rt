/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "libpopcnt/libpopcnt.h"

namespace image_hash
{
	template <typename Integer>
	inline constexpr unsigned hamming_distance(Integer a, Integer b)
	{
		return popcnt64(a xor b);
	}
}
