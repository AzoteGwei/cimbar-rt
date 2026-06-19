/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Decoder.h"
#include "support/os/File.h"
#include "support/image/cv_bridge.h"

#include <string>

class DecoderPlus : public Decoder
{
public:
	using Decoder::Decoder;
	using Decoder::decode;

#ifndef __EMSCRIPTEN__
	unsigned decode(std::string filename, std::string output);
#endif

	bool load_ccm(std::string filename);
	bool save_ccm(std::string filename);
};

#ifndef __EMSCRIPTEN__
inline unsigned DecoderPlus::decode(std::string filename, std::string output)
{
	Image img = cv_bridge::imread(filename);

	std::ofstream f(output);
	return Decoder::decode(img, f, false);
}
#endif

inline bool DecoderPlus::load_ccm(std::string filename)
{
	File f(filename);
	std::string data = f.read_all();
	if (data.size() < 3*3*4)
		return false;

	// Mat3x3 is std::array<float, 9>, layout matches file format
	cv_bridge::Mat3x3 ccm;
	std::memcpy(ccm.data(), data.data(), 9 * sizeof(float));
	_decoder.update_color_correction(std::move(ccm));
	return true;
}

inline bool DecoderPlus::save_ccm(std::string filename)
{
	if (not _decoder.get_ccm().active())
		return false;

	const cv_bridge::Mat3x3& ccm = _decoder.get_ccm().mat();
	File f(filename, true);
	if (f.write(reinterpret_cast<const char*>(ccm.data()), ccm.size() * sizeof(float)) == 0)
		return false;
	return true;
}
