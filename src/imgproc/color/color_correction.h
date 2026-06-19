/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "support/image/cv_bridge.h"

// transforms are in adaptation_transform.h
// http://brucelindbloom.com/Eqn_ChromAdapt.html

class color_correction
{
public:
	template <typename AT>
	static inline cv_bridge::Mat3x3 get_adaptation_matrix(const std::tuple<float, float, float>& actual, const std::tuple<float, float, float>& desired)
	{
		AT transform;
		std::array<float, 3> src = {std::get<0>(actual), std::get<1>(actual), std::get<2>(actual)};
		std::array<float, 3> dst = {std::get<0>(desired), std::get<1>(desired), std::get<2>(desired)};

		std::array<float, 3> m1 = cv_bridge::mat3x3_multiply_vec(transform(), src);
		std::array<float, 3> m2 = cv_bridge::mat3x3_multiply_vec(transform(), dst);

		// d = diag(m2.div(m1))
		std::array<float, 3> d = {m2[0] / m1[0], m2[1] / m1[1], m2[2] / m1[2]};
		cv_bridge::Mat3x3 diag = cv_bridge::mat3x3_diag(d);
		cv_bridge::Mat3x3 t_inv = cv_bridge::mat3x3_inv(transform());
		return cv_bridge::mat3x3_multiply(cv_bridge::mat3x3_multiply(t_inv, diag), transform());
	}

	static inline cv_bridge::Mat3x3 get_moore_penrose_lsm(const cv_bridge::FloatMatrix& actual, const cv_bridge::FloatMatrix& desired)
	{
		return cv_bridge::moore_penrose_lsm(actual.data.data(), desired.data.data(), actual.rows);
	}

public:
	color_correction()
		: _active(false)
	{
	}

	color_correction(cv_bridge::Mat3x3&& m)
		: _m(m)
		, _active(true)
	{
	}

	void update(cv_bridge::Mat3x3&& m)
	{
		_m = m;
		_active = true;
	}

	bool active() const
	{
		return _active;
	}

	std::tuple<float, float, float> transform(float r, float g, float b) const
	{
		std::array<float, 3> result = cv_bridge::mat3x3_multiply_vec(_m, {r, g, b});
		return {result[0], result[1], result[2]};
	}

	const cv_bridge::Mat3x3& mat() const
	{
		return _m;
	}

protected:
	cv_bridge::Mat3x3 _m;
	bool _active;
};
