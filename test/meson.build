test_inc = include_directories('.')
test_src_inc = include_directories('..')

# ==================== support tests ====================
test_support_src = files(
  'support/test.cpp',
  'support/bitbufferTest.cpp',
  'support/bitreaderTest.cpp',
)

test_support_exe = executable('test_support',
  test_support_src,
  include_directories: [test_inc, test_src_inc, src_inc, third_party_inc],
  dependencies: [support_bit_dep, support_text_dep, support_os_dep, opencv_dep],
)
test('support', test_support_exe)

# ==================== imgproc tests ====================
test_imgproc_src = files(
  'imgproc/test.cpp',
  'imgproc/color_correctionTest.cpp',
  'imgproc/CornersTest.cpp',
  'imgproc/DeskewerTest.cpp',
  'imgproc/ExtractorTest.cpp',
  'imgproc/ScannerTest.cpp',
  'imgproc/ScanStateTest.cpp',
  'imgproc/SimpleCameraCalibrationTest.cpp',
  'imgproc/UndistortTest.cpp',
  'imgproc/averageHashTest.cpp',
  'imgproc/bitExtractorTest.cpp',
  'imgproc/fuzzyAhashTest.cpp',
)

test_imgproc_deps = [
  opencv_dep,
  support_bit_dep, support_os_dep, support_text_dep,
  core_codec_dep,
  imgproc_extract_dep, imgproc_hash_dep, imgproc_color_dep,
]

test_imgproc_exe = executable('test_imgproc',
  test_imgproc_src,
  include_directories: [test_inc, test_src_inc, src_inc, third_party_inc],
  dependencies: test_imgproc_deps,
)
test('imgproc', test_imgproc_exe)

# ==================== core tests (codec + fountain + compression) ====================
test_core_src = files(
  'core/test.cpp',
  'core/AdjacentCellFinderTest.cpp',
  'core/CellDriftTest.cpp',
  'core/CellPositionsTest.cpp',
  'core/CellTest.cpp',
  'core/CimbDecoderTest.cpp',
  'core/CimbEncoderTest.cpp',
  'core/CimbReaderTest.cpp',
  'core/CimbWriterTest.cpp',
  'core/FloodDecodePositionsTest.cpp',
  'core/InterleaveTest.cpp',
  'core/LinearDecodePositionsTest.cpp',
  'core/FountainEncodingTest.cpp',
  'core/FountainMetadataTest.cpp',
  'core/fountain_sinkSpecialTest.cpp',
  'core/fountain_sinkTest.cpp',
  'core/fountain_streamTest.cpp',
  'core/zstd_compressorTest.cpp',
  'core/zstd_decompressorTest.cpp',
  'core/zstd_header_checkTest.cpp',
)

test_core_deps = [
  opencv_dep,
  support_bit_dep, support_os_dep, support_text_dep,
  core_codec_dep, core_fountain_dep, core_compression_dep, core_reedsolomon_dep,
  imgproc_hash_dep, imgproc_color_dep,
  wirehair_dep, zstd_dep, cpp_fs_dep,
]

test_core_exe = executable('test_core',
  test_core_src,
  include_directories: [test_inc, test_src_inc, src_inc, third_party_inc],
  dependencies: test_core_deps,
)
test('core', test_core_exe)

# ==================== pipeline tests (encoder/decoder) ====================
test_pipeline_src = files(
  'pipeline/test.cpp',
  'pipeline/aligned_streamTest.cpp',
  'pipeline/DecoderTest.cpp',
  'pipeline/EncoderRoundTripTest.cpp',
  'pipeline/EncoderTest.cpp',
  'pipeline/escrow_buffer_writerTest.cpp',
  'pipeline/reed_solomon_streamTest.cpp',
)

test_pipeline_deps = [
  opencv_dep,
  core_codec_dep, core_compression_dep, core_fountain_dep, core_reedsolomon_dep,
  imgproc_extract_dep, imgproc_hash_dep,
  pipeline_encode_dep, pipeline_decode_dep,
  support_bit_dep, support_os_dep, support_text_dep,
  correct_dep, wirehair_dep, zstd_dep, cpp_fs_dep,
  picosha2_dep,
]

test_pipeline_exe = executable('test_pipeline',
  test_pipeline_src,
  include_directories: [test_inc, test_src_inc, src_inc, third_party_inc],
  dependencies: test_pipeline_deps,
)
test('pipeline', test_pipeline_exe)

# ==================== API tests (cimbar_js + new C API) ====================
test_api_src = files(
  'api/test.cpp',
  'api/cimbar_jsTest.cpp',
  'api/cimbar_recv_jsTest.cpp',
  'api/cimbar_api_test.cpp',
)

test_api_deps = [
  api_dep,
  core_codec_dep, core_compression_dep,
  pipeline_encode_dep, pipeline_decode_dep,
  imgproc_extract_dep,
  support_os_dep, support_text_dep,
  correct_dep, wirehair_dep, zstd_dep,
  opencv_dep, cpp_fs_dep,
]

test_api_exe = executable('test_api',
  test_api_src,
  include_directories: [test_inc, test_src_inc, src_inc, third_party_inc],
  dependencies: test_api_deps,
)
test('api', test_api_exe)
