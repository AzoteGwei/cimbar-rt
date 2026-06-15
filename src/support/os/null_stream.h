/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

class null_stream
{
public:
	null_stream()
	{}

	null_stream& write(const char*, unsigned length)
	{
		_count += length;
		return *this;
	}

	bool good() const
	{
		return true;
	}

	long tellp() const
	{
		return _count;
	}

protected:
	long _count = 0;
};
