/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Common.h"

#include "Config.h"
#include "base91/base.hpp"
#include "support/text/format.h"
#include "support/image/cv_bridge.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <map>
#include <memory>
#include <string>
#include "bitmaps.h"

using cimbar::RGB;
using std::array;
using std::string;
using std::vector;

namespace {
	RGB getColor4(unsigned index)
	{
		static constexpr array<RGB, 4> colors = {
			RGB(0, 0xFF, 0),
			RGB(0, 0xFF, 0xFF),
			RGB(0xFF, 0xFF, 0),
			RGB(0xFF, 0, 0xFF),
		};
		return colors[index];
	}

	RGB getColor4_enc(unsigned index)
	{
		static constexpr array<RGB, 4> colors = {
			RGB(0, 0xFF, 0),
			RGB(0, 0xFF, 0xFF),
			RGB(0xFF, 0xFF, 0),
			RGB(0xFF, 0x55, 0xFF),
		};
		return colors[index];
	}

	RGB getColor4_old(unsigned index)
	{
		static constexpr array<RGB, 4> colors = {
			RGB(0, 0xFF, 0xFF),
			RGB(0xFF, 0xFF, 0),
			RGB(0xFF, 0, 0xFF),
			RGB(0, 0xFF, 0),
		};
		return colors[index];
	}

	RGB getColor8(unsigned index)
	{
		static constexpr array<RGB, 8> colors = {
			RGB(0, 0xFF, 0xFF), // cyan
			RGB(0xFF, 0xFF, 0), // yellow
			RGB(0x7F, 0x7F, 0xFF),  // mid-blue
			RGB(0xFF, 0xFF, 0xFF), // white
			RGB(0, 0xFF, 0), // green
			RGB(0xFF, 0x9F, 0),  // orange
			RGB(0xFF, 0, 0xFF), // magenta
			RGB(0xFF, 65, 65), // red
		};
		return colors[index];
	}

	RGB getColor8_old(unsigned index)
	{
		static constexpr array<RGB, 8> colors = {
			RGB(0, 0xFF, 0xFF), // cyan
			RGB(0x7F, 0x7F, 0xFF),  // mid-blue
			RGB(0xFF, 0, 0xFF), // magenta
			RGB(0xFF, 65, 65), // red
			RGB(0xFF, 0x9F, 0),  // orange
			RGB(0xFF, 0xFF, 0), // yellow
			RGB(0xFF, 0xFF, 0xFF),
			RGB(0, 0xFF, 0),
		};
		return colors[index];
	}

	RGB getBgColor4(unsigned index)
	{
		static constexpr array<RGB, 4> colors = {
			RGB(0, 0x20, 0),
			RGB(0, 0, 0xFF),
			RGB(0x7F, 0, 0),
			RGB(0, 0, 0),
		};
		return colors[index];
	}
}

namespace cimbar {

Image load_img(string path)
{
	auto it = cimbar::bitmaps.find(path);
	if (it == cimbar::bitmaps.end())
		return {};

	string bytes = base91::decode(it->second);
	vector<unsigned char> data(bytes.data(), bytes.data() + bytes.size());

	int width, height, channels;
	std::unique_ptr<uint8_t[], void (*)(void*)> imgdata(stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, STBI_rgb_alpha), ::free);
	if (!imgdata)
		return {};

	// stbi_load returns RGBA (4 channels), convert to RGB
	Image rgba(width, height, 4);
	std::memcpy(rgba.data, imgdata.get(), width * height * 4);

	Image rgb;
	cv_bridge::cvt_color(rgba, rgb, cv_bridge::COLOR_RGBA2RGB);
	return rgb;
}

RGB getColor(unsigned index, unsigned num_colors, unsigned color_mode)
{
	if ((color_mode & 0xFF) == 0)
	{
		if (num_colors <= 4)
			return getColor4_old(index);
		else
			return getColor8_old(index);
	}

	if (num_colors > 4)
		return getColor8(index);
	else if (color_mode > 0x100)
		return getColor4_enc(index);
	else
		return getColor4(index);

}

RGB getBgColor(unsigned index, unsigned num_colors, unsigned color_mode)
{
	// >0x100 because color_mode=0 will use default bg
	if (color_mode > 0x100 and num_colors <= 4)
		return getBgColor4(index);
	else
		return RGB(0,0,0);
}

Image getTile(unsigned symbol_bits, unsigned symbol, bool dark, unsigned num_colors, unsigned color, unsigned color_mode)
{
	static uint8_t bg_r = 0xFF, bg_g = 0xFF, bg_b = 0xFF;

	string imgPath = fmt::format("bitmap/{}/{:02x}.png", symbol_bits, symbol);
	Image tile = load_img(imgPath);

	auto [r, g, b] = getColor(color, num_colors, color_mode);
	auto [bgr, bgg, bgb] = getBgColor(color, num_colors, color_mode);

	for (unsigned y = 0; y < tile.height; ++y)
	{
		uint8_t* row = tile.ptr(y);
		for (unsigned x = 0; x < tile.width; ++x)
		{
			uint8_t* px = row + x * 3;
			if (px[0] != bg_r || px[1] != bg_g || px[2] != bg_b)
			{
				px[0] = r;
				px[1] = g;
				px[2] = b;
			}
			else if (dark)
			{
				px[0] = bgr;
				px[1] = bgg;
				px[2] = bgb;
			}
		}
	}
	return tile;
}

}
