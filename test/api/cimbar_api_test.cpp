/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#include "unittest.h"
#include "TestHelpers.h"

#include <libcimbar/cimbar.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Test zstd directly
#include "core/compression/zstd_compressor.h"
#include "core/compression/zstd_decompressor.h"

#include "core/codec/CimbWriter.h"
#include "core/codec/Config.h"
#include <opencv2/opencv.hpp>

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


TEST_CASE( "cimbar_api_test/testCellConsistency", "[unit]" )
{
	// Verify encode_next_cells produces valid cells with correct ranges.
	// Cells are captured directly from the fountain bitbuffer (no rendering).
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);

	std::string data = random_string(100);
	int ret = cimbar_encoder_set_input(enc, data.data(), data.size(), "test.bin");
	REQUIRE(ret == CIMBAR_OK);

	std::vector<cimbar_cell_t> cells(cimbar::Config::total_cells());
	unsigned num_cells = 0;
	ret = cimbar_encoder_encode_next_cells(enc, cells.data(), cells.size(), &num_cells);
	REQUIRE(ret > 0);
	REQUIRE(num_cells == cimbar::Config::total_cells());

	unsigned max_symbol = (1u << cimbar::Config::symbol_bits()) - 1;
	unsigned max_color = (1u << cimbar::Config::color_bits()) - 1;

	for (unsigned i = 0; i < num_cells; ++i)
	{
		REQUIRE(cells[i].symbol <= max_symbol);
		REQUIRE(cells[i].color <= max_color);
	}

	cimbar_encoder_destroy(enc);
}


TEST_CASE( "cimbar_api_test/testCellRoundtrip", "[unit]" )
{
	// Full roundtrip via encode_next_cells + fountain_feed_cells
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	const int DATA_SIZE = 500;
	std::string data = random_string(DATA_SIZE);

	int ret = cimbar_encoder_set_input(enc, data.data(), data.size(), "test.bin");
	REQUIRE(ret == CIMBAR_OK);

	std::vector<cimbar_cell_t> cells(100000);
	int frame_count = 0;
	int max_frames = 200;

	while (frame_count < max_frames)
	{
		unsigned num_cells = 0;
		ret = cimbar_encoder_encode_next_cells(enc, cells.data(), cells.size(), &num_cells);
		if (ret <= 0)
			break;

		frame_count++;

		ret = cimbar_decoder_fountain_feed_cells(dec, cells.data(), num_cells);
		REQUIRE(ret >= 0);

		if (cimbar_decoder_fountain_is_complete(dec))
			break;
	}

	std::cerr << "Cell roundtrip: " << frame_count << " frames, progress="
	          << cimbar_decoder_fountain_get_progress(dec) << "%" << std::endl;
	REQUIRE(cimbar_decoder_fountain_is_complete(dec));

	size_t file_size = cimbar_decoder_fountain_get_filesize(dec);
	REQUIRE(file_size > 0);

	std::vector<uint8_t> result(file_size + 4096);
	size_t out_len = result.size();
	ret = cimbar_decoder_fountain_read(dec, result.data(), &out_len);
	REQUIRE(ret >= 0);

	std::string decoded(reinterpret_cast<char*>(result.data()), out_len);
	REQUIRE(decoded == data);

	cimbar_encoder_destroy(enc);
	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testRoundtripProgress", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	const int DATA_SIZE = 10000;
	std::string data = random_string(DATA_SIZE);

	int ret = cimbar_encoder_set_input(enc, data.data(), data.size(), "prog_test.bin");
	REQUIRE(ret == CIMBAR_OK);

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	int max_frames = 500;
	int progress = 0;

	for (int i = 0; i < max_frames; ++i)
	{
		ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
		if (ret <= 0)
			break;

		ret = cimbar_decoder_fountain_feed(dec, rgba.data(), rgba.size(), w, h, CIMBAR_IMAGE_RGBA);
		REQUIRE(ret >= 0);

		// Verify progress increases over time
		int p = cimbar_decoder_fountain_get_progress(dec);
		REQUIRE(p >= progress);
		progress = p;

		if (cimbar_decoder_fountain_is_complete(dec))
			break;
	}

	REQUIRE(cimbar_decoder_fountain_is_complete(dec));
	REQUIRE(progress >= 100); // complete = 100%

	// Verify filename
	char name_buf[256]{};
	int fn_len = cimbar_decoder_fountain_get_filename(dec, name_buf, sizeof(name_buf));
	REQUIRE(fn_len > 0);
	std::string filename(name_buf, fn_len);
	REQUIRE(filename == "prog_test.bin");

	size_t file_size = cimbar_decoder_fountain_get_filesize(dec);
	REQUIRE(file_size > 0);

	std::vector<uint8_t> result(file_size + 4096);
	size_t out_len = result.size();
	ret = cimbar_decoder_fountain_read(dec, result.data(), &out_len);
	REQUIRE(ret >= 0);

	std::string decoded(reinterpret_cast<char*>(result.data()), out_len);
	REQUIRE(decoded == data);

	cimbar_encoder_destroy(enc);
	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testSetInputFile", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	const int DATA_SIZE = 5000;
	std::string data = random_string(DATA_SIZE);

	// Write data to temp file for set_input_file
	std::string tmp_path = "/tmp/cimbar_test_input.bin";
	{
		FILE* f = fopen(tmp_path.c_str(), "wb");
		REQUIRE(f != nullptr);
		fwrite(data.data(), 1, data.size(), f);
		fclose(f);
	}

	int ret = cimbar_encoder_set_input_file(enc, tmp_path.c_str());
	REQUIRE(ret == CIMBAR_OK);

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	int max_frames = 500;

	for (int i = 0; i < max_frames; ++i)
	{
		ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
		if (ret <= 0)
			break;

		ret = cimbar_decoder_fountain_feed(dec, rgba.data(), rgba.size(), w, h, CIMBAR_IMAGE_RGBA);
		REQUIRE(ret >= 0);

		if (cimbar_decoder_fountain_is_complete(dec))
			break;
	}

	REQUIRE(cimbar_decoder_fountain_is_complete(dec));

	size_t file_size = cimbar_decoder_fountain_get_filesize(dec);
	REQUIRE(file_size > 0);

	std::vector<uint8_t> result(file_size + 4096);
	size_t out_len = result.size();
	ret = cimbar_decoder_fountain_read(dec, result.data(), &out_len);
	REQUIRE(ret >= 0);

	std::string decoded(reinterpret_cast<char*>(result.data()), out_len);
	REQUIRE(decoded == data);

	// Clean up temp file
	std::remove(tmp_path.c_str());

	cimbar_encoder_destroy(enc);
	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testScanFile", "[unit]" )
{
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	std::string sample_path = TestCimbar::getSample("b/4cecc30f.png");

	std::vector<uint8_t> output(1024 * 1024);
	size_t out_len = output.size();
	int ret = cimbar_decoder_scan_file(dec, sample_path.c_str(), output.data(), &out_len);
	REQUIRE(ret > 0);
	REQUIRE(out_len > 0);

	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testEncoderStats", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);

	// Empty stats before input
	cimbar_encoder_stats_t stats{};
	int ret = cimbar_encoder_get_stats(enc, &stats);
	REQUIRE(ret == CIMBAR_OK);
	REQUIRE(stats.total_frames == 0);

	std::string data = random_string(500);
	cimbar_encoder_set_input(enc, data.data(), data.size(), nullptr);

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	for (int i = 0; i < 20; ++i)
	{
		ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
		if (ret <= 0) break;
	}

	ret = cimbar_encoder_get_stats(enc, &stats);
	REQUIRE(ret == CIMBAR_OK);
	REQUIRE(stats.total_frames > 0);
	REQUIRE(stats.fountain_blocks_required > 0);

	cimbar_encoder_destroy(enc);
}


TEST_CASE( "cimbar_api_test/testEncoderReset", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);

	std::string data1 = random_string(100);
	int ret = cimbar_encoder_set_input(enc, data1.data(), data1.size(), "file1.bin");
	REQUIRE(ret == CIMBAR_OK);

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
	REQUIRE(ret > 0);

	// Reset and reuse with different data
	ret = cimbar_encoder_reset(enc);
	REQUIRE(ret == CIMBAR_OK);

	std::string data2 = random_string(200);
	ret = cimbar_encoder_set_input(enc, data2.data(), data2.size(), "file2.bin");
	REQUIRE(ret == CIMBAR_OK);

	ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
	REQUIRE(ret > 0);

	cimbar_encoder_destroy(enc);
}


TEST_CASE( "cimbar_api_test/testDecoderReset", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	// First roundtrip
	std::string data1 = random_string(500);
	cimbar_encoder_set_input(enc, data1.data(), data1.size(), "reset1.bin");

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	int max_frames = 500;

	for (int i = 0; i < max_frames; ++i)
	{
		int ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
		if (ret <= 0) break;
		cimbar_decoder_fountain_feed(dec, rgba.data(), rgba.size(), w, h, CIMBAR_IMAGE_RGBA);
		if (cimbar_decoder_fountain_is_complete(dec)) break;
	}
	REQUIRE(cimbar_decoder_fountain_is_complete(dec));

	// Reset decoder
	int ret = cimbar_decoder_reset(dec);
	REQUIRE(ret == CIMBAR_OK);
	REQUIRE_FALSE(cimbar_decoder_fountain_is_complete(dec));

	// Second roundtrip
	std::string data2 = random_string(800);
	cimbar_encoder_set_input(enc, data2.data(), data2.size(), "reset2.bin");

	for (int i = 0; i < max_frames; ++i)
	{
		ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
		if (ret <= 0) break;
		cimbar_decoder_fountain_feed(dec, rgba.data(), rgba.size(), w, h, CIMBAR_IMAGE_RGBA);
		if (cimbar_decoder_fountain_is_complete(dec)) break;
	}
	REQUIRE(cimbar_decoder_fountain_is_complete(dec));

	size_t file_size = cimbar_decoder_fountain_get_filesize(dec);
	REQUIRE(file_size > 0);

	std::vector<uint8_t> result(file_size + 4096);
	size_t out_len = result.size();
	ret = cimbar_decoder_fountain_read(dec, result.data(), &out_len);
	REQUIRE(ret >= 0);

	std::string decoded(reinterpret_cast<char*>(result.data()), out_len);
	REQUIRE(decoded == data2);

	cimbar_encoder_destroy(enc);
	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testImageLoadExtract", "[unit]" )
{
	std::string sample_path = TestCimbar::getSample("b/4cecc30f.png");

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	int ret = cimbar_image_load(sample_path.c_str(), rgba.data(), rgba.size(), &w, &h);
	REQUIRE(ret > 0);
	REQUIRE(w > 0);
	REQUIRE(h > 0);

	std::vector<cimbar_cell_t> cells(100000);
	unsigned num_cells = 0;
	ret = cimbar_extract_cells(rgba.data(), rgba.size(), w, h, CIMBAR_IMAGE_RGBA, cells.data(), cells.size(), &num_cells);
	REQUIRE(ret > 0);
	REQUIRE(num_cells > 0);
}


TEST_CASE( "cimbar_api_test/testDump", "[unit]" )
{
	char buf[256]{};

	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	int ret = cimbar_encoder_dump(enc, buf, sizeof(buf));
	REQUIRE(ret > 0);
	cimbar_encoder_destroy(enc);

	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);
	ret = cimbar_decoder_dump(dec, buf, sizeof(buf));
	REQUIRE(ret > 0);
	cimbar_decoder_destroy(dec);

	// Null safety for dump
	ret = cimbar_encoder_dump(nullptr, buf, sizeof(buf));
	REQUIRE(ret < 0);

	ret = cimbar_decoder_dump(nullptr, buf, sizeof(buf));
	REQUIRE(ret < 0);
}


TEST_CASE( "cimbar_api_test/testEmptyInput", "[unit]" )
{
	// Verify encoder handles 0-byte input gracefully (no crash)
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);

	int ret = cimbar_encoder_set_input(enc, "", 0, "empty.bin");
	REQUIRE(ret == CIMBAR_OK);

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
	// Encoder produces at least one frame for padded empty data
	REQUIRE(ret > 0);

	cimbar_encoder_destroy(enc);
}


TEST_CASE( "cimbar_api_test/testUnicodeFilename", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	std::string data = random_string(500);
	std::string filename = "中文文件测试.bin";
	int ret = cimbar_encoder_set_input(enc, data.data(), data.size(), filename.c_str());
	REQUIRE(ret == CIMBAR_OK);

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	int max_frames = 200;
	for (int i = 0; i < max_frames; ++i)
	{
		ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
		if (ret <= 0) break;
		ret = cimbar_decoder_fountain_feed(dec, rgba.data(), rgba.size(), w, h, CIMBAR_IMAGE_RGBA);
		REQUIRE(ret >= 0);
		if (cimbar_decoder_fountain_is_complete(dec)) break;
	}

	REQUIRE(cimbar_decoder_fountain_is_complete(dec));

	char name_buf[256]{};
	int fn_len = cimbar_decoder_fountain_get_filename(dec, name_buf, sizeof(name_buf));
	REQUIRE(fn_len > 0);
	std::string decoded_name(name_buf, fn_len);
	REQUIRE(decoded_name == filename);

	size_t file_size = cimbar_decoder_fountain_get_filesize(dec);
	std::vector<uint8_t> result(file_size + 4096);
	size_t out_len = result.size();
	ret = cimbar_decoder_fountain_read(dec, result.data(), &out_len);
	REQUIRE(ret >= 0);
	std::string decoded(reinterpret_cast<char*>(result.data()), out_len);
	REQUIRE(decoded == data);

	cimbar_encoder_destroy(enc);
	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testFountainFeedFile", "[unit]" )
{
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	std::string sample_path = TestCimbar::getSample("b/4cecc30f.png");
	int ret = cimbar_decoder_fountain_feed_file(dec, sample_path.c_str());
	REQUIRE(ret >= 0);

	int progress = cimbar_decoder_fountain_get_progress(dec);
	REQUIRE(progress > 0);

	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testFountainFeedCells", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	std::string data = random_string(500);
	int ret = cimbar_encoder_set_input(enc, data.data(), data.size(), "test.bin");
	REQUIRE(ret == CIMBAR_OK);

	std::vector<cimbar_cell_t> cells(cimbar::Config::total_cells());
	int frame_count = 0;
	int max_frames = 200;
	while (frame_count < max_frames)
	{
		unsigned num_cells = 0;
		ret = cimbar_encoder_encode_next_cells(enc, cells.data(), cells.size(), &num_cells);
		if (ret <= 0) break;
		frame_count++;
		ret = cimbar_decoder_fountain_feed_cells(dec, cells.data(), num_cells);
		REQUIRE(ret >= 0);
		if (cimbar_decoder_fountain_is_complete(dec)) break;
	}

	REQUIRE(cimbar_decoder_fountain_is_complete(dec));
	size_t file_size = cimbar_decoder_fountain_get_filesize(dec);
	REQUIRE(file_size > 0);

	std::vector<uint8_t> result(file_size + 4096);
	size_t out_len = result.size();
	ret = cimbar_decoder_fountain_read(dec, result.data(), &out_len);
	REQUIRE(ret >= 0);
	std::string decoded(reinterpret_cast<char*>(result.data()), out_len);
	REQUIRE(decoded == data);

	cimbar_encoder_destroy(enc);
	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testDecodeScanFormats", "[unit]" )
{
	cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
	REQUIRE(enc != nullptr);
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	std::string data = random_string(10);
	cimbar_encoder_set_input(enc, data.data(), data.size(), nullptr);

	std::vector<uint8_t> rgba(2048 * 2048 * 4);
	unsigned w = 0, h = 0;
	int ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
	REQUIRE(ret > 0);

	cimbar_image_format_t formats[] = {
		CIMBAR_IMAGE_RGB, CIMBAR_IMAGE_RGBA, CIMBAR_IMAGE_BGR,
		CIMBAR_IMAGE_BGRA, CIMBAR_IMAGE_GRAY,
	};

	for (auto fmt : formats)
	{
		cimbar_decoder_reset(dec);
		uint8_t output[4096]{};
		size_t out_len = sizeof(output);
		ret = cimbar_decoder_scan(dec, rgba.data(), rgba.size(), w, h, fmt, output, &out_len);
		REQUIRE(ret != CIMBAR_ERR_BAD_PARAM);
	}

	cimbar_encoder_destroy(enc);
	cimbar_decoder_destroy(dec);
}


TEST_CASE( "cimbar_api_test/testDecoderGetConfig", "[unit]" )
{
	cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
	REQUIRE(dec != nullptr);

	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	cimbar_config_t keys[] = {
		CIMBAR_CFG_SYMBOL_BITS, CIMBAR_CFG_COLOR_BITS,
		CIMBAR_CFG_ECC_BYTES, CIMBAR_CFG_ECC_BLOCK_SIZE,
		CIMBAR_CFG_IMAGE_SIZE_X, CIMBAR_CFG_IMAGE_SIZE_Y,
	};

	for (auto key : keys)
	{
		int val = -1;
		int ret = cimbar_decoder_get_config(dec, key, &val);
		REQUIRE(ret == CIMBAR_OK);
		REQUIRE(val > 0);
	}

	int val;
	int ret = cimbar_decoder_get_config(dec, (cimbar_config_t)999, &val);
	REQUIRE(ret < 0);

	cimbar_decoder_destroy(dec);
}
