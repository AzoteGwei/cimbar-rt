#include <libcimbar/cimbar.h>

#include <cstdint>
#include <cstdlib>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	// Need: 4 bytes dims + at least 4*4 RGBA pixels
	if (Size < 4 + 64) return 0;

	unsigned w = (Data[0] | (Data[1] << 8)) % 256 + 4;
	unsigned h = (Data[2] | (Data[3] << 8)) % 256 + 4;

	const uint8_t* pixels = Data + 4;
	size_t pixel_size = Size - 4;

	// Must have enough for RGBA (4 bpp)
	size_t min_needed = (size_t)w * h * (size_t)4;
	if (pixel_size < min_needed)
		return 0;

	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	if (!dec) return 0;

	cimbar_image_format_t formats[] = {
		CIMBAR_IMAGE_RGB,
		CIMBAR_IMAGE_RGBA,
		CIMBAR_IMAGE_BGR,
		CIMBAR_IMAGE_BGRA,
		CIMBAR_IMAGE_GRAY,
	};

	for (auto fmt : formats)
	{
		uint8_t output[4096] = {};
		size_t out_len = sizeof(output);
		cimbar_decoder_scan(dec, pixels, pixel_size, w, h, fmt, output, &out_len);
	}

	cimbar_decoder_destroy(dec);
	return 0;
}
