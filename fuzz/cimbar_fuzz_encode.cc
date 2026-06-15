#include <libcimbar/cimbar.h>

#include <cstdint>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	if (Size == 0 || Size > 1024 * 100) return 0;

	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	if (!enc) return 0;

	cimbar_encoder_set_input(enc, Data, Size, "fuzz.bin");

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;

	for (int i = 0; i < 10; ++i)
	{
		int ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
		if (ret <= 0) break;
	}

	cimbar_encoder_destroy(enc);
	return 0;
}
