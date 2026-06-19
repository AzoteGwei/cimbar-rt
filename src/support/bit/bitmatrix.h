/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "bitbuffer.h"
#include "support/image/Image.h"

// wraps/inherits bitbuffer
// imposes dimensionality

class bitmatrix
{
public:
	template <typename BITSTREAM>
	static void mat_to_bitbuffer(const Image& img, BITSTREAM&& writer)
	{
		const uint8_t* p = img.ptr(0);
		unsigned size = img.width * img.height;
		while (size >= 8)
		{
			// we're turning 1 uint64_t into 8 uint8_ts
			uint64_t mval;
			memcpy(&mval, p, sizeof mval);
			mval = mval & 0x101010101010101ULL;
			const uint8_t* cv = reinterpret_cast<const uint8_t*>(&mval);
			// TODO: what about endianness???
			uint8_t val = (
				cv[0] << 7 | cv[1] << 6 | cv[2] << 5 | cv[3] << 4 | cv[4] << 3 | cv[5] << 2 |
				cv[6] << 1 | cv[7]
			);
			writer << val;
			p += 8;
			size -= 8;
		}

		// remainder
		if (size > 0)
		{
			uint8_t val = 0;
			int bit = 7;
			while (size > 0) {
				val |= (*p > 0) << bit;
				++p;
				--size;
				--bit;
			}
			writer << val;
		}
	}

public:
	bitmatrix(const bitbuffer& buff, unsigned width, unsigned height, unsigned xstart=0, unsigned ystart=0)
		: _buff(buff)
		, _xstart(xstart)
		, _ystart(ystart)
		, _width(width)
		, _height(height)
	{
	}

	unsigned get(unsigned x, unsigned y, unsigned bits) const
	{
		y += _ystart;
		x += _xstart;
		unsigned i = x + (y * _width);
		return _buff.read(i, bits);
	}

	unsigned width() const
	{
		return _width;
	}

	unsigned height() const
	{
		return _height;
	}

protected:
	const bitbuffer& _buff;
	unsigned _xstart;
	unsigned _ystart;
	unsigned _width;
	unsigned _height;
};
