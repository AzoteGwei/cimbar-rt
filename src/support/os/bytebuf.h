/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <streambuf>

namespace cimbar {

struct bytebuf : public std::streambuf
{
	bytebuf(char* data, size_t len)
	{
		setg(data, data, data + len);
	}
};

}
