/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "cv_bridge.h"
#include <opencv2/opencv.hpp>

namespace cv_bridge
{

// ====== 内部辅助 ======

static int to_cv_type(unsigned channels)
{
	return CV_MAKETYPE(CV_8U, channels);
}

static cv::Mat to_cv(const Image& img)
{
	if (img.empty())
		return cv::Mat();
	return cv::Mat(img.height, img.width, to_cv_type(img.channels()), img.data, img.stride);
}

static Image from_cv_mat(const cv::Mat& mat)
{
	if (mat.empty())
		return {};
	cv::Mat continuous = mat.isContinuous() ? mat : mat.clone();
	Image img(continuous.cols, continuous.rows, continuous.channels());
	std::memcpy(img.data, continuous.data, img.total());
	return img;
}

static cv::ColorConversionCodes to_cv_color(ColorCode code)
{
	switch (code)
	{
	case COLOR_BGR2RGB: return cv::COLOR_BGR2RGB;
	case COLOR_RGB2BGR: return cv::COLOR_RGB2BGR;
	case COLOR_RGBA2RGB: return cv::COLOR_RGBA2RGB;
	case COLOR_BGRA2RGB: return cv::COLOR_BGRA2RGB;
	case COLOR_GRAY2RGB: return cv::COLOR_GRAY2RGB;
	case COLOR_RGB2GRAY: return cv::COLOR_RGB2GRAY;
	case COLOR_RGB2RGBA: return cv::COLOR_RGB2RGBA;
	default: return cv::COLOR_BGR2RGB;
	}
}

// ====== 转换 ======

Image from_cv(const uint8_t* data, unsigned w, unsigned h, unsigned channels, unsigned stride)
{
	Image img;
	img.data = const_cast<uint8_t*>(data);
	img.width = w;
	img.height = h;
	// ponytail: 直接设置 _channels 通过构造
	img = Image(const_cast<uint8_t*>(data), w, h, channels);
	img.stride = stride; // 覆盖默认 stride
	return img;
}

Image from_cv_clone(const uint8_t* data, unsigned w, unsigned h, unsigned channels)
{
	Image img(w, h, channels);
	std::memcpy(img.data, data, img.total());
	return img;
}

Image mat_to_image(const void* mat_ptr)
{
	const cv::Mat& mat = *static_cast<const cv::Mat*>(mat_ptr);
	if (mat.empty())
		return {};
	return from_cv_mat(mat);
}

void image_to_mat(const Image& img, void* mat_out)
{
	cv::Mat& out = *static_cast<cv::Mat*>(mat_out);
	out = to_cv(img).clone();
}

// ====== I/O ======

Image imread(const std::string& path)
{
	cv::Mat bgr = cv::imread(path, cv::IMREAD_COLOR);
	if (bgr.empty())
		return {};
	cv::Mat rgb;
	cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
	return from_cv_mat(rgb);
}

bool imwrite(const std::string& path, const Image& img)
{
	if (img.empty())
		return false;
	cv::Mat bgr;
	cv::Mat rgb = to_cv(img);
	cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
	return cv::imwrite(path, bgr);
}

// ====== 颜色转换 ======

void cvt_color(const Image& src, Image& dst, ColorCode code)
{
	cv::Mat cv_src = to_cv(src);
	cv::Mat cv_dst;
	cv::cvtColor(cv_src, cv_dst, to_cv_color(code));
	dst = from_cv_mat(cv_dst);
}

void cvt_color(const cv::Mat& src, Image& dst, ColorCode code)
{
	cv::Mat cv_dst;
	cv::cvtColor(src, cv_dst, to_cv_color(code));
	dst = from_cv_mat(cv_dst);
}

void cvt_color(const Image& src, cv::Mat& dst, ColorCode code)
{
	cv::Mat cv_src = to_cv(src);
	cv::cvtColor(cv_src, dst, to_cv_color(code));
}

// ====== 阈值 ======

void threshold_otsu(const Image& src, Image& dst)
{
	cv::Mat cv_src = to_cv(src);
	cv::Mat cv_dst;
	cv::threshold(cv_src, cv_dst, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
	dst = from_cv_mat(cv_dst);
}

void threshold_otsu(const cv::Mat& src, Image& dst)
{
	cv::Mat cv_dst;
	cv::threshold(src, cv_dst, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
	dst = from_cv_mat(cv_dst);
}

void adaptive_threshold(const Image& src, Image& dst,
                        double maxval, int block_size, double C)
{
	cv::Mat cv_src = to_cv(src);
	cv::Mat cv_dst;
	cv::adaptiveThreshold(cv_src, cv_dst, maxval, cv::ADAPTIVE_THRESH_MEAN_C,
	                      cv::THRESH_BINARY, block_size, C);
	dst = from_cv_mat(cv_dst);
}

void adaptive_threshold(const cv::Mat& src, Image& dst,
                        double maxval, int block_size, double C)
{
	cv::Mat cv_dst;
	cv::adaptiveThreshold(src, cv_dst, maxval, cv::ADAPTIVE_THRESH_MEAN_C,
	                      cv::THRESH_BINARY, block_size, C);
	dst = from_cv_mat(cv_dst);
}

// ====== 滤波 ======

void gaussian_blur(const Image& src, Image& dst, int ksize)
{
	cv::Mat cv_src = to_cv(src);
	cv::Mat cv_dst;
	cv::GaussianBlur(cv_src, cv_dst, cv::Size(ksize, ksize), 0);
	dst = from_cv_mat(cv_dst);
}

void gaussian_blur(const cv::Mat& src, Image& dst, int ksize)
{
	cv::Mat cv_dst;
	cv::GaussianBlur(src, cv_dst, cv::Size(ksize, ksize), 0);
	dst = from_cv_mat(cv_dst);
}

Image clone_from_mat(const cv::Mat& mat)
{
	return from_cv_mat(mat);
}

void filter2D(const Image& src, Image& dst, const float* kernel_data, int ksize)
{
	cv::Mat cv_src = to_cv(src);
	cv::Mat kernel(ksize, ksize, CV_32F, const_cast<float*>(kernel_data));
	cv::Mat cv_dst;
	cv::filter2D(cv_src, cv_dst, -1, kernel);
	dst = from_cv_mat(cv_dst);
}

// ====== 几何变换 ======

void warp_perspective(const Image& src, Image& dst,
                      const Point2f* src_pts, const Point2f* dst_pts,
                      unsigned out_w, unsigned out_h,
                      InterpFlags interp)
{
	std::vector<cv::Point2f> cv_src(4), cv_dst(4);
	for (int i = 0; i < 4; ++i)
	{
		cv_src[i] = cv::Point2f(src_pts[i].x, src_pts[i].y);
		cv_dst[i] = cv::Point2f(dst_pts[i].x, dst_pts[i].y);
	}

	cv::Mat transform = cv::getPerspectiveTransform(cv_src, cv_dst);
	cv::Mat cv_src_mat = to_cv(src);
	cv::Mat cv_dst_mat;
	cv::warpPerspective(cv_src_mat, cv_dst_mat, transform, cv::Size(out_w, out_h),
	                    interp == INTER_LINEAR ? cv::INTER_LINEAR : cv::INTER_LINEAR);
	dst = from_cv_mat(cv_dst_mat);
}

// ====== 畸变校正 ======

DistortionMap init_undistort_rectify_map(
    const double* camera_matrix, const double* dist_coeffs,
    unsigned width, unsigned height)
{
	cv::Mat camera(3, 3, CV_64F, const_cast<double*>(camera_matrix));
	cv::Mat dist(1, 4, CV_64F, const_cast<double*>(dist_coeffs));
	cv::Mat map1, map2;
	cv::initUndistortRectifyMap(camera, dist, cv::Mat(), camera,
	                            cv::Size(width, height), CV_32FC1, map1, map2);

	DistortionMap dm;
	dm.width = width;
	dm.height = height;
	size_t map_size = width * height;
	dm.map1_data.resize(map_size);
	dm.map2_data.resize(map_size);
	std::memcpy(dm.map1_data.data(), map1.data, map_size * sizeof(float));
	std::memcpy(dm.map2_data.data(), map2.data, map_size * sizeof(float));
	return dm;
}

void remap(const Image& src, Image& dst, const DistortionMap& maps,
           InterpFlags interp, BorderMode border)
{
	cv::Mat cv_src = to_cv(src);
	cv::Mat cv_map1(maps.height, maps.width, CV_32FC1, const_cast<float*>(maps.map1_data.data()));
	cv::Mat cv_map2(maps.height, maps.width, CV_32FC1, const_cast<float*>(maps.map2_data.data()));
	cv::Mat cv_dst;
	cv::remap(cv_src, cv_dst, cv_map1, cv_map2,
	          interp == INTER_LINEAR ? cv::INTER_LINEAR : cv::INTER_LINEAR,
	          border == BORDER_CONSTANT ? cv::BORDER_CONSTANT : cv::BORDER_CONSTANT);
	dst = from_cv_mat(cv_dst);
}

// ====== 统计 ======

std::array<double, 4> mean(const Image& img)
{
	if (img.empty())
		return {0, 0, 0, 0};
	cv::Mat cv_img = to_cv(img);
	cv::Scalar m = cv::mean(cv_img);
	return {m[0], m[1], m[2], m[3]};
}

std::array<double, 4> mean_roi(const Image& img, unsigned x, unsigned y, unsigned w, unsigned h)
{
	if (img.empty())
		return {0, 0, 0, 0};
	cv::Mat cv_img = to_cv(img);
	cv::Mat roi = cv_img(cv::Rect(x, y, w, h));
	cv::Scalar m = cv::mean(roi);
	return {m[0], m[1], m[2], m[3]};
}

// ====== 缩放 ======

void resize(const Image& src, Image& dst, unsigned w, unsigned h)
{
	cv::Mat cv_src = to_cv(src);
	cv::Mat cv_dst;
	cv::resize(cv_src, cv_dst, cv::Size(w, h));
	dst = from_cv_mat(cv_dst);
}

// ====== 像素操作 ======

void copy_to(const Image& src, Image& dst, unsigned x, unsigned y)
{
	if (src.empty() || dst.empty())
		return;
	cv::Mat cv_src = to_cv(src);
	cv::Mat cv_dst = to_cv(dst);
	cv::Mat roi = cv_dst(cv::Rect(x, y, src.width, src.height));
	cv_src.copyTo(roi);
}

Image create(unsigned w, unsigned h, unsigned channels, uint8_t fill)
{
	return Image(w, h, channels, fill);
}

// ====== 矩阵运算 ======

Mat3x3 mat3x3_multiply(const Mat3x3& a, const Mat3x3& b)
{
	Mat3x3 r = {};
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			for (int k = 0; k < 3; ++k)
				r[i * 3 + j] += a[i * 3 + k] * b[k * 3 + j];
	return r;
}

std::array<float, 3> mat3x3_multiply_vec(const Mat3x3& m, const std::array<float, 3>& v)
{
	std::array<float, 3> r = {};
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			r[i] += m[i * 3 + j] * v[j];
	return r;
}

Mat3x3 mat3x3_diag(const std::array<float, 3>& d)
{
	Mat3x3 r = {};
	r[0] = d[0];
	r[4] = d[1];
	r[8] = d[2];
	return r;
}

Mat3x3 mat3x3_transpose(const Mat3x3& m)
{
	Mat3x3 r;
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			r[i * 3 + j] = m[j * 3 + i];
	return r;
}

Mat3x3 mat3x3_inv(const Mat3x3& m)
{
	// 3x3 矩阵求逆（伴随矩阵法）
	float a = m[0], b = m[1], c = m[2];
	float d = m[3], e = m[4], f = m[5];
	float g = m[6], h = m[7], i = m[8];

	float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
	if (det == 0.0f)
		return {}; // 奇异矩阵

	float inv_det = 1.0f / det;
	Mat3x3 r;
	r[0] = (e * i - f * h) * inv_det;
	r[1] = (c * h - b * i) * inv_det;
	r[2] = (b * f - c * e) * inv_det;
	r[3] = (f * g - d * i) * inv_det;
	r[4] = (a * i - c * g) * inv_det;
	r[5] = (c * d - a * f) * inv_det;
	r[6] = (d * h - e * g) * inv_det;
	r[7] = (b * g - a * h) * inv_det;
	r[8] = (a * e - b * d) * inv_det;
	return r;
}

Mat3x3 moore_penrose_lsm(const float* actual, const float* desired, int n)
{
	// 使用 OpenCV 的 SVD 求伪逆
	// actual: Nx3, desired: Nx3
	// 计算 desired^T * pinv(actual^T)
	cv::Mat cv_actual(n, 3, CV_32F, const_cast<float*>(actual));
	cv::Mat cv_desired(n, 3, CV_32F, const_cast<float*>(desired));

	cv::Mat x, y, z;
	cv::transpose(cv_desired, x);
	cv::transpose(cv_actual, y);
	cv::invert(y, z, cv::DECOMP_SVD);

	cv::Mat result = x * z;

	// 复制回 Mat3x3（3x3 矩阵，行优先）
	Mat3x3 r;
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			r[i * 3 + j] = result.at<float>(i, j);
	return r;
}

// ====== FloatMatrix ======

void FloatMatrix::push_back_row(const float* row)
{
	data.insert(data.end(), row, row + cols);
	++rows;
}

float* FloatMatrix::row_ptr(unsigned r)
{
	return data.data() + r * cols;
}

const float* FloatMatrix::row_ptr(unsigned r) const
{
	return data.data() + r * cols;
}

float FloatMatrix::at(unsigned r, unsigned c) const
{
	return data[r * cols + c];
}

FloatMatrix make_float_matrix(const double* src, unsigned rows, unsigned cols)
{
	FloatMatrix m(rows, cols);
	for (unsigned i = 0; i < rows * cols; ++i)
		m.data[i] = static_cast<float>(src[i]);
	return m;
}

}
