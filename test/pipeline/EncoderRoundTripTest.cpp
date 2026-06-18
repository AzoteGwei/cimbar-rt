/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "unittest.h"
#include "TestHelpers.h"

#include "pipeline/decode/Decoder.h"
#include "pipeline/encode/EncoderPlus.h"

#include "core/compression/zstd_decompressor.h"
#include "core/fountain/fountain_decoder_sink.h"
#include "imgproc/hash/average_hash.h"
#include "support/text/format.h"
#include "core/codec/ConfigScope.h"
#include "support/os/File.h"
#include "support/os/MakeTempDirectory.h"
#include "support/image/cv_bridge.h"

#include <iostream>
#include <string>

TEST_CASE( "EncoderRoundTripTest/testFountain.Pad", "[unit]" )
{
	MakeTempDirectory tempdir;

	std::string inputFile = tempdir.path() / "hello.txt";
	std::string outPrefix = tempdir.path() / "encoder.fountain";

	{
		std::ofstream f(inputFile);
		f << "hello"; // 5 bytes!
	}

	// will be padded so the fountain encoding is happy. The encoded image looks suspiciously non-random!
	EncoderPlus enc(4, 2);
	assertEquals( 1, enc.encode_fountain(inputFile, outPrefix) );

	uint64_t hash = 0xeecc8800efcea808;
	std::string path = fmt::format("{}_0.png", outPrefix);
	cv::Mat encodedImg = cv::imread(path);
	cv::cvtColor(encodedImg, encodedImg, cv::COLOR_BGR2RGB);
	assertEquals( hash, image_hash::average_hash(encodedImg) );

	Image encodedForDecode = cv_bridge::clone_from_mat(encodedImg);

	SECTION ("default filename") {
		// decoder
		Decoder dec;
		fountain_decoder_sink fds(cimbar::Config::fountain_chunk_size(), write_on_store<cimbar::zstd_decompressor<std::ofstream>>(tempdir.path()));

		unsigned bytesDecoded = dec.decode_fountain(encodedForDecode, fds);
		assertEquals( 7500, bytesDecoded );

		std::string decodedContents = File(tempdir.path() / "0.626").read_all();
		assertEquals( "hello", decodedContents );

		assertEquals( 1, fds.num_done() );
	}

	SECTION ("parsed filename") {
		Decoder dec;
		fountain_decoder_sink fds(cimbar::Config::fountain_chunk_size(), decompress_on_store<std::ofstream>(tempdir.path()));

		unsigned bytesDecoded = dec.decode_fountain(encodedForDecode, fds);
		assertEquals( 7500, bytesDecoded );

		std::string decodedContents = File(tempdir.path() / "hello.txt").read_all();
		assertEquals( "hello", decodedContents );

		assertEquals( 1, fds.num_done() );
	}
}

TEST_CASE( "EncoderRoundTripTest/testFountain.SinkMismatch", "[unit]" )
{
	MakeTempDirectory tempdir;
	ConfigScope cs(4);

	std::string inputFile = tempdir.path() / "hello.txt";
	std::string outPrefix = tempdir.path() / "encoder.fountain";

	{
		std::ofstream f(inputFile);
		f << "hello"; // 5 bytes!
	}

	// will be padded so the fountain encoding is happy. The encoded image looks suspiciously non-random!
	EncoderPlus enc(4, 2);
	assertEquals( 1, enc.encode_fountain(inputFile, outPrefix) );

	uint64_t hash = 0xae4c8c00efce9c28;
	std::string path = fmt::format("{}_0.png", outPrefix);
	cv::Mat encodedImg = cv::imread(path);
	cv::cvtColor(encodedImg, encodedImg, cv::COLOR_BGR2RGB);
	assertEquals( hash, image_hash::average_hash(encodedImg) );

	Image encodedForDecode = cv_bridge::clone_from_mat(encodedImg);

	// decoder
	Decoder dec;
	// sink with a mismatched fountain_chunk_size
	// importantly, the sink expects a *smaller* chunk than we'll give it...
	// because that's a more interesting test...
	fountain_decoder_sink fds(cimbar::Config::fountain_chunk_size()-125, write_on_store<cimbar::zstd_decompressor<std::ofstream>>(tempdir.path()));

	unsigned bytesDecoded = dec.decode_fountain(encodedForDecode, fds);
	assertEquals( 7500, bytesDecoded );

	assertEquals( 0, fds.num_done() );
}

TEST_CASE( "EncoderRoundTripTest/testStreaming", "[unit]" )
{
	MakeTempDirectory tempdir;

	//input
	std::ifstream infile(TestCimbar::getProjectDir() + "/LICENSE");

	// create encoder
	EncoderPlus enc(4, 2);
	fountain_encoder_stream::ptr fes = enc.create_fountain_encoder(infile, "");
	assertTrue( fes );
	assertTrue( fes->good() );

	// create decoder
	Decoder dec;
	fountain_decoder_sink fds(cimbar::Config::fountain_chunk_size(), write_on_store<cimbar::zstd_decompressor<std::ofstream>>(tempdir.path()));

	// encode frames, then pass to decoder
	for (int i = 0; i < 100; ++i)
	{
		std::optional<Image> frame = enc.encode_next(*fes);
		assertTrue( frame );

		unsigned bytesDecoded = dec.decode_fountain(*frame, fds);
		assertEquals( 7500, bytesDecoded );

		if (fds.num_done())
			break;
	}

	// done
	assertEquals( 1, fds.num_done() );
	std::string decodedContents = File(tempdir.path() / "0.5256").read_all();
	assertEquals( 16727, decodedContents.size() );
	assertStringContains( "Mozilla Public License Version 2.0", decodedContents );
}
