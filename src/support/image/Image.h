/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <utility>

class Image
{
public:
	uint8_t* data = nullptr;
	unsigned width = 0;
	unsigned height = 0;
	unsigned stride = 0;
	bool owns_data = false;

	// duck-typing: .cols/.rows 作为数据成员兼容 cv::Mat 模板
	unsigned& cols = width;
	unsigned& rows = height;

	unsigned channels() const { return _channels; }

	Image() = default;

	Image(unsigned w, unsigned h, unsigned ch, uint8_t fill = 0)
		: width(w)
		, height(h)
		, _channels(ch)
		, stride(w * ch)
		, owns_data(true)
	{
		data = new uint8_t[stride * height];
		std::memset(data, fill, stride * height);
	}

	Image(uint8_t* d, unsigned w, unsigned h, unsigned ch)
		: data(d)
		, width(w)
		, height(h)
		, _channels(ch)
		, stride(w * ch)
		, owns_data(false)
	{
	}

	~Image()
	{
		if (owns_data)
			delete[] data;
	}

	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;

	Image(Image&& o) noexcept
		: data(o.data)
		, width(o.width)
		, height(o.height)
		, _channels(o._channels)
		, stride(o.stride)
		, owns_data(o.owns_data)
	{
		o.data = nullptr;
		o.owns_data = false;
	}

	Image& operator=(Image&& o) noexcept
	{
		if (this != &o)
		{
			if (owns_data)
				delete[] data;
			data = o.data;
			width = o.width;
			height = o.height;
			_channels = o._channels;
			stride = o.stride;
			owns_data = o.owns_data;
			o.data = nullptr;
			o.owns_data = false;
		}
		return *this;
	}

	uint8_t* ptr(unsigned row)
	{
		return data + row * stride;
	}

	const uint8_t* ptr(unsigned row) const
	{
		return data + row * stride;
	}

	bool empty() const
	{
		return !data;
	}

	unsigned total() const
	{
		return stride * height;
	}

	// duck-typing compatibility with cv::Mat templates
	unsigned isContinuous() const { return true; }
	unsigned type() const { return 0; } // ponytail: 仅供 Deskewer 模板兼容

	Image clone() const
	{
		if (empty())
			return {};
		Image out(width, height, _channels);
		unsigned row_bytes = width * _channels;
		if (stride == row_bytes)
			std::memcpy(out.data, data, row_bytes * height);
		else
			for (unsigned i = 0; i < height; ++i)
				std::memcpy(out.data + i * row_bytes, data + i * stride, row_bytes);
		return out;
	}

	Image roi(unsigned x, unsigned y, unsigned w, unsigned h) const
	{
		// ponytail: ROI 不拥有数据，调用者确保原 Image 生命周期
		Image sub;
		sub.data = data + y * stride + x * _channels;
		sub.width = w;
		sub.height = h;
		sub._channels = _channels;
		sub.stride = stride;
		sub.owns_data = false;
		return sub;
	}

private:
	unsigned _channels = 0;
};
