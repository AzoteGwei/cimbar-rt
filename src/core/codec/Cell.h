/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "support/image/Image.h"
#include <opencv2/opencv.hpp>

#include <tuple>

class Cell
{
public:
	static const bool SKIP = true;

public:
	Cell(const Image& img)
		: _data(img.ptr(0))
		, _img_stride(img.stride)
		, _img_width(img.width)
		, _img_height(img.height)
		, _img_channels(img.channels())
		, _xstart(0)
		, _ystart(0)
		, _cols(img.width)
		, _rows(img.height)
	{
	}

	Cell(const Image& img, int xstart, int ystart, int cols, int rows)
		: _data(img.ptr(0))
		, _img_stride(img.stride)
		, _img_width(img.width)
		, _img_height(img.height)
		, _img_channels(img.channels())
		, _xstart(xstart)
		, _ystart(ystart)
		, _cols(cols)
		, _rows(rows)
	{}

	Cell(const cv::Mat& img)
		: _data(img.ptr<uchar>(0))
		, _img_stride(img.step[0])
		, _img_width(img.cols)
		, _img_height(img.rows)
		, _img_channels(img.channels())
		, _xstart(0)
		, _ystart(0)
		, _cols(img.cols)
		, _rows(img.rows)
	{
	}

	Cell(const cv::Mat& img, int xstart, int ystart, int cols, int rows)
		: _data(img.ptr<uchar>(0))
		, _img_stride(img.step[0])
		, _img_width(img.cols)
		, _img_height(img.rows)
		, _img_channels(img.channels())
		, _xstart(xstart)
		, _ystart(ystart)
		, _cols(cols)
		, _rows(rows)
	{}

	std::tuple<uchar,uchar,uchar> mean_rgb_continuous(bool skip) const
	{
		uint16_t blue = 0;
		uint16_t green = 0;
		uint16_t red = 0;
		uint16_t count = 0;

		int channels = _img_channels;
		int index = (_ystart * _img_width) + _xstart;
		const uchar* p = _data + (index * channels);

		int increment = 1 + skip;
		int toNextRow = channels * (_img_width - _cols);
		if (skip)
			toNextRow += channels * _img_width;

		for (int i = 0; i < _rows; i+=increment)
		{
			for (int j = 0; j < _cols; ++j, ++count)
			{
				red += p[0];
				green += p[1];
				blue += p[2];
				p += channels;
			}
			p += toNextRow;
		}

		if (!count)
			return std::tuple<uchar,uchar,uchar>(0, 0, 0);

		return std::tuple<uchar,uchar,uchar>(red/count, green/count, blue/count);
	}

	std::tuple<uchar,uchar,uchar> mean_rgb(bool skip=false) const
	{
		int channels = _img_channels;
		if (channels < 3)
			return std::tuple<uchar,uchar,uchar>(0, 0, 0);
		if (_img_stride == _img_width * _img_channels && _cols > 0)
			return mean_rgb_continuous(skip);

		uint16_t blue = 0;
		uint16_t green = 0;
		uint16_t red = 0;
		uint16_t count = 0;

		int increment = 1 + skip;
		int yend = _img_height * _img_channels;
		for (int i = 0; i < _img_width; i+=increment)
		{
			const uchar* p = _data + i * _img_stride;
			for (int j = 0; j < yend; j+=_img_channels, ++count)
			{
				red += p[j];
				green += p[j+1];
				blue += p[j+2];
			}
		}

		if (!count)
			return std::tuple<uchar,uchar,uchar>(0, 0, 0);

		return std::tuple<uchar,uchar,uchar>(red/count, green/count, blue/count);
	}

	uchar mean_grayscale_continuous() const
	{
		uint16_t total = 0;
		uint16_t count = 0;

		int index = (_ystart * _img_width) + _xstart;
		const uchar* p = _data + index;
		int toNextCol = _img_height - _rows;

		for (int i = 0; i < _img_width; ++i)
		{
			for (int j = 0; j < _img_height; ++j, ++count)
				total += p[count];
			count += toNextCol;
		}

		if (!count)
			return 0;
		return (uchar)(total/count);
	}

	uchar mean_grayscale() const
	{
		if (_img_channels > 1)
			return 0;
		if (_img_stride == _img_width * _img_channels && _cols > 0)
			return mean_grayscale_continuous();

		uint16_t total = 0;
		uint16_t count = 0;

		for (int i = 0; i < _img_width; ++i)
		{
			const uchar* p = _data + i * _img_stride;
			for (int j = 0; j < _img_height; ++j, ++count)
				total += p[j];
		}

		if (!count)
			return 0;
		return (uchar)(total/count);
	}

	void crop(int x, int y, int cols, int rows)
	{
		_xstart += x;
		_ystart += y;
		_cols = cols;
		_rows = rows;
	}

	int cols() const
	{
		return _cols;
	}

	int rows() const
	{
		return _rows;
	}

protected:
	const uchar* _data;
	int _img_stride;
	int _img_width;
	int _img_height;
	int _img_channels;
	int _xstart = 0;
	int _ystart = 0;
	int _cols;
	int _rows;
};
