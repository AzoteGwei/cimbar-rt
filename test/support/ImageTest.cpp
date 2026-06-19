/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"

#include "support/image/Image.h"
#include "support/image/cv_bridge.h"
#include "support/bit/bitmatrix.h"
#include <cstring>

TEST_CASE( "ImageTest/testCreate", "[unit]" )
{
	Image img(10, 8, 3, 0);
	assertFalse( img.empty() );
	assertEquals( 10u, img.width );
	assertEquals( 8u, img.height );
	assertEquals( 3u, img.channels() );
	assertEquals( 30u, img.stride );
	assertTrue( img.owns_data );
	assertEquals( 10u, img.cols );
	assertEquals( 8u, img.rows );
	assertTrue( img.isContinuous() );

	// check fill
	for (unsigned i = 0; i < img.total(); ++i)
		assertEquals( 0, img.data[i] );
}

TEST_CASE( "ImageTest/testCreateFill", "[unit]" )
{
	Image img(4, 4, 1, 255);
	for (unsigned i = 0; i < img.total(); ++i)
		assertEquals( (uint8_t)255, img.data[i] );
}

TEST_CASE( "ImageTest/testPtr", "[unit]" )
{
	Image img(10, 8, 3);
	// ptr(0) should be data
	assertTrue( img.ptr(0) == img.data );
	// ptr(1) should be data + stride
	assertTrue( img.ptr(1) == img.data + 30 );
}

TEST_CASE( "ImageTest/testClone", "[unit]" )
{
	Image img(10, 8, 3, 42);
	Image cloned = img.clone();
	assertFalse( cloned.empty() );
	assertTrue( cloned.owns_data );
	assertTrue( cloned.data != img.data );
	assertEquals( img.width, cloned.width );
	assertEquals( img.height, cloned.height );
	assertEquals( img.channels(), cloned.channels() );
	// verify data copied
	for (unsigned i = 0; i < cloned.total(); ++i)
		assertEquals( (uint8_t)42, cloned.data[i] );
}

TEST_CASE( "ImageTest/testRoi", "[unit]" )
{
	Image img(10, 8, 3, 0);
	Image roi = img.roi(2, 3, 4, 5);
	assertFalse( roi.empty() );
	assertFalse( roi.owns_data );
	assertEquals( 4u, roi.width );
	assertEquals( 5u, roi.height );
	assertEquals( 3u, roi.channels() );
	// roi data should point into original image
	assertTrue( roi.ptr(0) == img.ptr(3) + 2 * 3 );
}

TEST_CASE( "ImageTest/testMoveConstructor", "[unit]" )
{
	Image img(10, 8, 3, 1);
	uint8_t* data_ptr = img.data;
	Image moved = std::move(img);
	assertTrue( moved.owns_data );
	assertTrue( moved.data == data_ptr );
	assertTrue( img.data == nullptr );
	assertFalse( img.owns_data );
}

TEST_CASE( "ImageTest/testMoveAssignment", "[unit]" )
{
	Image img(10, 8, 3, 1);
	uint8_t* data_ptr = img.data;
	Image other;
	other = std::move(img);
	assertTrue( other.owns_data );
	assertTrue( other.data == data_ptr );
	assertTrue( img.data == nullptr );
}

TEST_CASE( "ImageTest/testEmptyClone", "[unit]" )
{
	Image empty;
	Image cloned = empty.clone();
	assertTrue( cloned.empty() );
}

TEST_CASE( "cv_bridgeTest/testCreate", "[unit]" )
{
	Image img = cv_bridge::create(10, 8, 3, 128);
	assertFalse( img.empty() );
	assertEquals( 10u, img.width );
	assertEquals( 8u, img.height );
	assertEquals( 3u, img.channels() );
	for (unsigned i = 0; i < img.total(); ++i)
		assertEquals( (uint8_t)128, img.data[i] );
}

TEST_CASE( "cv_bridgeTest/testCvtColorRgbToGray", "[unit]" )
{
	// create a 2x2 RGB image with known colors
	Image rgb(2, 2, 3);
	// pixel (0,0) = red (255,0,0)
	rgb.ptr(0)[0] = 255; rgb.ptr(0)[1] = 0; rgb.ptr(0)[2] = 0;
	// pixel (1,0) = green (0,255,0)
	rgb.ptr(0)[3] = 0; rgb.ptr(0)[4] = 255; rgb.ptr(0)[5] = 0;
	// pixel (0,1) = blue (0,0,255)
	rgb.ptr(1)[0] = 0; rgb.ptr(1)[1] = 0; rgb.ptr(1)[2] = 255;
	// pixel (1,1) = white (255,255,255)
	rgb.ptr(1)[3] = 255; rgb.ptr(1)[4] = 255; rgb.ptr(1)[5] = 255;

	Image gray;
	cv_bridge::cvt_color(rgb, gray, cv_bridge::COLOR_RGB2GRAY);
	assertFalse( gray.empty() );
	assertEquals( 2u, gray.width );
	assertEquals( 2u, gray.height );
	assertEquals( 1u, gray.channels() );
}

TEST_CASE( "cv_bridgeTest/testCvtColorRgbaToRgb", "[unit]" )
{
	Image rgba(1, 1, 4);
	rgba.ptr(0)[0] = 10; rgba.ptr(0)[1] = 20; rgba.ptr(0)[2] = 30; rgba.ptr(0)[3] = 255;

	Image rgb;
	cv_bridge::cvt_color(rgba, rgb, cv_bridge::COLOR_RGBA2RGB);
	assertEquals( 1u, rgb.width );
	assertEquals( 1u, rgb.height );
	assertEquals( 3u, rgb.channels() );
	assertEquals( (uint8_t)10, rgb.ptr(0)[0] );
	assertEquals( (uint8_t)20, rgb.ptr(0)[1] );
	assertEquals( (uint8_t)30, rgb.ptr(0)[2] );
}

TEST_CASE( "cv_bridgeTest/testMean", "[unit]" )
{
	Image img(2, 2, 3, 100);
	auto m = cv_bridge::mean(img);
	assertAlmostEquals( 100.0, m[0] );
	assertAlmostEquals( 100.0, m[1] );
	assertAlmostEquals( 100.0, m[2] );
}

TEST_CASE( "cv_bridgeTest/testMeanRoi", "[unit]" )
{
	Image img(4, 4, 3, 0);
	// fill top-left 2x2 with 200
	for (unsigned y = 0; y < 2; ++y)
		for (unsigned x = 0; x < 2; ++x)
			for (unsigned c = 0; c < 3; ++c)
				img.ptr(y)[x * 3 + c] = 200;

	auto m = cv_bridge::mean_roi(img, 0, 0, 2, 2);
	assertAlmostEquals( 200.0, m[0] );
	assertAlmostEquals( 200.0, m[1] );
	assertAlmostEquals( 200.0, m[2] );
}

TEST_CASE( "cv_bridgeTest/testResize", "[unit]" )
{
	Image src(8, 8, 1, 128);
	Image dst;
	cv_bridge::resize(src, dst, 4, 4);
	assertFalse( dst.empty() );
	assertEquals( 4u, dst.width );
	assertEquals( 4u, dst.height );
	assertEquals( 1u, dst.channels() );
}

TEST_CASE( "cv_bridgeTest/testCopyTo", "[unit]" )
{
	Image src(2, 2, 3, 42);
	Image dst = cv_bridge::create(4, 4, 3, 0);
	cv_bridge::copy_to(src, dst, 1, 1);
	// check that pixel (1,1) in dst is 42
	assertEquals( (uint8_t)42, dst.ptr(1)[1 * 3 + 0] );
	assertEquals( (uint8_t)42, dst.ptr(1)[1 * 3 + 1] );
	assertEquals( (uint8_t)42, dst.ptr(1)[1 * 3 + 2] );
	// check that pixel (0,0) in dst is still 0
	assertEquals( (uint8_t)0, dst.ptr(0)[0] );
}

TEST_CASE( "cv_bridgeTest/testGaussianBlur", "[unit]" )
{
	Image src(8, 8, 1, 128);
	Image dst;
	cv_bridge::gaussian_blur(src, dst, 3);
	assertFalse( dst.empty() );
	assertEquals( 8u, dst.width );
	assertEquals( 8u, dst.height );
}

TEST_CASE( "cv_bridgeTest/testThresholdOtsu", "[unit]" )
{
	Image src(8, 8, 1);
	// fill with gradient
	for (unsigned i = 0; i < 64; ++i)
		src.data[i] = i * 4;
	Image dst;
	cv_bridge::threshold_otsu(src, dst);
	assertFalse( dst.empty() );
	assertEquals( 1u, dst.channels() );
}

TEST_CASE( "cv_bridgeTest/testAdaptiveThreshold", "[unit]" )
{
	Image src(8, 8, 1, 128);
	Image dst;
	cv_bridge::adaptive_threshold(src, dst, 255, 5, -10);
	assertFalse( dst.empty() );
	assertEquals( 1u, dst.channels() );
}

TEST_CASE( "cv_bridgeTest/testFilter2D", "[unit]" )
{
	Image src(8, 8, 1, 128);
	Image dst;
	float kernel[9] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
	cv_bridge::filter2D(src, dst, kernel, 3);
	assertFalse( dst.empty() );
	assertEquals( 8u, dst.width );
	assertEquals( 8u, dst.height );
}

TEST_CASE( "cv_bridgeTest/testWarpPerspective", "[unit]" )
{
	Image src(100, 100, 3, 0);
	// identity transform
	cv_bridge::Point2f src_pts[4] = {{0, 0}, {99, 0}, {0, 99}, {99, 99}};
	cv_bridge::Point2f dst_pts[4] = {{0, 0}, {99, 0}, {0, 99}, {99, 99}};
	Image dst;
	cv_bridge::warp_perspective(src, dst, src_pts, dst_pts, 100, 100);
	assertFalse( dst.empty() );
	assertEquals( 100u, dst.width );
	assertEquals( 100u, dst.height );
}

TEST_CASE( "cv_bridgeTest/testMat3x3Multiply", "[unit]" )
{
	// identity * identity = identity
	cv_bridge::Mat3x3 I = {1,0,0, 0,1,0, 0,0,1};
	cv_bridge::Mat3x3 result = cv_bridge::mat3x3_multiply(I, I);
	for (int i = 0; i < 9; ++i)
	{
		REQUIRE( result[i] >= I[i] - 1 );
		REQUIRE( result[i] <= I[i] + 1 );
	}
}

TEST_CASE( "cv_bridgeTest/testMat3x3MultiplyVec", "[unit]" )
{
	cv_bridge::Mat3x3 I = {1,0,0, 0,1,0, 0,0,1};
	std::array<float, 3> v = {1.0f, 2.0f, 3.0f};
	auto r = cv_bridge::mat3x3_multiply_vec(I, v);
	assertAlmostEquals( 1.0f, r[0] );
	assertAlmostEquals( 2.0f, r[1] );
	assertAlmostEquals( 3.0f, r[2] );
}

TEST_CASE( "cv_bridgeTest/testMat3x3Diag", "[unit]" )
{
	std::array<float, 3> d = {2.0f, 3.0f, 4.0f};
	cv_bridge::Mat3x3 m = cv_bridge::mat3x3_diag(d);
	assertAlmostEquals( 2.0f, m[0] );
	assertAlmostEquals( 0.0f, m[1] );
	assertAlmostEquals( 0.0f, m[2] );
	assertAlmostEquals( 0.0f, m[3] );
	assertAlmostEquals( 3.0f, m[4] );
	assertAlmostEquals( 0.0f, m[5] );
	assertAlmostEquals( 0.0f, m[6] );
	assertAlmostEquals( 0.0f, m[7] );
	assertAlmostEquals( 4.0f, m[8] );
}

TEST_CASE( "cv_bridgeTest/testMat3x3Inv", "[unit]" )
{
	// inv of identity is identity
	cv_bridge::Mat3x3 I = {1,0,0, 0,1,0, 0,0,1};
	cv_bridge::Mat3x3 inv = cv_bridge::mat3x3_inv(I);
	for (int i = 0; i < 9; ++i)
	{
		REQUIRE( inv[i] >= I[i] - 1 );
		REQUIRE( inv[i] <= I[i] + 1 );
	}
}

TEST_CASE( "cv_bridgeTest/testMat3x3Transpose", "[unit]" )
{
	cv_bridge::Mat3x3 m = {1,2,3, 4,5,6, 7,8,9};
	cv_bridge::Mat3x3 t = cv_bridge::mat3x3_transpose(m);
	// row 0 col 1 should become row 1 col 0
	assertAlmostEquals( 1.0f, t[0] );
	assertAlmostEquals( 4.0f, t[1] );
	assertAlmostEquals( 7.0f, t[2] );
	assertAlmostEquals( 2.0f, t[3] );
	assertAlmostEquals( 5.0f, t[4] );
	assertAlmostEquals( 8.0f, t[5] );
	assertAlmostEquals( 3.0f, t[6] );
	assertAlmostEquals( 6.0f, t[7] );
	assertAlmostEquals( 9.0f, t[8] );
}

TEST_CASE( "cv_bridgeTest/testMoorePenroseLsm", "[unit]" )
{
	// simple case: identity transform
	// actual == desired, so result should be close to identity
	float actual[] = {
		1, 0, 0,
		0, 1, 0,
		0, 0, 1,
		0.5f, 0.5f, 0.5f,
	};
	float desired[] = {
		1, 0, 0,
		0, 1, 0,
		0, 0, 1,
		0.5f, 0.5f, 0.5f,
	};
	cv_bridge::Mat3x3 result = cv_bridge::moore_penrose_lsm(actual, desired, 4);
	// should be close to identity
	assertAlmostEquals( 1.0f, result[0] );
	assertAlmostEquals( 0.0f, result[1] );
	assertAlmostEquals( 0.0f, result[2] );
	assertAlmostEquals( 0.0f, result[3] );
	assertAlmostEquals( 1.0f, result[4] );
	assertAlmostEquals( 0.0f, result[5] );
	assertAlmostEquals( 0.0f, result[6] );
	assertAlmostEquals( 0.0f, result[7] );
	assertAlmostEquals( 1.0f, result[8] );
}

TEST_CASE( "cv_bridgeTest/testFloatMatrix", "[unit]" )
{
	cv_bridge::FloatMatrix m;
	m.cols = 3;
	float row1[] = {1, 2, 3};
	float row2[] = {4, 5, 6};
	m.push_back_row(row1);
	m.push_back_row(row2);
	assertEquals( 2u, m.rows );
	assertEquals( 3u, m.cols );
	assertAlmostEquals( 1.0f, m.at(0, 0) );
	assertAlmostEquals( 5.0f, m.at(1, 1) );
}

TEST_CASE( "cv_bridgeTest/testImreadImwrite", "[unit]" )
{
	// create a small image, write it, read it back
	Image img(4, 4, 3, 100);
	std::string path = "/tmp/cv_bridge_test_imread_imwrite.png";
	assertTrue( cv_bridge::imwrite(path, img) );

	Image loaded = cv_bridge::imread(path);
	assertFalse( loaded.empty() );
	assertEquals( 4u, loaded.width );
	assertEquals( 4u, loaded.height );
	assertEquals( 3u, loaded.channels() );
	// pixel values should be close (lossy PNG compression might differ slightly)
	assertAlmostEquals( 100, (int)loaded.ptr(0)[0] );
}

TEST_CASE( "ImageTest/testCloneRoi", "[unit]" )
{
	// Regression: clone() on ROI must copy row-by-row respecting source stride
	Image parent(10, 10, 1, 0);
	for (unsigned i = 0; i < 100; ++i)
		parent.data[i] = i;

	Image roi = parent.roi(2, 3, 4, 5);
	assertEquals( 4u, roi.width );
	assertEquals( 5u, roi.height );
	assertTrue( roi.stride > roi.width ); // stride is parent's stride

	Image cloned = roi.clone();
	assertTrue( cloned.owns_data );
	assertEquals( 4u, cloned.width );
	assertEquals( 5u, cloned.height );
	assertEquals( 4u, cloned.stride ); // stride = width * channels for owned image

	// verify pixel data matches ROI
	for (unsigned y = 0; y < 5; ++y)
		for (unsigned x = 0; x < 4; ++x)
			assertEquals( roi.ptr(y)[x], cloned.ptr(y)[x] );
}

TEST_CASE( "cv_bridgeTest/testRemap", "[unit]" )
{
	// identity distortion map
	double camera[] = {100, 0, 50, 0, 100, 50, 0, 0, 1};
	double dist[] = {0, 0, 0, 0};
	cv_bridge::DistortionMap dm = cv_bridge::init_undistort_rectify_map(camera, dist, 100, 100);
	assertFalse( dm.empty() );
	assertEquals( 100u, dm.width );
	assertEquals( 100u, dm.height );

	Image src = cv_bridge::create(100, 100, 3, 128);
	Image dst;
	cv_bridge::remap(src, dst, dm);
	assertFalse( dst.empty() );
	assertEquals( 100u, dst.width );
	assertEquals( 100u, dst.height );
}

TEST_CASE( "bitmatrixTest/testMatToBitbufferRemainder", "[unit]" )
{
	// Regression: remainder packing must use MSB-first bit positions
	// 10x10 binary image has 100 pixels = 12 full bytes + 4 remainder pixels
	Image img(10, 10, 1, 0);
	// set all pixels to 255 (binary 1)
	for (unsigned i = 0; i < 100; ++i)
		img.data[i] = 255;

	bitbuffer bb(100);
	bitmatrix::mat_to_bitbuffer(img, bb.get_writer());

	// all 100 bits should be 1
	// first 12 bytes (96 bits) should be 0xFF
	for (int i = 0; i < 12; ++i)
		assertEquals( (uint8_t)0xFF, (uint8_t)bb.buffer()[i] );

	// remainder byte: 4 pixels at MSB positions (bits 7,6,5,4) = 0xF0
	assertEquals( (uint8_t)0xF0, (uint8_t)bb.buffer()[12] );
}
