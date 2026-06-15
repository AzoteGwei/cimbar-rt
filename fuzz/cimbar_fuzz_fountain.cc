#include <libcimbar/cimbar.h>
#include "core/fountain/FountainInit.h"

#include <cstdint>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	FountainInit::init();

	if (Size < 4 + 64) return 0;

	unsigned w = (Data[0] | (Data[1] << 8)) % 256 + 4;
	unsigned h = (Data[2] | (Data[3] << 8)) % 256 + 4;

	const uint8_t* pixels = Data + 4;
	size_t pixel_size = Size - 4;

	size_t min_needed = (size_t)w * h * (size_t)4;
	if (pixel_size < min_needed)
		return 0;

	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	if (!dec) return 0;

	cimbar_decoder_fountain_feed(dec, pixels, pixel_size, w, h, CIMBAR_IMAGE_RGBA);

	if (cimbar_decoder_fountain_is_complete(dec))
	{
		size_t file_size = cimbar_decoder_fountain_get_filesize(dec);
		if (file_size > 0 && file_size < 1024 * 1024)
		{
			std::vector<uint8_t> result(file_size);
			size_t out_len = result.size();
			cimbar_decoder_fountain_read(dec, result.data(), &out_len);
		}
	}

	cimbar_decoder_destroy(dec);
	return 0;
}
