# ==================== Header-only libraries ====================

fmt_dep = declare_dependency(
  include_directories: include_directories('fmt'),
)

cxxopts_dep = declare_dependency(
  include_directories: include_directories('cxxopts'),
)

intx_dep = declare_dependency(
  include_directories: include_directories('intx'),
)

base91_dep = declare_dependency(
  include_directories: include_directories('base91'),
)

libpopcnt_dep = declare_dependency(
  include_directories: include_directories('libpopcnt'),
)

concurrentqueue_dep = declare_dependency(
  include_directories: include_directories('concurrentqueue'),
)

picosha2_dep = declare_dependency(
  include_directories: include_directories('PicoSHA2'),
)

stb_dep = declare_dependency(
  include_directories: include_directories('stb'),
)

# ==================== Wirehair (static library) ====================

wirehair_sources = files(
  'wirehair/wirehair.cpp',
  'wirehair/gf256.cpp',
  'wirehair/WirehairCodec.cpp',
  'wirehair/WirehairTools.cpp',
)

wirehair_inc = include_directories('wirehair/include')

wirehair_cpp_args = []
if not is_wasm and host_machine.system() != 'android'
  wirehair_cpp_args += ['-march=native']
endif

wirehair_lib = static_library('wirehair', wirehair_sources,
  pic: true,
  include_directories: wirehair_inc,
  cpp_args: wirehair_cpp_args,
  override_options: ['cpp_std=c++11'],
)

wirehair_dep = declare_dependency(
  link_with: wirehair_lib,
  include_directories: wirehair_inc,
)

# ==================== Zstd (object library aggregation) ====================

zstd_common_src = files(
  'zstd/common/debug.c',
  'zstd/common/error_private.c',
  'zstd/common/entropy_common.c',
  'zstd/common/fse_decompress.c',
  'zstd/common/pool.c',
  'zstd/common/threading.c',
  'zstd/common/xxhash.c',
  'zstd/common/zstd_common.c',
)

zstd_compress_src = files(
  'zstd/compress/fse_compress.c',
  'zstd/compress/hist.c',
  'zstd/compress/huf_compress.c',
  'zstd/compress/zstd_compress.c',
  'zstd/compress/zstd_compress_literals.c',
  'zstd/compress/zstd_compress_sequences.c',
  'zstd/compress/zstd_compress_superblock.c',
  'zstd/compress/zstd_double_fast.c',
  'zstd/compress/zstd_fast.c',
  'zstd/compress/zstd_lazy.c',
  'zstd/compress/zstd_ldm.c',
  'zstd/compress/zstd_opt.c',
  'zstd/compress/zstdmt_compress.c',
)

zstd_decompress_src = files(
  'zstd/decompress/huf_decompress.c',
  'zstd/decompress/zstd_ddict.c',
  'zstd/decompress/zstd_decompress.c',
  'zstd/decompress/zstd_decompress_block.c',
)

zstd_decompress_args = ['-DZSTD_DISABLE_ASM']

zstd_common_lib = static_library('zstd-common', zstd_common_src, pic: true,)
zstd_compress_lib = static_library('zstd-compress', zstd_compress_src, pic: true,)
zstd_decompress_lib = static_library('zstd-decompress', zstd_decompress_src,
  pic: true,
  c_args: zstd_decompress_args,
)

zstd_lib = static_library('zstd',
  pic: true,
  objects: [
    zstd_common_lib.extract_all_objects(recursive: false),
    zstd_compress_lib.extract_all_objects(recursive: false),
    zstd_decompress_lib.extract_all_objects(recursive: false),
  ],
  c_args: ['-DZSTD_STATIC_LINKING_ONLY'],
)

zstd_dep = declare_dependency(
  link_with: zstd_lib,
  include_directories: include_directories('zstd/common'),
)

# ==================== Libcorrect (object library aggregation) ====================

correct_rs_src = files(
  'libcorrect/src/reed-solomon/polynomial.c',
  'libcorrect/src/reed-solomon/reed-solomon.c',
  'libcorrect/src/reed-solomon/encode.c',
  'libcorrect/src/reed-solomon/decode.c',
)

correct_conv_src = files(
  'libcorrect/src/convolutional/bit.c',
  'libcorrect/src/convolutional/metric.c',
  'libcorrect/src/convolutional/history_buffer.c',
  'libcorrect/src/convolutional/error_buffer.c',
  'libcorrect/src/convolutional/lookup.c',
  'libcorrect/src/convolutional/convolutional.c',
  'libcorrect/src/convolutional/encode.c',
  'libcorrect/src/convolutional/decode.c',
)

correct_conv_sse_src = files(
  'libcorrect/src/convolutional/sse/lookup.c',
  'libcorrect/src/convolutional/sse/convolutional.c',
  'libcorrect/src/convolutional/sse/encode.c',
  'libcorrect/src/convolutional/sse/decode.c',
)

correct_inc = include_directories('libcorrect/include')

# SSE 4.1 detection
sse_code = '''
#include <x86intrin.h>
int main() {
  __m128i a, b;
  __m128i c = _mm_min_epu16(a, b);
  return 0;
}
'''

have_sse = cc.compiles(sse_code, args: ['-march=native'], name: 'SSE 4.1 check')

correct_c_args = ['-std=c99', '-Wall']
if cc.has_argument('-Wpedantic')
  correct_c_args += ['-Wpedantic']
endif

correct_rs_lib = static_library('correct-reed-solomon', correct_rs_src,
  pic: true,
  c_args: correct_c_args,
  include_directories: correct_inc,
)
correct_conv_lib = static_library('correct-convolutional', correct_conv_src,
  pic: true,
  c_args: correct_c_args,
  include_directories: correct_inc,
)

correct_obj_files = [
  correct_rs_lib.extract_all_objects(recursive: false),
  correct_conv_lib.extract_all_objects(recursive: false),
]

if have_sse
  correct_conv_sse_lib = static_library('correct-convolutional-sse', correct_conv_sse_src,
    pic: true,
    c_args: correct_c_args + ['-msse4.1'],
    include_directories: correct_inc,
  )
  correct_obj_files += [correct_conv_sse_lib.extract_all_objects(recursive: false)]
  correct_sse_def = '-DHAVE_SSE=1'
else
  correct_sse_def = ''
endif

correct_lib = static_library('correct_static',
  objects: correct_obj_files,
  pic: true,
  c_args: correct_c_args,
)

correct_dep = declare_dependency(
  link_with: correct_lib,
  include_directories: correct_inc,
  compile_args: correct_sse_def,
)
