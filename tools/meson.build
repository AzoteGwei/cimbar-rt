# ==================== CLI tools ====================

one_line = find_program('bash', required: false)

# Helper: strip debug symbols after build
strip_cmd = find_program('strip', required: false)
if strip_cmd.found()
  strip_dbg = '''
    tmp=$(basename "$1")
    cp "$1" "${0}/${tmp}.dbg"
    strip -g "$1"
  '''
else
  strip_dbg = ''
endif

# --- cimbar (combined encode/decode) ---
cimbar_deps = [
  core_codec_dep,
  imgproc_extract_dep,
  correct_dep,
  wirehair_dep,
  zstd_dep,
  opencv_dep,
  cpp_fs_dep,
]

executable('cimbar',
  'cimbar/cimbar.cpp',
  include_directories: [src_inc, third_party_inc],
  dependencies: cimbar_deps,
  install: true,
  install_dir: 'bin',
)



# --- cimbar-extract ---
executable('cimbar_extract',
  'cimbar-extract/cimbar_extract.cpp',
  include_directories: [src_inc, third_party_inc],
  dependencies: [
    core_codec_dep,
    imgproc_extract_dep,
    opencv_dep,
  ],
  install: true,
  install_dir: 'bin',
)

# --- build-image-assets (build tool, not installed by default) ---
executable('build_image_assets',
  'build-image-assets/build_image_assets.cpp',
  include_directories: [src_inc, third_party_inc],
  dependencies: [
    support_os_dep,
    support_text_dep,
  ],
  build_by_default: false,
)
