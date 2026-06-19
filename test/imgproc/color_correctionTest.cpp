/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"

#include "adaptation_transform.h"
#include "color_correction.h"

#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using std::string;

TEST_CASE( "color_correctionTest/testTransform", "[unit]" )
{
	cv_bridge::Mat3x3 mat = color_correction::get_adaptation_matrix<adaptation_transform::von_kries>({192, 255, 255}, {255, 255, 255});

	{
		// verify matrix values (row-major)
		assertAlmostEquals( 1.0655777f, mat[0] );
		assertAlmostEquals( 0.2109226f, mat[1] );
		assertAlmostEquals( -0.013239831f, mat[2] );
	}

	std::tuple<float, float, float> c = color_correction(std::move(mat)).transform(180, 98, 255);
	assertAlmostEquals( 209.09822971, std::get<0>(c) );
	assertAlmostEquals( 99.72629027, std::get<1>(c) );
	assertAlmostEquals( 255, std::get<2>(c) );
}

TEST_CASE( "color_correctionTest/testComputeMoorePenrose", "[unit]" )
{
	cv_bridge::FloatMatrix actual(5, 3);
	float actual_data[] = {
		0, 142.31060606f, 0,
		0, 148.75f, 148.75f,
		148.75f, 148.75f, 0,
		148.75f, 0, 148.75f,
		255, 255, 255
	};
	actual.data.assign(actual_data, actual_data + 15);

	cv_bridge::FloatMatrix desired(5, 3);
	float desired_data[] = {
		0, 255, 0,
		0, 255, 255,
		255, 255, 0,
		255, 0, 255,
		255, 255, 255
	};
	desired.data.assign(desired_data, desired_data + 15);

	cv_bridge::Mat3x3 mat = color_correction::get_moore_penrose_lsm(actual, desired);
	{
		// verify matrix values (row-major)
		assertAlmostEquals( 1.5223049f, mat[0] );
		assertAlmostEquals( -0.10023587f, mat[1] );
		assertAlmostEquals( -0.19198087f, mat[2] );
		assertAlmostEquals( -0.20533442f, mat[3] );
		assertAlmostEquals( 1.6441474f, mat[4] );
		assertAlmostEquals( -0.20533434f, mat[5] );
		assertAlmostEquals( -0.19198078f, mat[6] );
		assertAlmostEquals( -0.10023584f, mat[7] );
		assertAlmostEquals( 1.5223049f, mat[8] );
	}
}

TEST_CASE( "color_correctionTest/testComputeMoorePenrose.2", "[unit]" )
{
	cv_bridge::FloatMatrix actual(5, 3);
	float actual_data[] = {
		14.58901515f, 115.74431818f, 39.88320707f,
		19.34027778f, 124.4375f, 115.37152778f,
		140.70486111f, 137.45833333f, 65.50694444f,
		131.59722222f, 41.22222222f, 104.84027778f,
		171.625f, 163.625f, 158.875f
	};
	actual.data.assign(actual_data, actual_data + 15);

	cv_bridge::FloatMatrix desired(5, 3);
	float desired_data[] = {
		0, 255, 0,
		0, 255, 255,
		255, 255, 0,
		255, 0, 255,
		255, 255, 255
	};
	desired.data.assign(desired_data, desired_data + 15);

	cv_bridge::Mat3x3 mat = color_correction::get_moore_penrose_lsm(actual, desired);
	{
		assertAlmostEquals( 2.0261116f, mat[0] );
		assertAlmostEquals( -0.21691091f, mat[1] );
		assertAlmostEquals( -0.19806443f, mat[2] );
		assertAlmostEquals( -0.43822661f, mat[3] );
		assertAlmostEquals( 2.4562523f, mat[4] );
		assertAlmostEquals( -0.41700464f, mat[5] );
		assertAlmostEquals( -0.55769891f, mat[6] );
		assertAlmostEquals( -1.1443435f, mat[7] );
		assertAlmostEquals( 3.4819376f, mat[8] );
	}
}
