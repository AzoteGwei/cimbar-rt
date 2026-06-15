/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "bytebuf.h"
#include <istream>

namespace cimbar {

struct byte_istream : std::istream
{
	byte_istream(const char* data, unsigned len)
	    : std::istream(&_buf)
	    , _buf((char*)data, len)
	{
	}

	bytebuf _buf;
};

}
