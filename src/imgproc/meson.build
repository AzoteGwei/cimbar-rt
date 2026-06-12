# ==================== imgproc/hash - perceptual hashing ====================
imgproc_hash_dep = declare_dependency(
  include_directories: include_directories('hash'),
)

# ==================== imgproc/color - chromatic adaptation ====================
imgproc_color_dep = declare_dependency(
  include_directories: include_directories('color'),
)

# ==================== imgproc/extract - cell extraction (static library) ====================
imgproc_extract_src = files(
  'extract/Deskewer.cpp',
  'extract/Extractor.cpp',
  'extract/Scanner.cpp',
  'extract/SimpleCameraCalibration.cpp',
)

imgproc_extract_lib = static_library('extractor',
  imgproc_extract_src,
  pic: true,
  include_directories: [include_directories('extract'), src_inc, src_lib_inc, third_party_inc],
  dependencies: [opencv_dep, support_os_dep, libpopcnt_dep],
)

imgproc_extract_dep = declare_dependency(
  link_with: imgproc_extract_lib,
  include_directories: [include_directories('extract'), src_inc, src_lib_inc, third_party_inc],
)
