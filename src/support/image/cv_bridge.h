/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Image.h"
#include <opencv2/opencv.hpp>
#include <array>
#include <string>
#include <vector>

namespace cv_bridge
{
	// ====== 类型替代 ======
	struct Point2f
	{
		float x = 0;
		float y = 0;
		Point2f() = default;
		Point2f(float x_, float y_) : x(x_), y(y_) {}
	};

	// ====== 颜色转换枚举 ======
	enum ColorCode
	{
		COLOR_BGR2RGB = 0,
		COLOR_RGB2BGR,
		COLOR_RGBA2RGB,
		COLOR_BGRA2RGB,
		COLOR_GRAY2RGB,
		COLOR_RGB2GRAY,
		COLOR_RGB2RGBA,
	};

	// ====== 插值方法 ======
	enum InterpFlags
	{
		INTER_LINEAR = 0,
	};

	// ====== 边界模式 ======
	enum BorderMode
	{
		BORDER_CONSTANT = 0,
	};

	// ====== 转换 ======
	Image from_cv_clone(const uint8_t* data, unsigned w, unsigned h, unsigned channels);

	// ====== I/O ======
#ifndef __EMSCRIPTEN__
	Image imread(const std::string& path);
	bool imwrite(const std::string& path, const Image& img);
#endif

	// ====== 颜色转换 ======
	void cvt_color(const Image& src, Image& dst, ColorCode code);

	// cv::Mat 兼容重载（模板实例化需要）
	void cvt_color(const cv::Mat& src, Image& dst, ColorCode code);
	void cvt_color(const Image& src, cv::Mat& dst, ColorCode code);

	// ====== 阈值 ======
	void threshold_otsu(const Image& src, Image& dst);
	void adaptive_threshold(const Image& src, Image& dst,
	                        double maxval, int block_size, double C);

	// cv::Mat 兼容重载
	void threshold_otsu(const cv::Mat& src, Image& dst);
	void adaptive_threshold(const cv::Mat& src, Image& dst,
	                        double maxval, int block_size, double C);

	// ====== 滤波 ======
	void gaussian_blur(const Image& src, Image& dst, int ksize);
	void filter2D(const Image& src, Image& dst, const float* kernel_data, int ksize);

	// cv::Mat 兼容重载
	void gaussian_blur(const cv::Mat& src, Image& dst, int ksize);

	// ====== 几何变换 ======
	void warp_perspective(const Image& src, Image& dst,
	                      const Point2f* src_pts, const Point2f* dst_pts,
	                      unsigned out_w, unsigned out_h,
	                      InterpFlags interp = INTER_LINEAR);

	// ====== 畸变校正 ======
	struct DistortionMap
	{
		// ponytail: 畸变映射表是 float 类型，用 vector 存储
		std::vector<float> map1_data;
		std::vector<float> map2_data;
		unsigned width = 0;
		unsigned height = 0;
		bool empty() const { return map1_data.empty(); }
	};

	DistortionMap init_undistort_rectify_map(
	    const double* camera_matrix, const double* dist_coeffs,
	    unsigned width, unsigned height);

	void remap(const Image& src, Image& dst, const DistortionMap& maps,
	           InterpFlags interp = INTER_LINEAR, BorderMode border = BORDER_CONSTANT);

	// ====== 统计 ======
	std::array<double, 4> mean(const Image& img);
	std::array<double, 4> mean_roi(const Image& img, unsigned x, unsigned y, unsigned w, unsigned h);

	// ====== 缩放 ======
	void resize(const Image& src, Image& dst, unsigned w, unsigned h);

	// ====== 像素操作 ======
	void copy_to(const Image& src, Image& dst, unsigned x, unsigned y);
	Image create(unsigned w, unsigned h, unsigned channels, uint8_t fill = 0);

	// ====== 矩阵运算（3x3 固定大小）======
	using Mat3x3 = std::array<float, 9>;

	Mat3x3 mat3x3_multiply(const Mat3x3& a, const Mat3x3& b);
	std::array<float, 3> mat3x3_multiply_vec(const Mat3x3& m, const std::array<float, 3>& v);
	Mat3x3 mat3x3_diag(const std::array<float, 3>& d);
	Mat3x3 mat3x3_inv(const Mat3x3& m);
	Mat3x3 mat3x3_transpose(const Mat3x3& m);

	// Moore-Penrose 伪逆: actual[Nx3], desired[Nx3] -> 返回 3x3
	Mat3x3 moore_penrose_lsm(const float* actual, const float* desired, int n);

	// ====== 动态矩阵（替代 cv::Mat push_back 等）======
	struct FloatMatrix
	{
		std::vector<float> data;
		unsigned rows = 0;
		unsigned cols = 0;

		FloatMatrix() = default;
		FloatMatrix(unsigned r, unsigned c) : data(r * c, 0.0f), rows(r), cols(c) {}

		void push_back_row(const float* row);
		float* row_ptr(unsigned r);
		const float* row_ptr(unsigned r) const;
		float at(unsigned r, unsigned c) const;
	};

	// 从 DistortionParameters 的 double* 构造 FloatMatrix
	FloatMatrix make_float_matrix(const double* data, unsigned rows, unsigned cols);
}
