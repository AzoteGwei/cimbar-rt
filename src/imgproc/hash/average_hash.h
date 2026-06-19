/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ahash_result.h"
#include "bit_extractor.h"
#include "support/bit/bitmatrix.h"
#include "core/codec/Cell.h"
#include "support/os/compiler_constants.h"
#include "support/image/cv_bridge.h"

#include "intx/intx.hpp"

#include <array>
#include <bitset>
#include <cstdint>

namespace image_hash
{
	inline uint64_t average_hash(const Image& img, uint8_t threshold=0)
	{
		Image gray;
		if (img.channels() != 1)
			cv_bridge::cvt_color(img, gray, cv_bridge::COLOR_RGB2GRAY);
		else
			gray = img.clone();
		if (gray.width > 8 or gray.height > 8)
			cv_bridge::resize(gray, gray, 8, 8);

		if (threshold == 0)
			threshold = Cell(gray).mean_grayscale();

		uint64_t res = 0;
		int bitpos = gray.height * gray.width - 1;
		for (unsigned i = 0; i < gray.height; ++i)
		{
			const uint8_t* p = gray.ptr(i);
			for (unsigned j = 0; j < gray.width; ++j, --bitpos)
				res |= (uint64_t)(p[j] > threshold) << bitpos;
		}
		return res;
	}

	template <unsigned CELLSIZE>
	inline ahash_result<CELLSIZE> fuzzy_ahash(const Image& img, uint8_t threshold=0, unsigned mode=ahash_result<CELLSIZE>::ALL)
	{
		Image gray;
		if (img.channels() != 1)
			cv_bridge::cvt_color(img, gray, cv_bridge::COLOR_RGB2GRAY);
		else
			gray = img.clone();

		if (threshold == 0)
			threshold = Cell(gray).mean_grayscale();

		intx::uint128 res(0);
		int bitpos = gray.width * gray.height - 1;
		for (unsigned i = 0; i < gray.height; ++i)
		{
			const uint8_t* p = gray.ptr(i);
			for (unsigned j = 0; j < gray.width; ++j, --bitpos)
				res |= intx::uint128(p[j] > threshold) << bitpos;
		}
		return ahash_result<CELLSIZE>(res, mode);
	}

	template <unsigned CELLSIZE>
	CIMBAR_ALWAYS_INLINE inline ahash_result<CELLSIZE> fuzzy_ahash(const bitmatrix& img, unsigned mode=ahash_result<CELLSIZE>::ALL)
	{
		const unsigned readlen = CELLSIZE+2;
		intx::uint128 res(0);
		int bitpos = readlen*readlen - readlen; // 7*7 - 7 ..
		for (unsigned i = 0; i < readlen; ++i, bitpos-=readlen)
		{
			intx::uint128 r = img.get(0, i, readlen);
			res |= r << bitpos;
		}
		return ahash_result<CELLSIZE>(res, mode);
	}
}
