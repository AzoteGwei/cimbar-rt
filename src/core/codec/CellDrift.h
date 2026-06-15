/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <array>
#include <cstdint>
#include <utility>

class CellDrift
{
public:
	CellDrift(int x=0, int y=0);

	inline static const std::array<std::pair<int, int>, 9> driftPairs = {{
		{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}
	}};

	static uint8_t calculate_cooldown(uint8_t previous, uint8_t idx);

	int x() const;
	int y() const;

	void updateDrift(int dx, int dy);

protected:
	int _x;
	int _y;
};
