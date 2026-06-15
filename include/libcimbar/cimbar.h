/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#ifndef CIMBAR_H
#define CIMBAR_H

#include "cimbar_export.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


// =========================================================================
// Version
// =========================================================================

CIMBAR_EXPORT void cimbar_version(int* major, int* minor, int* patch);


// =========================================================================
// Error handling
// =========================================================================

typedef enum cimbar_error {
	CIMBAR_OK                =  0,
	CIMBAR_ERR_NOMEM         = -1,
	CIMBAR_ERR_BAD_PARAM     = -2,
	CIMBAR_ERR_NO_DATA       = -3,
	CIMBAR_ERR_DECODE_FAIL   = -4,
	CIMBAR_ERR_ENCODE_FAIL   = -5,
	CIMBAR_ERR_STREAM_END    = -6,
	CIMBAR_ERR_INCOMPLETE    = -7,
	CIMBAR_ERR_FORMAT        = -8,
	CIMBAR_ERR_IO            = -9,
} cimbar_error_t;

CIMBAR_EXPORT const char* cimbar_error_string(const void* handle);


// =========================================================================
// Allocator
// =========================================================================

typedef void* (*cimbar_alloc_func)(void* context, size_t size);
typedef void  (*cimbar_free_func)(void* context, void* ptr);

typedef struct cimbar_allocator {
	cimbar_alloc_func alloc;
	cimbar_free_func  free;
	void*             context;
} cimbar_allocator_t;


// =========================================================================
// Cell value type
// =========================================================================

typedef struct cimbar_cell {
	uint8_t symbol;
	uint8_t color;
} cimbar_cell_t;


// =========================================================================
// Image format (for decoder input)
// =========================================================================

typedef enum cimbar_image_format {
	CIMBAR_IMAGE_UNKNOWN = 0,
	CIMBAR_IMAGE_RGB,       // 3-channel, row-major
	CIMBAR_IMAGE_RGBA,      // 4-channel, row-major
	CIMBAR_IMAGE_BGR,       // 3-channel OpenCV-compatible
	CIMBAR_IMAGE_BGRA,      // 4-channel OpenCV-compatible
	CIMBAR_IMAGE_GRAY,      // 1-channel 8-bit
} cimbar_image_format_t;


// =========================================================================
// Configuration keys
// =========================================================================

typedef enum cimbar_config {
	CIMBAR_CFG_COLOR_MODE          = 0,  // int: 0=legacy(coupled), 1=decoupled
	CIMBAR_CFG_SYMBOL_BITS         = 1,  // int: bits per symbol
	CIMBAR_CFG_COLOR_BITS          = 2,  // int: bits per color
	CIMBAR_CFG_ECC_BYTES           = 3,  // int: Reed-Solomon ECC bytes
	CIMBAR_CFG_ECC_BLOCK_SIZE      = 4,  // int: RS block size
	CIMBAR_CFG_COMPRESSION         = 5,  // int: zstd compression level (0=disable)
	CIMBAR_CFG_FOUNTAIN_REDUNDANCY = 6,  // int: fountain redundancy multiplier (percent, 100=1.0x)
	CIMBAR_CFG_IMAGE_SIZE_X        = 7,  // int: output image width
	CIMBAR_CFG_IMAGE_SIZE_Y        = 8,  // int: output image height
	CIMBAR_CFG_PRESET              = 9,  // int: preset mode (4, 8, 66, 67, 68)
} cimbar_config_t;


// =========================================================================
// Statistics
// =========================================================================

typedef struct cimbar_encoder_stats {
	unsigned total_frames;
	unsigned bytes_in;
	unsigned bytes_out_compressed;
	unsigned fountain_blocks;
	unsigned fountain_blocks_required;
} cimbar_encoder_stats_t;


// =========================================================================
// Opaque types
// =========================================================================

typedef struct cimbar_encoder cimbar_encoder_t;
typedef struct cimbar_decoder cimbar_decoder_t;


// =========================================================================
// Encoder API
// =========================================================================

CIMBAR_EXPORT cimbar_encoder_t* cimbar_encoder_create(const cimbar_allocator_t* allocator);
CIMBAR_EXPORT void              cimbar_encoder_destroy(cimbar_encoder_t* encoder);

// Configuration
CIMBAR_EXPORT int cimbar_encoder_set_config(cimbar_encoder_t* encoder, cimbar_config_t key, int value);
CIMBAR_EXPORT int cimbar_encoder_get_config(const cimbar_encoder_t* encoder, cimbar_config_t key, int* value);

// Input data
CIMBAR_EXPORT int cimbar_encoder_set_input(cimbar_encoder_t* encoder, const void* data, size_t len, const char* filename);
CIMBAR_EXPORT int cimbar_encoder_set_input_file(cimbar_encoder_t* encoder, const char* path);

// Encode next frame as RGBA pixels
// Returns 1 on success (frame written), 0 if no more frames (STREAM_END),
// negative on error. Caller owns rgba buffer.
CIMBAR_EXPORT int cimbar_encoder_encode_next(cimbar_encoder_t* encoder,
                                             uint8_t* rgba, size_t buf_size,
                                             unsigned* out_width, unsigned* out_height);

// Encode next frame as cell grid (bypass rendering)
// Returns 1 on success (cells written), 0 if no more frames, negative on error.
// Caller owns cells buffer.
CIMBAR_EXPORT int cimbar_encoder_encode_next_cells(cimbar_encoder_t* encoder,
                                                   cimbar_cell_t* cells, size_t max_cells,
                                                   unsigned* num_cells);

// Reset encoder state (reuse object for new encoding session)
CIMBAR_EXPORT int cimbar_encoder_reset(cimbar_encoder_t* encoder);

// Statistics / debug
CIMBAR_EXPORT int cimbar_encoder_get_stats(const cimbar_encoder_t* encoder, cimbar_encoder_stats_t* stats);
CIMBAR_EXPORT int cimbar_encoder_dump(const cimbar_encoder_t* encoder, char* buf, size_t len);


// =========================================================================
// Decoder API
// =========================================================================

CIMBAR_EXPORT cimbar_decoder_t* cimbar_decoder_create(const cimbar_allocator_t* allocator);
CIMBAR_EXPORT void              cimbar_decoder_destroy(cimbar_decoder_t* decoder);

// Configuration
CIMBAR_EXPORT int cimbar_decoder_set_config(cimbar_decoder_t* decoder, cimbar_config_t key, int value);
CIMBAR_EXPORT int cimbar_decoder_get_config(const cimbar_decoder_t* decoder, cimbar_config_t key, int* value);

// ---- Single-frame decode (non-fountain) ----

// Decode from raw image pixels (full pipeline: image -> cells -> data)
// out_len: on input = buffer capacity, on output = bytes written
// Returns >0 bytes decoded, 0 = no data, negative on error.
CIMBAR_EXPORT int cimbar_decoder_scan(cimbar_decoder_t* decoder,
                                      const uint8_t* image_data, size_t data_len,
                                      unsigned width, unsigned height, cimbar_image_format_t format,
                                      uint8_t* output, size_t* out_len);

// Decode from image file path (png/bmp)
CIMBAR_EXPORT int cimbar_decoder_scan_file(cimbar_decoder_t* decoder,
                                           const char* path,
                                           uint8_t* output, size_t* out_len);

// Decode from pre-extracted cell grid (codec-only: cells -> data)
CIMBAR_EXPORT int cimbar_decoder_decode_cells(cimbar_decoder_t* decoder,
                                              const cimbar_cell_t* cells, size_t num_cells,
                                              uint8_t* output, size_t* out_len);

// ---- Fountain multi-frame decode ----

// Feed one frame into the fountain decoder (accumulates state)
// Returns 1 if more frames needed, 0 if complete file assembled,
// negative on error.
CIMBAR_EXPORT int cimbar_decoder_fountain_feed(cimbar_decoder_t* decoder,
                                               const uint8_t* image_data, size_t data_len,
                                               unsigned width, unsigned height, cimbar_image_format_t format);

// Feed one frame from file path
CIMBAR_EXPORT int cimbar_decoder_fountain_feed_file(cimbar_decoder_t* decoder, const char* path);

// Feed pre-extracted cell grid into fountain decoder
CIMBAR_EXPORT int cimbar_decoder_fountain_feed_cells(cimbar_decoder_t* decoder,
                                                     const cimbar_cell_t* cells, size_t num_cells);

// Query fountain decode status
CIMBAR_EXPORT int  cimbar_decoder_fountain_is_complete(const cimbar_decoder_t* decoder);
CIMBAR_EXPORT int  cimbar_decoder_fountain_get_progress(const cimbar_decoder_t* decoder);

// Read the reassembled file after fountain decode completes
// out_len: on input = buffer capacity, on output = bytes written
CIMBAR_EXPORT int cimbar_decoder_fountain_read(cimbar_decoder_t* decoder,
                                               uint8_t* buf, size_t* out_len);

// Get the filename from the reassembled fountain data (if available)
CIMBAR_EXPORT int cimbar_decoder_fountain_get_filename(cimbar_decoder_t* decoder,
                                                       char* name_buf, size_t buf_size);

// Get the uncompressed file size
CIMBAR_EXPORT size_t cimbar_decoder_fountain_get_filesize(const cimbar_decoder_t* decoder);

// Reset decoder state (reuse for new decode session)
CIMBAR_EXPORT int cimbar_decoder_reset(cimbar_decoder_t* decoder);

// Debug dump
CIMBAR_EXPORT int cimbar_decoder_dump(const cimbar_decoder_t* decoder, char* buf, size_t len);


// =========================================================================
// Standalone image utilities (no encoder/decoder state needed)
// =========================================================================

// Load an image file (png/bmp) into an RGBA buffer
// Returns 1 on success, negative on error.
CIMBAR_EXPORT int cimbar_image_load(const char* path,
                                    uint8_t* rgba, size_t buf_size,
                                    unsigned* out_width, unsigned* out_height);

// Extract cell grid from an RGBA image (image processing: pixels -> cells)
// Returns number of cells extracted, negative on error.
CIMBAR_EXPORT int cimbar_extract_cells(const uint8_t* image_data, size_t data_len,
                                       unsigned width, unsigned height, cimbar_image_format_t format,
                                       cimbar_cell_t* cells, size_t max_cells,
                                       unsigned* num_cells);

// Render cell grid to RGBA pixels (rendering: cells -> pixels)
// Returns 1 on success, negative on error.
CIMBAR_EXPORT int cimbar_render(const cimbar_cell_t* cells, size_t num_cells,
                                uint8_t* rgba, size_t buf_size,
                                unsigned* out_width, unsigned* out_height);


#ifdef __cplusplus
}
#endif

#endif
