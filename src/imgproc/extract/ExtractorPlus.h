/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Extractor.h"
#include "support/image/cv_bridge.h"

#include <string>

class ExtractorPlus : public Extractor
{
public:
	using Extractor::Extractor;
	using Extractor::extract;

	int extract(std::string read_path, Image& out);
	int extract(std::string read_path, std::string write_path);

protected:
};

inline int ExtractorPlus::extract(std::string read_path, Image& out)
{
	Image img = cv_bridge::imread(read_path);
	return Extractor::extract(img, out);
}

inline int ExtractorPlus::extract(std::string read_path, std::string write_path)
{
	Image img = cv_bridge::imread(read_path);

	int res = Extractor::extract(img, img);

	cv_bridge::imwrite(write_path, img);
	return res;
}
