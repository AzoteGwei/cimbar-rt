/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "support/image/Image.h"

#include <string>
#include <vector>

class CimbEncoder
{
public:
	CimbEncoder(unsigned symbol_bits, unsigned color_bits, bool dark=true, unsigned color_mode=1);

	Image load_tile(unsigned symbol_bits, unsigned index);
	bool load_tiles(unsigned symbol_bits);

	const Image& encode(unsigned bits) const;

protected:
	std::vector<Image> _tiles;
	unsigned _numSymbols;
	unsigned _numColors;
	bool _dark;
	unsigned _colorMode;
};
