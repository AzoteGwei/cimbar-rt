/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "core/codec/Config.h"

class ConfigScope : cimbar::Config
{
public:
	using cimbar::Config::active_conf;

	ConfigScope(int mode_val=0)
	{
		cimbar::Config::update(mode_val);
	}

	~ConfigScope()
	{
		// reset
		cimbar::Config::update();
	}

protected:
};
