# ==================== C API implementation ====================
api_deps = [
  core_codec_dep,
  core_compression_dep,
  core_fountain_dep,
  pipeline_encode_dep,
  pipeline_decode_dep,
  imgproc_extract_dep,
  support_os_dep,
  support_text_dep,
  correct_dep,
  wirehair_dep,
  zstd_dep,
  opencv_dep,
  cpp_fs_dep,
]

if is_wasm
  # WASM: emit an executable with emscripten flags
  wasm_sources = files('cimbar_api.cc')

  wasm_exported_fns = [
    '_malloc', '_free',
    '_cimbar_encoder_create', '_cimbar_encoder_destroy',
    '_cimbar_encoder_set_config', '_cimbar_encoder_get_config',
    '_cimbar_encoder_set_input',
    '_cimbar_encoder_encode_next_cells',
    '_cimbar_encoder_reset', '_cimbar_encoder_get_stats',
    '_cimbar_render',
  ]

  if wasm_mode != 2
    wasm_exported_fns += [
      '_cimbard_get_report', '_cimbard_get_bufsize',
      '_cimbard_scan_extract_decode', '_cimbard_fountain_decode',
      '_cimbard_get_filesize', '_cimbard_get_filename',
      '_cimbard_get_decompress_bufsize', '_cimbard_decompress_read',
      '_cimbard_configure_decode',
    ]
  endif

  wasm_link_args = [
    '-Os',
    '-s', 'WASM_BIGINT=1',
    '-s', 'FILESYSTEM=0',
    '-s', 'TOTAL_MEMORY=134217728',
    '-s', 'EXPORTED_FUNCTIONS=[' + ','.join(wasm_exported_fns) + ']',
    '-s', 'EXPORTED_RUNTIME_METHODS=[HEAPU8,UTF8ToString,stringToUTF8]',
  ]

  if wasm_mode == 2
    wasm_link_args += ['-s', 'SINGLE_FILE=1', '--closure', '1']
  else
    wasm_link_args += ['-s', 'WASM=1']
  endif

  executable('cimbar_js',
    wasm_sources,
    dependencies: api_deps,
    link_args: wasm_link_args,
    install: true,
  )
else
  # Public C API header path
  pub_inc = include_directories('../../include')

  # Native static library
  api_sources = files(
    'cimbar_api.cc',
  )

  api_lib = static_library('cimbar_js',
    api_sources,
    pic: true,
    include_directories: [src_inc, third_party_inc, pub_inc],
    dependencies: api_deps,
  )

  api_dep = declare_dependency(
    link_with: api_lib,
    include_directories: [src_inc, third_party_inc, pub_inc],
  )

  # Install public headers
  install_headers(
    '../../include/libcimbar/cimbar.h',
    '../../include/libcimbar/cimbar_export.h',
    subdir: 'libcimbar',
  )
endif
