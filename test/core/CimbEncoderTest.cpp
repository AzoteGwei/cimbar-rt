/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"

#include "CimbEncoder.h"
#include "Common.h"
#include <string>
using std::string;

namespace {
	bool images_equal(const Image& a, const Image& b)
	{
		if (a.width != b.width || a.height != b.height || a.channels() != b.channels())
			return false;
		for (unsigned y = 0; y < a.height; ++y)
		{
			const uint8_t* pa = a.ptr(y);
			const uint8_t* pb = b.ptr(y);
			for (unsigned x = 0; x < a.width * a.channels(); ++x)
				if (pa[x] != pb[x])
					return false;
		}
		return true;
	}
}

TEST_CASE( "CimbEncoderTest/testSimple", "[unit]" )
{
	CimbEncoder cw(4, 0);
	const Image& res = cw.encode(14);

	Image expected = cimbar::getTile(4, 14, true);
	REQUIRE( images_equal(res, expected) );
}

TEST_CASE( "CimbEncoderTest/testColor", "[unit]" )
{
	CimbEncoder cw(4, 3);
	const Image& res = cw.encode(55);

	Image expected = cimbar::getTile(4, 7, true, 8, 3); // 3*16 + 7 == 55
	REQUIRE( images_equal(res, expected) );
}
