/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "CellPositions.h"
#include "CimbEncoder.h"
#include "support/image/Image.h"
#include "support/os/vec_xy.h"

class CimbWriter
{
public:
	CimbWriter(unsigned symbol_bits, unsigned color_bits, bool dark=true, unsigned color_mode=1, cimbar::vec_xy size={});

	bool write(unsigned bits);
	bool done() const;

	const Image& image() const;

	unsigned num_cells() const;

protected:
	void paste(const Image& img, int x, int y);

protected:
	Image _image;
	CellPositions _positions;
	CimbEncoder _encoder;
	unsigned _offsetX = 0;
	unsigned _offsetY = 0;
};
