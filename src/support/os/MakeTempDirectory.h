/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdio>
#include <filesystem>
#include <string>

class MakeTempDirectory
{
public:
	MakeTempDirectory(bool cleanup=true)
		: _cleanup(cleanup)
	{
		_path = std::tmpnam(nullptr);
		std::filesystem::create_directory(_path);
	}

	~MakeTempDirectory()
	{
		if (_cleanup)
		{
			std::error_code ec;
			std::filesystem::remove_all(_path, ec);
		}
	}

	std::filesystem::path path() const
	{
		return _path;
	}

protected:
	bool _cleanup;
	std::filesystem::path _path;
};
