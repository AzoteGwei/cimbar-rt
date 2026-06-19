/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"
#include "TestHelpers.h"

#include "CimbDecoder.h"

#include "support/bit/bitbuffer.h"
#include "support/bit/bitmatrix.h"
#include "core/codec/Common.h"
#include "support/text/format.h"
#include "support/image/Image.h"
#include "support/image/cv_bridge.h"

#include <iostream>
#include <string>
#include <vector>
using std::string;

namespace {
	// for performance reasons, the high level Decoder/CimbReader does the decode in 2 parts. This is the one-shot version.
	unsigned decode(CimbDecoder& cd, const Image& tile10)
	{
		unsigned drift_offset;
		unsigned distance;
		unsigned bits = cd.decode_symbol(tile10, drift_offset, distance);

		auto [x,y] = CellDrift::driftPairs[drift_offset];
		Image tile8 = tile10.roi(1+x, 1+y, tile10.cols-2, tile10.rows-2);

		bits |= cd.decode_color(Cell(tile8), 1) << cd.symbol_bits();
		return bits;
	}
}

TEST_CASE( "CimbDecoderTest/testSimpleDecode", "[unit]" )
{
	CimbDecoder cd(4, 0);

	for (unsigned i = 0; i < 16; ++i)
	{
		Image tile = cimbar::getTile(4, i, true);
		Image tenxten = cv_bridge::create(10, 10, tile.channels());
		cv_bridge::copy_to(tile, tenxten, 1, 1);
		unsigned res = decode(cd, tenxten);
		assertEquals(i, res);
	}
}

TEST_CASE( "CimbDecoderTest/testPrethresholdDecode", "[unit]" )
{
	// validate the bitmatrix version acts as we expect
	CimbDecoder cd(4, 0, true, 0xFF);

	for (unsigned i = 0; i < 16; ++i)
	{
		Image tile = cimbar::getTile(4, i, true);
		Image tenxten = cv_bridge::create(10, 10, tile.channels());
		cv_bridge::copy_to(tile, tenxten, 1, 1);

		// grayscale and threshold, since that's what average_hash needs
		Image gray;
		cv_bridge::cvt_color(tenxten, gray, cv_bridge::COLOR_RGB2GRAY);
		cv_bridge::adaptive_threshold(gray, gray, 255, 9, 0);

		bitbuffer bb((100/8) + 1);
		bitmatrix::mat_to_bitbuffer(gray, bb.get_writer());
		bitmatrix bm(bb, 10, 10);

		unsigned drift_offset;
		unsigned distance;
		unsigned res = cd.decode_symbol(bm, drift_offset, distance);
		assertEquals(i, res);
		assertEquals(4, drift_offset);
		assertEquals(0, distance);
	}
}

TEST_CASE( "CimbDecoderTest/test_get_best_color_mode0", "[unit]" )
{
	CimbDecoder cd(4, 2);

	// obvious ones
	assertEquals(2, cd.get_best_color(255, 0, 255, 0));
	assertEquals(1, cd.get_best_color(255, 255, 0, 0));
	assertEquals(0, cd.get_best_color(0, 255, 255, 0));
	assertEquals(3, cd.get_best_color(0, 255, 0, 0));

	// arbitrary edge cases. We can't really say anything about the value of these colors, but we can at least pick a consistent one
	assertEquals(0, cd.get_best_color(0, 0, 0, 0));
	assertEquals(0, cd.get_best_color(70, 70, 70, 0));

	// these we can use!
	assertEquals(3, cd.get_best_color(20, 200, 20, 0));
	assertEquals(3, cd.get_best_color(50, 155, 50, 0));

	assertEquals(2, cd.get_best_color(200, 30, 200, 0));
	assertEquals(2, cd.get_best_color(155, 50, 155, 0));

	assertEquals(1, cd.get_best_color(200, 155, 20, 0));
	assertEquals(1, cd.get_best_color(155, 155, 50, 0));

	assertEquals(0, cd.get_best_color(50, 155, 200, 0));
	assertEquals(0, cd.get_best_color(50, 155, 155, 0));
}

TEST_CASE( "CimbDecoderTest/test_get_best_color_mode1", "[unit]" )
{
	CimbDecoder cd(4, 2);

	// obvious ones
	assertEquals(3, cd.get_best_color(255, 0, 255, 1));
	assertEquals(2, cd.get_best_color(255, 255, 0, 1));
	assertEquals(1, cd.get_best_color(0, 255, 255, 1));
	assertEquals(0, cd.get_best_color(0, 255, 0, 1));

	// arbitrary edge cases. We can't really say anything about the value of these colors, but we can at least pick a consistent one
	assertEquals(0, cd.get_best_color(0, 0, 0, 1));
	assertEquals(0, cd.get_best_color(70, 70, 70, 1));

	// these we can use!
	assertEquals(0, cd.get_best_color(20, 200, 20, 1));
	assertEquals(0, cd.get_best_color(50, 155, 50, 1));

	assertEquals(3, cd.get_best_color(200, 30, 200, 1));
	assertEquals(3, cd.get_best_color(155, 50, 155, 1));

	assertEquals(2, cd.get_best_color(200, 155, 20, 1));
	assertEquals(2, cd.get_best_color(155, 155, 50, 1));

	assertEquals(1, cd.get_best_color(50, 155, 200, 1));
	assertEquals(1, cd.get_best_color(50, 155, 155, 1));
}

TEST_CASE( "CimbDecoderTest/testColorDecode", "[unit]" )
{
	CimbDecoder cd(4, 2);

	Image tile = cimbar::getTile(4, 2, true, 4, 2);
	Image resized;
	cv_bridge::resize(tile, resized, 10, 10);

	unsigned color = cd.decode_color(Cell(resized), 1);
	assertEquals(2, color);
	unsigned res = decode(cd, resized);
	assertEquals(34, res);
}

TEST_CASE( "CimbDecoderTest/testAllColorDecodes", "[unit]" )
{
	CimbDecoder cd(4, 2);

	for (unsigned c = 0; c < 4; ++c)  // 2 color bits == 4 colors
		for (unsigned i = 0; i < 16; ++i)
		{
			DYNAMIC_SECTION( "testColor " << c << ":" << i )
			{
				Image tile = cimbar::getTile(4, i, true, 4, c);
				Image tenxten = cv_bridge::create(10, 10, tile.channels());
				cv_bridge::copy_to(tile, tenxten, 1, 1);

				unsigned color = cd.decode_color(Cell(tenxten), 1);
				assertEquals(c, color);
				unsigned res = decode(cd, tenxten);
				assertEquals(i+16*c, res);
			}
		}
}

TEST_CASE( "CimbDecoderTest/test_decode_symbol_sloppy", "[unit]" )
{
	CimbDecoder cd(4, 2);

	Image cell = TestCimbar::loadSample("mycell.png");

	unsigned drift_offset;
	unsigned best_distance;
	unsigned res = cd.decode_symbol(cell, drift_offset, best_distance);
	assertEquals(0, res);
	assertEquals(7, drift_offset);
	assertEquals(6, best_distance);
}
