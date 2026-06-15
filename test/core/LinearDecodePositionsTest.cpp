/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"

#include "LinearDecodePositions.h"

#include <iostream>
#include <string>
#include <vector>

TEST_CASE( "LinearDecodePositionsTest/testSimple", "[unit]" )
{
	LinearDecodePositions cells(cimbar::vec_xy{9, 9}, cimbar::vec_xy{112, 112}, 8, cimbar::vec_xy{6, 6});

	// test first coordinate. We'll just count the rest.
	assertFalse( cells.done() );
	auto [i, xy, drift] = cells.next();
	assertEquals(62, xy.first);
	assertEquals(8, xy.second);
	assertEquals(i, 0);

	unsigned count = 1;
	while (!cells.done())
	{
		auto [i, xy, drift] = cells.next();
		assertEquals(i, count);
		++count;
	}

	assertEquals(12400, count);
}
