/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "CimbDecoder.h"
#include "FloodDecodePositions.h"
#include "PositionData.h"

#include "support/bit/bitbuffer.h"
#include "core/fountain/FountainMetadata.h"
#include "support/os/compiler_constants.h"
#include "support/image/Image.h"

class CimbReader
{
public:
	CimbReader(const Image& img, CimbDecoder& decoder, unsigned color_mode, bool needs_sharpen=false, int color_correction=2);

	CIMBAR_ALWAYS_INLINE unsigned read(PositionData& pos);
	CIMBAR_ALWAYS_INLINE unsigned read_color(const PositionData& pos) const;
	bool done() const;

	void init_ccm(unsigned color_bits, unsigned interleave_blocks, unsigned interleave_partitions, unsigned fountain_blocks);
	void update_metadata(char* buff, unsigned len, unsigned chunk_size);

	unsigned num_reads() const;

protected:
	Image _image;
	bitbuffer _grayscale;
	FountainMetadata _fountainColorHeader;
	unsigned _radioactiveBlockId;

	unsigned _cellSize;
	unsigned _gridPadding;
	FloodDecodePositions _positions;
	CimbDecoder& _decoder;
	bool _good;
	int _colorCorrection;
	unsigned _colorMode;
};
