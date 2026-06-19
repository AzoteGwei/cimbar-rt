/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"

#include "average_hash.h"

#include "support/bit/bitbuffer.h"
#include "support/bit/bitmatrix.h"
#include "core/codec/CellDrift.h"
#include "core/codec/Common.h"
#include "support/image/Image.h"
#include "support/image/cv_bridge.h"

#include <iostream>
#include <string>
#include <vector>

using std::string;

namespace {
	Image embedTile5x5(const Image& tile, bool binaryThresh=false)
	{
		Image sevens = cv_bridge::create(7, 7, tile.channels());
		cv_bridge::copy_to(tile, sevens, 1, 1);

		if (binaryThresh)
		{
			Image gray;
			cv_bridge::cvt_color(sevens, gray, cv_bridge::COLOR_RGB2GRAY);
			cv_bridge::adaptive_threshold(gray, gray, 0xFF, 3, 0);
			return gray;
		}
		return sevens;
	}

	Image embedTile8x8(const Image& tile, bool binaryThresh=false)
	{
		Image tenxten = cv_bridge::create(10, 10, tile.channels());
		cv_bridge::copy_to(tile, tenxten, 1, 1);

		if (binaryThresh)
		{
			Image gray;
			cv_bridge::cvt_color(tenxten, gray, cv_bridge::COLOR_RGB2GRAY);
			cv_bridge::adaptive_threshold(gray, gray, 0xFF, 3, 0);
			return gray;
		}
		return tenxten;
	}
}

TEST_CASE( "fuzzyAhashTest/testCorrectness5", "[unit]" )
{
	Image tile = cimbar::getTile(2, 0, true);
	Image tenxten = embedTile5x5(tile);

	// compute the hashes we expect
	std::vector<uint64_t> expected;
	for (const std::pair<int, int>& drift : CellDrift::driftPairs)
	{
		Image img = tenxten.roi(drift.first + 1, drift.second + 1, 5, 5);
		expected.push_back(image_hash::average_hash(img, 64)); // we pass in a threshold value to match what fuzzy_ahash will compute
	}

	// do the real work
	auto actual = image_hash::fuzzy_ahash<5>(tenxten);

	for (unsigned i = 0; i < actual.size(); ++i)
		DYNAMIC_SECTION( "are we correct? : " << i )
		{
			assertEquals(expected[i], actual[i]);
		}
}

TEST_CASE( "fuzzyAhashTest/testCorrectness8", "[unit]" )
{
	Image tile = cimbar::getTile(4, 0, true);
	Image tenxten = embedTile8x8(tile);

	// compute the hashes we expect
	std::vector<uint64_t> expected;
	for (const std::pair<int, int>& drift : CellDrift::driftPairs)
	{
		Image img = tenxten.roi(drift.first + 1, drift.second + 1, 8, 8);
		expected.push_back(image_hash::average_hash(img, 64)); // we pass in a threshold value to match what fuzzy_ahash will compute
	}

	// do the real work
	auto actual = image_hash::fuzzy_ahash<8>(tenxten);

	for (unsigned i = 0; i < actual.size(); ++i)
		DYNAMIC_SECTION( "are we correct? : " << i )
		{
			assertEquals(expected[i], actual[i]);
		}
}

TEST_CASE( "fuzzyAhashTest/testIterator", "[unit]" )
{
	Image tile = cimbar::getTile(4, 0, true);
	Image tenxten = embedTile8x8(tile);

	// compute the hashes we expect
	std::vector<uint64_t> expected;
	for (const std::pair<int, int>& drift : CellDrift::driftPairs)
	{
		Image img = tenxten.roi(drift.first + 1, drift.second + 1, 8, 8);
		expected.push_back(image_hash::average_hash(img, 64)); // we pass in a threshold value to match what fuzzy_ahash will compute
	}

	// do the real work
	image_hash::ahash_result actual = image_hash::fuzzy_ahash<8>(tenxten);
	int count = 0;
	for (auto it : actual)
	{
		++count;
		DYNAMIC_SECTION( "are we correct? : " << it.first )
		{
			assertEquals(it.second, expected[it.first]);
		}
	}
	assertEquals(9, count);

	// different range-based for loop
	count = 0;
	for (auto&& [drift_idx, hash] : actual)
	{
		++count;
		DYNAMIC_SECTION( "2nd for? : " << drift_idx )
		{
			assertEquals(hash, expected[drift_idx]);
		}
	}
	assertEquals(9, count);
}

TEST_CASE( "fuzzyAhashTest/testPreThreshold", "[unit]" )
{
	Image tile = cimbar::getTile(4, 0, true);
	Image tenxten = embedTile8x8(tile, true);

	// compute the hashes we expect
	std::vector<uint64_t> expected;
	for (const std::pair<int, int>& drift : CellDrift::driftPairs)
	{
		Image img = tenxten.roi(drift.first + 1, drift.second + 1, 8, 8);
		expected.push_back(image_hash::average_hash(img, 64));
	}

	// do the real work
	auto actual = image_hash::fuzzy_ahash<8>(tenxten, 0xFE);

	for (unsigned i = 0; i < actual.size(); ++i)
		DYNAMIC_SECTION( "are we correct? : " << i )
		{
			assertEquals(expected[i], actual[i]);
		}
}

TEST_CASE( "fuzzyAhashTest/testPreThreshold.BitMatrix", "[unit]" )
{
	Image tile = cimbar::getTile(4, 0, true);
	Image tenxten = embedTile8x8(tile, true);

	// compute the hashes we expect
	std::vector<uint64_t> expected;
	for (const std::pair<int, int>& drift : CellDrift::driftPairs)
	{
		Image img = tenxten.roi(drift.first + 1, drift.second + 1, 8, 8);
		expected.push_back(image_hash::average_hash(img, 64));
	}

	bitbuffer bb;
	bitbuffer::writer writer(bb);
	bitmatrix::mat_to_bitbuffer(tenxten, writer);

	// do the real work
	bitmatrix bm(bb, 10, 10);
	auto actual = image_hash::fuzzy_ahash<8>(bm);

	for (unsigned i = 0; i < actual.size(); ++i)
		DYNAMIC_SECTION( "are we correct? : " << i )
		{
			assertEquals(expected[i], actual[i]);
		}
}

TEST_CASE( "fuzzyAhashTest/testPreThreshold.BitMatrix8.Fast", "[unit]" )
{
	Image tile = cimbar::getTile(4, 0, true);
	Image tenxten = embedTile8x8(tile, true);

	// compute the hashes we expect
	std::vector<uint64_t> expected;
	for (const std::pair<int, int>& drift : CellDrift::driftPairs)
	{
		Image img = tenxten.roi(drift.first + 1, drift.second + 1, 8, 8);
		expected.push_back(image_hash::average_hash(img, 64));
	}

	bitbuffer bb;
	bitbuffer::writer writer(bb);
	bitmatrix::mat_to_bitbuffer(tenxten, writer);

	// clear the hashes we don't care about
	expected[0] = 0;
	expected[2] = 0;
	expected[6] = 0;
	expected[8] = 0;

	// do the real work
	bitmatrix bm(bb, 10, 10);
	auto actual = image_hash::fuzzy_ahash<8>(bm, image_hash::ahash_result<8>::FAST);

	for (unsigned i = 0; i < actual.size(); ++i)
		DYNAMIC_SECTION( "are we correct? : " << i )
		{
			assertEquals(expected[i], actual[i]);
		}
}

TEST_CASE( "fuzzyAhashTest/testPreThreshold.BitMatrix5.Fast", "[unit]" )
{
	Image tile = cimbar::getTile(2, 0, true);
	Image tenxten = embedTile5x5(tile, true);

	// compute the hashes we expect
	std::vector<uint64_t> expected;
	for (const std::pair<int, int>& drift : CellDrift::driftPairs)
	{
		Image img = tenxten.roi(drift.first + 1, drift.second + 1, 5, 5);
		expected.push_back(image_hash::average_hash(img, 64));
	}

	bitbuffer bb;
	bitbuffer::writer writer(bb);
	bitmatrix::mat_to_bitbuffer(tenxten, writer);

	// clear the hashes we don't care about
	expected[0] = 0;
	expected[2] = 0;
	expected[6] = 0;
	expected[8] = 0;

	// do the real work
	bitmatrix bm(bb, 7, 7);
	auto actual = image_hash::fuzzy_ahash<5>(bm, image_hash::ahash_result<5>::FAST);

	for (unsigned i = 0; i < actual.size(); ++i)
		DYNAMIC_SECTION( "are we correct? : " << i )
		{
			assertEquals(expected[i], actual[i]);
		}
}
