/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "CimbReader.h"

#include "CellDrift.h"
#include "Common.h"
#include "Config.h"
#include "Interleave.h"

#include "support/bit/bitmatrix.h"
#include "imgproc/color/adaptation_transform.h"
#include "imgproc/color/color_correction.h"
#include "support/image/cv_bridge.h"

using namespace cimbar;

namespace {
	void sharpenSymbolGrid(const Image& img, Image& out)
	{
		static const float kernel_data[9] = {0, -1, 0, -1, 4.5f, -1, 0, -1, 0};
		cv_bridge::filter2D(img, out, kernel_data, 3);
	}

	bitbuffer preprocessSymbolGrid(const Image& img, bool needs_sharpen)
	{
		int blockSize = 5; // default: no preprocessing

		Image gray;
		cv_bridge::cvt_color(img, gray, cv_bridge::COLOR_RGB2GRAY);

		Image symbols;
		if (needs_sharpen)
		{
			blockSize = 7;
			Image sharpened;
			sharpenSymbolGrid(gray, sharpened);
			cv_bridge::adaptive_threshold(sharpened, symbols, 255, blockSize, 0);
		}
		else
		{
			cv_bridge::adaptive_threshold(gray, symbols, 255, blockSize, 0);
		}

		bitbuffer bb(symbols.height * symbols.width / 8);
		bitmatrix::mat_to_bitbuffer(symbols, bb.get_writer());
		return bb;
	}

	void updateMaxColor(std::tuple<float, float, float>& max_color, const std::array<double, 4>& c)
	{
		std::get<0>(max_color) = std::max(std::get<0>(max_color), static_cast<float>(c[0]));
		std::get<1>(max_color) = std::max(std::get<1>(max_color), static_cast<float>(c[1]));
		std::get<2>(max_color) = std::max(std::get<2>(max_color), static_cast<float>(c[2]));
	}

	std::tuple<float, float, float> calculateWhite(const Image& img, unsigned padding, bool dark)
	{
		std::tuple<float, float, float> bestColor({1, 1, 1});
		if (dark)
		{
			unsigned tl = Config::anchor_size() + padding - 2;
			unsigned right = Config::image_size_x() + padding - Config::anchor_size() - 2;
			unsigned bottom = Config::image_size_y() + padding - Config::anchor_size() - 2;
			std::array<std::pair<unsigned, unsigned>, 3> anchors = {{ {tl, tl}, {tl, bottom}, {right, tl} }};
			for (auto [x, y] : anchors)
			{
				std::array<double, 4> avgColor = cv_bridge::mean_roi(img, x, y, 4, 4);
				updateMaxColor(bestColor, avgColor);
			}
		}
		else // light
		{
			// TODO ONO: this is wrong, fix it
			unsigned tl = (Config::anchor_size() << 1) + padding + 6;
			unsigned right = Config::image_size_x() + padding - tl - 4;
			unsigned bottom = Config::image_size_y() + padding - tl - 4;
			std::array<std::pair<unsigned, unsigned>, 4> anchors = {{ {0, tl}, {tl, 0}, {0, bottom}, {right, 0} }};
			for (auto [x, y] : anchors)
			{
				std::array<double, 4> avgColor = cv_bridge::mean_roi(img, x, y, 4, 4);
				updateMaxColor(bestColor, avgColor);
			}
		}
		return bestColor;
	}

	bool simpleColorCorrection(const Image& img, CimbDecoder& decoder, unsigned padding)
	{
		std::tuple<float, float, float> white = calculateWhite(img, padding, Config::dark());
		decoder.update_color_correction(color_correction::get_adaptation_matrix<adaptation_transform::von_kries>(white, {255.0, 255.0, 255.0}));
		return true;
	}

	// we will always skip the last unaltered block of a file
	// if it does not exactly match the chunk size
	// -- specifically, the encoder will skip *any* block that does not
	// match, but it so happens that it's only ever that one block.
	unsigned computeRadioactiveBlockId(const FountainMetadata& md, unsigned chunk_size)
	{
		if (md.file_size() % chunk_size == 0)
			return 0xFFFFFFFF; // there isn't one
		return md.file_size() / chunk_size;
	}
}

CimbReader::CimbReader(const Image& img, CimbDecoder& decoder, unsigned color_mode, bool needs_sharpen, int color_correction)
	: _image(img.clone())
	, _fountainColorHeader(0U)
	, _radioactiveBlockId(0) // can only compute once we know the file size
	, _cellSize(Config::cell_size() + 2)
	, _gridPadding(std::min(_image.width - Config::image_size_x(), _image.height - Config::image_size_y())/2)
	, _positions(
		  cimbar::vec_xy{Config::cell_spacing_x(), Config::cell_spacing_y()},
		  cimbar::vec_xy{Config::cells_per_col_x(), Config::cells_per_col_y()},
		  Config::cell_offset()+_gridPadding, cimbar::vec_xy{Config::corner_padding_x(), Config::corner_padding_y()}
	)
	, _decoder(decoder)
	, _good(_image.width >= Config::image_size_x() and _image.height >= Config::image_size_y())
	, _colorCorrection(color_correction)
	, _colorMode(color_mode)
{
	_grayscale = preprocessSymbolGrid(img, needs_sharpen);
	if (_good and color_correction == 1)
		simpleColorCorrection(_image, decoder, _gridPadding);
}

CIMBAR_ALWAYS_INLINE unsigned CimbReader::read_color(const PositionData& pos) const
{
	Cell color_cell(_image, pos.x, pos.y, Config::cell_size(), Config::cell_size());
	return _decoder.decode_color(color_cell, _colorMode);
}

CIMBAR_ALWAYS_INLINE unsigned CimbReader::read(PositionData& pos)
{
	if (done())
		return 0;

	// need coordinate, index, and drift from next position
	auto [i, xy, drift, cooldown] = _positions.next();
	int x = xy.first + drift.x();
	int y = xy.second + drift.y();
	bitmatrix cell(_grayscale, _image.width, _image.height, x-1, y-1);

	unsigned drift_offset = 0;
	unsigned error_distance;
	unsigned bits = _decoder.decode_symbol(cell, drift_offset, error_distance, cooldown);

	std::pair<int, int> best_drift = CellDrift::driftPairs[drift_offset];
	drift.updateDrift(best_drift.first, best_drift.second);
	_positions.update(i, drift, error_distance, CellDrift::calculate_cooldown(cooldown, drift_offset));

	pos.i = i;
	pos.x = x + best_drift.first;
	pos.y = y + best_drift.second;
	return bits;
}

bool CimbReader::done() const
{
	return !_good or _positions.done();
}

void CimbReader::init_ccm(unsigned color_bits, unsigned interleave_blocks, unsigned interleave_partitions, unsigned fountain_blocks)
{
	if (_colorCorrection != 2)
		return;

	// if no fountain header, we don't attempt color correction
	if (_fountainColorHeader.id() == 0)
		return;

	// full ccm, using header values as known color index
	CellPositions::positions_list positions = Interleave::interleave(_positions.positions(), interleave_blocks, interleave_partitions);

	unsigned end = cimbar::Config::capacity(color_bits) * 8 / color_bits;
	unsigned headerStartInterval = cimbar::Config::capacity(_decoder.symbol_bits() + color_bits) * 8 / fountain_blocks / color_bits;
	unsigned headerLen = (_fountainColorHeader.md_size) * 8 / color_bits;

	// get color map
	std::unordered_map<uint16_t, std::tuple<unsigned, unsigned, unsigned, unsigned>> colors;
	bitbuffer buff;
	for (unsigned block = 0; block < end; block+=headerStartInterval)
	{
		buff.copy_to_buffer(reinterpret_cast<const char*>(_fountainColorHeader.data()), _fountainColorHeader.md_size);

		// sample all colors in header
		for (unsigned idx = block, i = 0; idx < block+headerLen; ++idx, i+=color_bits)
		{
			unsigned expected = buff.read(i, color_bits);
			CellPositions::coordinate pos = positions[idx];

			Cell color_cell(_image, pos.first+1, pos.second+1, Config::cell_size()-2, Config::cell_size()-2);
			auto col = color_cell.mean_rgb();

			auto [it, isNew] = colors.try_emplace(expected, std::make_tuple(0, 0, 0, 0)); // count,r,g,b
			std::get<0>(it->second) += 1;
			std::get<1>(it->second) += std::get<0>(col);
			std::get<2>(it->second) += std::get<1>(col);
			std::get<3>(it->second) += std::get<2>(col);
		}

		_fountainColorHeader.increment_block_id(_radioactiveBlockId);
	}

	// build actual/desired matrices for Moore-Penrose LSM
	cv_bridge::FloatMatrix actual;
	actual.cols = 3;
	cv_bridge::FloatMatrix desired;
	desired.cols = 3;

	for (auto& it : colors)
	{
		unsigned total = std::get<0>(it.second);
		if (total == 0)
			continue;

		std::get<1>(it.second) /= total;
		std::get<2>(it.second) /= total;
		std::get<3>(it.second) /= total;

		float arow[3] = {static_cast<float>(std::get<1>(it.second)),
		                 static_cast<float>(std::get<2>(it.second)),
		                 static_cast<float>(std::get<3>(it.second))};
		actual.push_back_row(arow);

		cimbar::RGB cc = _decoder.get_color(it.first, _colorMode);
		float drow[3] = {static_cast<float>(std::get<0>(cc)),
		                 static_cast<float>(std::get<1>(cc)),
		                 static_cast<float>(std::get<2>(cc))};
		desired.push_back_row(drow);
	}

	// bail if we don't have enough data...
	if (actual.rows < 4)
		return;

	// sample corners
	{
		std::tuple<float, float, float> white = calculateWhite(_image, _gridPadding, Config::dark());
		float arow[3] = {std::get<0>(white), std::get<1>(white), std::get<2>(white)};
		actual.push_back_row(arow);

		float drow[3] = {255, 255, 255};
		desired.push_back_row(drow);
	}

	// generate ccm from avgs, save in decoder
	cv_bridge::Mat3x3 ccm = cv_bridge::moore_penrose_lsm(actual.data.data(), desired.data.data(), actual.rows);
	// ponytail: color_correction 还接受 cv::Matx，等 Phase 2.6 改为接受 Mat3x3
	cv::Matx<float, 3, 3> ccm_mat(
		ccm[0], ccm[1], ccm[2],
		ccm[3], ccm[4], ccm[5],
		ccm[6], ccm[7], ccm[8]
	);
	_decoder.update_color_correction(std::move(ccm_mat));
}

void CimbReader::update_metadata(char* buff, unsigned len, unsigned chunk_size)
{
	if (len == 0 and _fountainColorHeader.id() == 0)
		return;

	if (_fountainColorHeader.id() == 0)
		_fountainColorHeader = FountainMetadata(buff, len);

	if (_radioactiveBlockId == 0)
		_radioactiveBlockId = computeRadioactiveBlockId(_fountainColorHeader, chunk_size);
	_fountainColorHeader.increment_block_id(_radioactiveBlockId); // we always want to be +1
}

unsigned CimbReader::num_reads() const
{
	return _positions.size();
}
