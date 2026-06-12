# ==================== C API implementation (was cimbar_js) ====================
api_deps = [
  core_codec_dep,
  core_compression_dep,
  core_fountain_dep,
  pipeline_encode_dep,
  pipeline_decode_dep,
  imgproc_extract_dep,
  support_os_dep,
  support_text_dep,
  support_display_dep,
  correct_dep,
  wirehair_dep,
  zstd_dep,
  opencv_dep,
  cpp_fs_dep,
]

if is_wasm
  # WASM: emit an executable with emscripten flags
  wasm_sources = files('cimbar_js.cpp', 'cimbar_js.h')
  if wasm_mode != 2
    wasm_sources += files('cimbar_recv_js.cpp', 'cimbar_recv_js.h')
  endif

  wasm_exported_fns = [
    '_malloc', '_free',
    '_cimbare_render', '_cimbare_next_frame', '_cimbare_init_encode',
    '_cimbare_encode', '_cimbare_encode_bufsize', '_cimbare_configure',
    '_cimbare_rotate_window', '_cimbare_get_aspect_ratio',
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
    '--bind', '-Os',
    '-s', 'USE_WEBGL2=1',
    '-s', 'MIN_WEBGL_VERSION=2',
    '-s', 'MAX_WEBGL_VERSION=2',
    '-s', 'USE_GLFW=3',
    '-s', 'WASM_BIGINT=1',
    '-s', 'FILESYSTEM=0',
    '-s', 'TOTAL_MEMORY=134217728',
    '-s', 'EXPORTED_FUNCTIONS=[' + ','.join(wasm_exported_fns) + ']',
    '-s', 'EXPORTED_RUNTIME_METHODS=[HEAPU8]',
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
  # Native static library
  api_sources = files(
    'cimbar_js.cpp', 'cimbar_js.h',
    'cimbar_recv_js.cpp', 'cimbar_recv_js.h',
  )

  api_lib = static_library('cimbar_js',
    api_sources,
    pic: true,
    include_directories: [src_inc, third_party_inc],
    dependencies: api_deps + [glfw_dep, gl_dep],
  )

  api_dep = declare_dependency(
    link_with: api_lib,
    include_directories: [src_inc, third_party_inc],
  )
endif
