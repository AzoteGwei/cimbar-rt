# ==================== core/fountain - fountain codec (header-only) ====================
core_fountain_dep = declare_dependency(
  include_directories: include_directories('fountain'),
)

# ==================== core/compression - zstd compression (header-only) ====================
core_compression_dep = declare_dependency(
  include_directories: [include_directories('compression'), third_party_inc],
)

# ==================== core/reedsolomon - Reed-Solomon error correction (header-only) ====================
core_reedsolomon_dep = declare_dependency(
  include_directories: include_directories('reedsolomon'),
)

# ==================== core/codec - cimb cell codec (static library) ====================
core_codec_src = files(
  'codec/AdjacentCellFinder.cpp',
  'codec/CellDrift.cpp',
  'codec/CellPositions.cpp',
  'codec/CimbDecoder.cpp',
  'codec/CimbEncoder.cpp',
  'codec/CimbReader.cpp',
  'codec/CimbWriter.cpp',
  'codec/Common.cpp',
  'codec/FloodDecodePositions.cpp',
)

core_codec_lib = static_library('cimb_translator',
  core_codec_src,
  pic: true,
  include_directories: [
    include_directories('codec'),
    src_inc, third_party_inc,
  ],
  dependencies: [
    opencv_dep,
    support_os_dep, support_bit_dep, support_text_dep, support_image_dep,
    imgproc_hash_dep, imgproc_color_dep,
    core_fountain_dep,
  ],
)

core_codec_dep = declare_dependency(
  link_with: core_codec_lib,
  include_directories: [
    include_directories('codec'),
    src_inc, third_party_inc,
  ],
)