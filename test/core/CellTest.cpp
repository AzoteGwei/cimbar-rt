/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"
#include "TestHelpers.h"

#include "Cell.h"
#include "Common.h"
#include "support/image/cv_bridge.h"

#include <iostream>
#include <string>
#include <vector>
using std::string;


TEST_CASE( "CellTest/testRgbMatchesMean", "[unit]" )
{
	Image img = TestCimbar::loadSample("mycell.png");
	auto expectedColor = cv_bridge::mean(img);

	auto [r, g, b] = Cell(img).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (unsigned)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (unsigned)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (unsigned)b );
	}
}

TEST_CASE( "CellTest/testRgbCellOffsets", "[unit]" )
{
	Image img = TestCimbar::loadSample("6bit/4color_ecc30_fountain_0.png");

	auto expectedColor = cv_bridge::mean_roi(img, 125, 8, 8, 8);

	auto [r, g, b] = Cell(img, 125, 8, 8, 8).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (int)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (int)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (int)b );
	}
}

TEST_CASE( "CellTest/testRgbCellOffsets.Contiguous", "[unit]" )
{
	Image img = TestCimbar::loadSample("6bit/4color_ecc30_fountain_0.png");

	auto expectedColor = cv_bridge::mean_roi(img, 125, 8, 8, 8);

	auto [r, g, b] = Cell(img, 125, 8, 8, 8).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (int)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (int)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (int)b );
	}
}

TEST_CASE( "CellTest/testRgbCellOffsets.Asymmetric", "[unit]" )
{
	Image img = TestCimbar::loadSample("6bit/4color_ecc30_fountain_0.png");

	auto expectedColor = cv_bridge::mean_roi(img, 125, 8, 4, 6);

	auto [r, g, b] = Cell(img, 125, 8, 4, 6).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (int)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (int)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (int)b );
	}
}

TEST_CASE( "CellTest/testRgbCellOffsets.Asymmetric.Contiguous", "[unit]" )
{
	Image img = TestCimbar::loadSample("6bit/4color_ecc30_fountain_0.png");

	auto expectedColor = cv_bridge::mean_roi(img, 126, 9, 6, 6);

	auto [r, g, b] = Cell(img, 126, 9, 6, 6).mean_rgb();

	DYNAMIC_SECTION( "r" )
	{
		assertAlmostEquals( expectedColor[0], (int)r );
		assertEquals( 198, (int)r );
	}
	DYNAMIC_SECTION( "g" )
	{
		assertAlmostEquals( expectedColor[1], (int)g );
		assertEquals( 198, (int)g );
	}
	DYNAMIC_SECTION( "b" )
	{
		assertAlmostEquals( expectedColor[2], (int)b );
		assertEquals( 0, (int)b );
	}
}
