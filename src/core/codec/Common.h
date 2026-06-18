/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "support/image/Image.h"
#include <cstdint>
#include <string>
#include <tuple>

namespace cimbar
{
	using RGB = std::tuple<uint8_t,uint8_t,uint8_t>;

	Image load_img(std::string path);

	std::tuple<uint8_t,uint8_t,uint8_t> getColor(unsigned index, unsigned num_colors, unsigned color_mode);
	Image getTile(unsigned symbol_bits, unsigned symbol, bool dark=true, unsigned num_colors=4, unsigned color=0, unsigned color_mode=1);
}
