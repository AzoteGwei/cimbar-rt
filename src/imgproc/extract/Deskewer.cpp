/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Deskewer.h"
#include "core/codec/Config.h"

Deskewer::Deskewer(unsigned padding, cimbar::vec_xy image_size, unsigned anchor_size)
	: _imageSize({
		image_size.width()? image_size.width() : cimbar::Config::image_size_x(),
		image_size.height()? image_size.height() : cimbar::Config::image_size_y()})
	, _anchorSize(anchor_size? anchor_size : cimbar::Config::anchor_size())
	, _padding(padding)
{
}
