/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Deskewer.h"
#include "support/image/cv_bridge.h"

class DeskewerPlus : public Deskewer
{
public:
	using Deskewer::Deskewer;
	using Deskewer::deskew;

	Image deskew(std::string img, const Corners& corners);
	bool save(const Image& img, std::string path);

protected:
};

inline Image DeskewerPlus::deskew(std::string img, const Corners& corners)
{
	Image mat = cv_bridge::imread(img);
	return Deskewer::deskew(mat, corners);
}

inline bool DeskewerPlus::save(const Image& img, std::string path)
{
	return cv_bridge::imwrite(path, img);
}
