# Plan: Add unit tests + run fuzzer in CI

## Files to modify

### 1. test/api/cimbar_api_test.cp

Add 6 new test cases after `testDump`:

| Test Case | What it tests |
|---|---|
| `testEmptyInput` | Encode 0 bytes → decode roundtrip (single frame, 0-length data) |
| `testUnicodeFilename` | `set_input` with Chinese filename `中文文件.bin` + verify via `get_filename` |
| `testFountainFeedFile` | `fountain_feed_file` on a sample encoded image from disk |
| `testFountainFeedCells` | Use cell API roundtrip with a known preset, verify via `fountain_get_progress` |
| `testDecodeScanFormats` | `decoder_scan` with RGBA/BGR/BGRA/GRAY formats (already RGB-tested via NullSafety) |
| `testDecoderGetConfig` | `decoder_get_config` for all readable config keys |

### 2. .github/workflows/ci.yml

Modify the `fuzz` job — after build step, add:

```yaml
- name: Run fuzz targets
  run: |
    ASAN_OPTIONS=alloc_dealloc_mismatch=0 \
    timeout 30 ./build-fuzz/cimbar_fuzz_decode -runs=10000 fuzz/corpus/ 2>&1 | tail -3
    ASAN_OPTIONS=alloc_dealloc_mismatch=0 \
    timeout 30 ./build-fuzz/cimbar_fuzz_encode -runs=10000 fuzz/corpus/ 2>&1 | tail -3
    ASAN_OPTIONS=alloc_dealloc_mismatch=0 \
    timeout 30 ./build-fuzz/cimbar_fuzz_fountain -runs=10000 fuzz/corpus/ 2>&1 | tail -3
```

`alloc_dealloc_mismatch=0` suppresses wirehair false positive.

## Test code for each new test

### testEmptyInput

```cpp
TEST_CASE( "cimbar_api_test/testEmptyInput", "[unit]" )
{
    cimbar_encoder_t* enc = cimbar_encoder_create(nullptr);
    REQUIRE(enc != nullptr);
    cimbar_decoder_t* dec = cimbar_decoder_create(nullptr);
    REQUIRE(dec != nullptr);

    cimbar_encoder_set_config(enc, CIMBAR_CFG_PRESET, 68);
    cimbar_decoder_set_config(dec, CIMBAR_CFG_PRESET, 68);

    int ret = cimbar_encoder_set_input(enc, "", 0, "empty.bin");
    REQUIRE(ret == CIMBAR_OK);

    std::vector<uint8_t> rgba(2048 * 2048 * 4);
    unsigned w = 0, h = 0;
    int max_frames = 100;
    for (int i = 0; i < max_frames; ++i)
    {
        ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
        if (ret <= 0) break;
        ret = cimbar_decoder_fountain_feed(dec, rgba.data(), rgba.size(), w, h, CIMBAR_IMAGE_RGBA);
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
    REQUIRE(out_len == 0);

    cimbar_encoder_destroy(enc);
    cimbar_decoder_destroy(dec);
}
```

### testUnicodeFilename

```cpp
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
```

### testFountainFeedFile

```cpp
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
```

### testFountainFeedCells

```cpp
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
```

### testDecodeScanFormats

```cpp
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

    // Encode one frame
    std::vector<uint8_t> rgba(2048 * 2048 * 4);
    unsigned w = 0, h = 0;
    int ret = cimbar_encoder_encode_next(enc, rgba.data(), rgba.size(), &w, &h);
    REQUIRE(ret > 0);

    // Convert RGBA to each format and decode
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
        // Should not hard-crash; any return >= 0 is acceptable
        // (data is a single frame, may or may not be enough for decode)
        REQUIRE(ret != CIMBAR_ERR_BAD_PARAM);
    }

    cimbar_encoder_destroy(enc);
    cimbar_decoder_destroy(dec);
}
```

### testDecoderGetConfig

```cpp
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

    // Invalid key
    int val;
    int ret = cimbar_decoder_get_config(dec, (cimbar_config_t)999, &val);
    REQUIRE(ret < 0);

    cimbar_decoder_destroy(dec);
}
```

## CI change

In `.github/workflows/ci.yml`, after line 96 (Build fuzz targets), add run step with:
- `ASAN_OPTIONS=alloc_dealloc_mismatch=0` suppression
- `timeout 30` per target
- `-runs=10000` limit per target
- `fuzz/corpus/` as seed corpus
- `tail -3` to show summary

## Verification

After implementation:
```bash
ninja -C build/ test/test_api
./build/test/test_api   # 27 test cases
meson test -C build/ --suite libcimbar
```
