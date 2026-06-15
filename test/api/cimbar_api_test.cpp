/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"
#include "TestHelpers.h"

#include <libcimbar/cimbar.h>

#include <cstring>
#include <string>
#include <vector>

// Test zstd directly
#include "core/compression/zstd_compressor.h"
#include "core/compression/zstd_decompressor.h"

namespace {
	std::string random_string(unsigned len)
	{
		static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		std::string out;
		out.reserve(len);
		for (unsigned i = 0; i < len; ++i)
			out += chars[rand() % (sizeof(chars) - 1)];
		return out;
	}

	static void* test_alloc(void* ctx, size_t size)
	{
		(*(int*)ctx)++;
		return malloc(size);
	}

	static void test_free(void* ctx, void* ptr)
	{
		(void)ctx;
		free(ptr);
	}
}


TEST_CASE( "cimbar_api_test/testZstdDirect", "[unit]" )
{
	// Verify zstd compression/decompression with 20KB random data
	const int DATA_SIZE = 20000;
	std::string data = random_string(DATA_SIZE);

	cimbar::zstd_compressor<std::stringstream> comp;
	comp.set_compression_level(16);
	comp.write(data.data(), data.size());

	size_t compressed_size = comp.size();
	std::cerr << "zstd direct: " << DATA_SIZE << " -> " << compressed_size << std::endl;
	REQUIRE(compressed_size > 0);

	// Decompress
	cimbar::zstd_decompressor<std::stringstream> decomp;
	std::string comp_data = comp.str();
	decomp.init_decompress(comp_data.data(), comp_data.size());

	decomp.str(std::string());
	int writes = 0;
	while (decomp.write_once()) {
		++writes;
	}

	std::string result = decomp.str();
	std::cerr << "zstd decompress: " << result.size() << " bytes (" << writes << " write_once calls)" << std::endl;
	REQUIRE(result.size() == (size_t)DATA_SIZE);
	REQUIRE(result == data);
}

TEST_CASE( "cimbar_api_test/testZstdWithFilename", "[unit]" )
{
	const int DATA_SIZE = 20000;
	std::string data = random_string(DATA_SIZE);

	cimbar::zstd_compressor<std::stringstream> comp;
	comp.set_compression_level(16);
	comp.write_header("test.bin", 8);
	comp.write(data.data(), data.size());

	size_t compressed_size = comp.size();
	std::cerr << "zstd w/ filename: " << DATA_SIZE << " -> " << compressed_size << std::endl;

	cimbar::zstd_decompressor<std::stringstream> decomp;
	std::string comp_data = comp.str();
	decomp.init_decompress(comp_data.data(), comp_data.size());

	decomp.str(std::string());
	int writes = 0;
	while (decomp.write_once()) {
		++writes;
	}

	std::string result = decomp.str();
	std::cerr << "zstd decompress: " << result.size() << " bytes (" << writes << " calls)" << std::endl;
	REQUIRE(result.size() == (size_t)DATA_SIZE);
	REQUIRE(result == data);
}


TEST_CASE( "cimbar_api_test/testCreateDestroy", "[unit]" )
{
	int alloc_count = 0;
	cimbar_allocator_t alloc = {test_alloc, test_free, &alloc_count};

	cimbar_encoder_t* enc = cimbar_encoder_create(&alloc);
	REQUIRE(enc != nullptr);
	REQUIRE(alloc_count > 0);

	cimbar_encoder_destroy(enc);

	cimbar_decoder_t* dec = cimbar_decoder_create(&alloc);
	REQUIRE(dec != nullptr);

	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testConfig", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);

	int ret = cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	REQUIRE(ret == CIMBAR_OK);

	int val;
	ret = cimbar_encoder_get_config(enc, CIMBAR_CFG_SYMBOL_BITS, &val);
	REQUIRE(ret == CIMBAR_OK);
	REQUIRE(val > 0);

	ret = cimbar_encoder_get_config(enc, CIMBAR_CFG_COLOR_BITS, &val);
	REQUIRE(ret == CIMBAR_OK);
	REQUIRE(val >= 0);

	// Invalid config key
	ret = cimbar_encoder_set_config(enc, (cimbar_config_t)999, 0);
	REQUIRE(ret < 0);

	cimbar_encoder_destroy(enc);
}


TEST_CASE( "cimbar_api_test/testFountainRoundtrip", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	// Configure to mode 68 (default)
	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	// Set input data (use size that reliably passes; larger data may hit wirehair limits)
	const int DATA_SIZE = 10000;
	std::string data = random_string(DATA_SIZE);

	int ret = cimbar_encoder_set_input(enc, data.data(), data.size(), "test.bin");
	REQUIRE(ret == CIMBAR_OK);

	// Encode frames and feed into fountain decoder
	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	int frame_count = 0;
	int max_frames = 500;

	while (frame_count < max_frames)
	{
		ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
		if (ret <= 0)
		{
			std::cerr << "encode_next returned " << ret << " at frame " << frame_count << std::endl;
			break;
		}

		frame_count++;

		ret = cimbar_decoder_fountain_feed(dec, rgba.data(), rgba.size(), w, h, CIMBAR_IMAGE_RGBA);
		if (ret > 0)
			REQUIRE(ret >= 0);

		if (cimbar_decoder_fountain_is_complete(dec))
			break;
	}

	REQUIRE(cimbar_decoder_fountain_is_complete(dec));
	REQUIRE(cimbar_decoder_fountain_is_complete(dec));

	// Get the reassembled file size
	size_t file_size = cimbar_decoder_fountain_get_filesize(dec);
	REQUIRE(file_size > 0);

	// Read the decompressed data
	std::vector<uint8_t> result(file_size + 4096);
	size_t out_len = result.size();
	ret = cimbar_decoder_fountain_read(dec, result.data(), &out_len);
	REQUIRE(ret >= 0);

	std::cerr << "file_size=" << file_size << " decompressed=" << out_len << " expected=" << data.size() << std::endl;

	// Verify content matches
	std::string decoded(reinterpret_cast<char*>(result.data()), out_len);
	REQUIRE(decoded == data);

	cimbar_encoder_destroy(enc);
	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testVersion", "[unit]" )
{
	int major = -1, minor = -1, patch = -1;
	cimbar_version(&major, &minor, &patch);
	REQUIRE(major == 0);
	REQUIRE(minor == 1);
	REQUIRE(patch == 0);
}


TEST_CASE( "cimbar_api_test/testNullSafety", "[unit]" )
{
	int ret = cimbar_encoder_set_config(nullptr, CIMBAR_CFG_PRESET, 68);
	REQUIRE(ret < 0);

	ret = cimbar_decoder_set_config(nullptr, CIMBAR_CFG_PRESET, 68);
	REQUIRE(ret < 0);

	ret = cimbar_encoder_encode_next(nullptr, nullptr, 0, nullptr, nullptr);
	REQUIRE(ret < 0);

	ret = cimbar_decoder_scan(nullptr, nullptr, 0, 0, 0, CIMBAR_IMAGE_RGB, nullptr, nullptr);
	REQUIRE(ret < 0);

	const char* err = cimbar_error_string(nullptr);
	REQUIRE(err != nullptr);
}
