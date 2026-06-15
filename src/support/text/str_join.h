/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sstream>

namespace turbo {
namespace str
{
	template <class Iter>
	inline std::string join(const Iter& start, const Iter& end, char delim=' ')
	{
		std::stringstream ss;
		Iter it = start;
		if (it != end)
			ss << *it++;
		for (; it != end; ++it)
			ss << delim << *it;
		return ss.str();
	}

	template <class Type>
	inline std::string join(const Type& container, char delim=' ')
	{
		return join(container.begin(), container.end(), delim);
	}
}
}// namespace turbo
