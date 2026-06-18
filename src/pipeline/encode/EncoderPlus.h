/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Encoder.h"
#include "core/codec/Config.h"
#include "imgproc/extract/Scanner.h"
#include "support/text/format.h"
#include "support/os/File.h"
#include "support/image/cv_bridge.h"

#include <filesystem>
#include <functional>
#include <string>

class EncoderPlus : public Encoder
{
public:
	using Encoder::Encoder;

#ifndef __EMSCRIPTEN__
	unsigned encode(const std::string& filename, std::string output_prefix);
	unsigned encode_fountain(const std::string& filename, std::string output_prefix, int compression_level=16, double redundancy=1.2);
#endif
	unsigned encode_fountain(const std::string& filename, const std::function<bool(const Image&, unsigned)>& on_frame, int compression_level=16, double redundancy=4.0);
};

#ifndef __EMSCRIPTEN__
inline unsigned EncoderPlus::encode(const std::string& filename, std::string output_prefix)
{
	std::ifstream f(filename);

	unsigned i = 0;
	while (true)
	{
		auto frame = encode_next(f);
		if (!frame)
			break;

		std::string output = fmt::format("{}_{}.png", output_prefix, i);
		cv_bridge::imwrite(output, *frame);
		++i;
	}
	return i;
}
#endif

inline unsigned EncoderPlus::encode_fountain(const std::string& filename, const std::function<bool(const Image&, unsigned)>& on_frame, int compression_level, double redundancy)
{
	std::ifstream infile(filename, std::ios::binary);
	fountain_encoder_stream::ptr fes = create_fountain_encoder(infile, File::basename(filename), compression_level);
	if (!fes)
		return 0;

	// ex: with ecc = 30 and 155 byte blocks, we have 60 rs blocks * 125 bytes per block == 7500 bytes to work with.
	// if fountain_chunks_per_frame() is 10, the fountain_chunk_size will be 750.
	// we calculate requiredFrames based only on symbol bits, to avoid the situation where the color decode is failing while we're
	// refusing to generate additional frames...
	unsigned requiredFrames = fes->blocks_required() * redundancy / cimbar::Config::fountain_chunks_per_frame(_bitsPerSymbol);
	if (requiredFrames == 0)
		requiredFrames = 1;

	unsigned i = 0;
	unsigned consecutiveScansFailed = 0;
	while (i < requiredFrames)
	{
		auto frame = encode_next(*fes);
		if (!frame)
			break;

		// some % of generated frames (for the current 8x8 impl)
		// will produce random patterns that falsely match as
		// corner "anchors" and fail to extract. So:
		// if frame fails the scan, skip it.
		if (!Scanner::will_it_scan(*frame))
		{
			if (++consecutiveScansFailed < 5)
				continue;

			// else, we gotta make forward progress. And it's probably a bug?
			std::cerr << fmt::format("generated {} bad frames in a row. This really shouldn't happen, maybe report a bug. :(", consecutiveScansFailed) << std::endl;
		}

		consecutiveScansFailed = 0;
		if (!on_frame(*frame, i))
			break;
		++i;
	}
	return i;
}

#ifndef __EMSCRIPTEN__
inline unsigned EncoderPlus::encode_fountain(const std::string& filename, std::string output_prefix, int compression_level, double redundancy)
{
	std::function<bool(const Image&, unsigned)> fun = [output_prefix] (const Image& frame, unsigned i) {
		std::string output = fmt::format("{}_{}.png", output_prefix, i);
		return cv_bridge::imwrite(output, frame);
	};
	return encode_fountain(filename, fun, compression_level, redundancy);
}
#endif
