/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#include "support/image/Image.h"
#include "support/image/cv_bridge.h"
#include "support/text/format.h"
#include <string>

namespace TestCimbar
{
	inline std::string getSample(std::string filename)
	{
		return std::string(LIBCIMBAR_PROJECT_ROOT) + "/samples/" + filename;
	}

	inline Image loadSample(std::string filename)
	{
		return cv_bridge::imread(getSample(filename));
	}

	inline std::string getProjectDir()
	{
		return std::string(LIBCIMBAR_PROJECT_ROOT);
	}
}

