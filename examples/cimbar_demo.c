/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * cimbar-c-demo — minimal example of the libcimbar C API.
 *
 * Build (in-tree):
 *   BUILD_DIR=../build ./build-demo.sh
 *
 * Build (installed):
 *   ./build-demo.sh
 */
#include <libcimbar/cimbar.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Custom allocator — track total allocations                        */
/* ------------------------------------------------------------------ */
static void* tracking_alloc(void* ctx, size_t size)
{
	(*(size_t*)ctx) += size;
	return malloc(size);
}

static void tracking_free(void* ctx, void* ptr)
{
	(void)ctx;
	free(ptr);
}

static size_t s_alloc_total = 0;
static const cimbar_allocator_t s_alloc = {tracking_alloc, tracking_free, &s_alloc_total};

/* ------------------------------------------------------------------ */
/*  Encode & decode a file with fountain code (multi-frame)           */
/* ------------------------------------------------------------------ */
static int encode_decode(const char* input_path)
{
	/* ---- read input file ---- */
	FILE* f = fopen(input_path, "rb");
	if (!f) { perror("fopen"); return -1; }
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	rewind(f);

	unsigned char* input = malloc(fsize);
	fread(input, 1, fsize, f);
	fclose(f);

	printf("input: %ld bytes\n", fsize);

	/* ---- version ---- */
	int ver_major, ver_minor, ver_patch;
	cimbar_version(&ver_major, &ver_minor, &ver_patch);
	printf("libcimbar version: %d.%d.%d\n", ver_major, ver_minor, ver_patch);

	/* ---- create encoder ---- */
	cimbar_encoder_t* enc = cimbar_encoder_create(&s_alloc);
	if (!enc) { fprintf(stderr, "encoder_create failed\n"); return -1; }

	cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
	cimbar_encoder_set_config(enc, CIMBAR_CFG_COMPRESSION, 16);

	/* ---- set input data (with filename for metadata) ---- */
	const char* fname = strrchr(input_path, '/');
	fname = fname ? fname + 1 : input_path;
	int ret = cimbar_encoder_set_input(enc, input, fsize, fname);
	if (ret < 0) { fprintf(stderr, "set_input failed: %d\n", ret); return -1; }

	/* ---- create decoder ---- */
	cimbar_decoder_t* dec = cimbar_decoder_create(&s_alloc);
	if (!dec) { fprintf(stderr, "decoder_create failed\n"); return -1; }
	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	/* ---- encode frames & feed to fountain decoder ---- */
	unsigned char* rgba = malloc(2048 * 2048 * 4);
	if (!rgba) { fprintf(stderr, "malloc failed\n"); return -1; }
	int frames = 0;
	while (frames < 500)
	{
		unsigned w = 0, h = 0;
		ret = cimbar_encoder_encode_next(enc, rgba, 2048 * 2048 * 4, &w, &h);
		if (ret <= 0) break;          // STREAM_END
		frames++;

		ret = cimbar_decoder_fountain_feed(dec, rgba, 2048 * 2048 * 4,
		                                   w, h, CIMBAR_IMAGE_RGBA);
		if (ret < 0) break;

		if (cimbar_decoder_fountain_is_complete(dec))
		{
			printf("decoder complete after %d frames\n", frames);
			break;
		}
	}

	if (!cimbar_decoder_fountain_is_complete(dec))
	{
		fprintf(stderr, "fountain decode did not complete\n");
		return -1;
	}

	/* ---- read back the decoded file ---- */
	size_t out_len = 1024 * 1024;
	unsigned char* output = malloc(out_len);
	ret = cimbar_decoder_fountain_read(dec, output, &out_len);
	if (ret < 0) { fprintf(stderr, "fountain_read failed: %d\n", ret); return -1; }

	/* ---- verify ---- */
	int ok = (out_len == (size_t)fsize &&
	          memcmp(output, input, fsize) == 0);
	printf("decode: %zu bytes — %s\n", out_len, ok ? "MATCH" : "MISMATCH");

	/* ---- get filename from reassembled data ---- */
	char name_buf[256] = {0};
	cimbar_decoder_fountain_get_filename(dec, name_buf, sizeof(name_buf));
	if (name_buf[0])
		printf("original filename: %s\n", name_buf);

	/* ---- cleanup ---- */
	cimbar_encoder_destroy(enc);
	cimbar_decoder_destroy(dec);
	free(rgba);
	free(input);
	free(output);
	return ok ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/*  Single-frame scan (without fountain)                              */
/* ------------------------------------------------------------------ */
static int scan_file(const char* image_path)
{
	cimbar_decoder_t* dec = cimbar_decoder_create(NULL);
	if (!dec) return -1;

	cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

	/* load image and extract cells */
	unsigned char* rgba = malloc(2048 * 2048 * 4);
	if (!rgba) { fprintf(stderr, "malloc failed\n"); return -1; }
	unsigned w = 0, h = 0;
	int ret = cimbar_image_load(image_path, rgba, 2048 * 2048 * 4, &w, &h);
	if (ret < 0) { fprintf(stderr, "image_load failed: %d\n", ret); free(rgba); return -1; }

	printf("image: %ux%u\n", w, h);

	/* extract raw cells (bypass full decode) */
	cimbar_cell_t* cells = malloc(20000 * sizeof(cimbar_cell_t));
	if (!cells) { fprintf(stderr, "malloc failed\n"); free(rgba); return -1; }
	unsigned num_cells = 0;
	ret = cimbar_extract_cells(rgba, 2048 * 2048 * 4, w, h, CIMBAR_IMAGE_RGBA,
	                           cells, 20000, &num_cells);
	if (ret < 0)
	{
		fprintf(stderr, "extract_cells failed: %d\n", ret);
		/* fall through to scan path */
	}
	else
		printf("extracted %u cells\n", num_cells);

	/* full scan (image → data) */
	size_t out_len = 1024 * 1024;
	unsigned char* output = malloc(out_len);
	ret = cimbar_decoder_scan_file(dec, image_path, output, &out_len);
	if (ret < 0) { fprintf(stderr, "scan_file failed: %d\n", ret); return -1; }

	printf("scanned: %zu bytes\n", out_len);

	cimbar_decoder_destroy(dec);
	free(rgba);
	free(cells);
	free(output);
	return 0;
}

/* ------------------------------------------------------------------ */
/*  main                                                              */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "  %s encode-decode <file>\n", argv[0]);
		fprintf(stderr, "  %s scan <cimbar-image.png>\n", argv[0]);
		return 1;
	}

	if (strcmp(argv[1], "encode-decode") == 0)
		return encode_decode(argv[2]);

	if (strcmp(argv[1], "scan") == 0)
		return scan_file(argv[2]);

	fprintf(stderr, "unknown subcommand: %s\n", argv[1]);
	return 1;
}
