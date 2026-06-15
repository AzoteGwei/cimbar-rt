project('libcimbar', 'cpp', 'c',
  version: '0.1.0',
  meson_version: '>=0.56.0',
  default_options: [
    'cpp_std=c++17',
    'c_std=c99',
    'buildtype=debugoptimized',
    'b_lto=false',
    'warning_level=2',
  ],
)



cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')

libcimbar_root = meson.project_source_root()

# ==================== Options ====================
wasm_mode = get_option('wasm')
portable_linux = get_option('portable_linux')
disable_tests = get_option('disable_tests')

# ==================== Global defines ====================
add_project_arguments(
  '-DLIBCIMBAR_PROJECT_ROOT="' + libcimbar_root + '"',
  language: 'cpp',
)

# ==================== Build type flags ====================
# C++ flags: Wpedantic, Wdeprecated added manually (not in warning_level)
add_project_arguments('-Wpedantic', '-Wdeprecated', language: 'cpp')
add_project_arguments('-Wpedantic', language: 'c')

# Required for portable builds / WASM
if get_option('default_library') == 'static' or portable_linux
  add_project_arguments('-fPIC', language: ['c', 'cpp'])
endif

# ==================== WASM mode ====================
is_wasm = wasm_mode != 0
if is_wasm
  add_project_arguments('-DGF256_TARGET_MOBILE', language: 'cpp')
  if wasm_mode == 1
    link_wasm_opt = ['-s', 'WASM=1']
  elif wasm_mode == 2
    link_wasm_opt = ['-s', 'SINGLE_FILE=1', '--closure', '1']
  endif
  disable_tests = true
endif

# ==================== Portable Linux ====================
if portable_linux
  add_project_arguments('-static-libstdc++', '-static-libgcc', language: 'cpp')
endif

# ==================== MSVC ====================
if host_machine.system() == 'windows'
  if cpp.get_id() == 'msvc'
    add_project_arguments('/FI', 'iso646.h', language: 'cpp')
  endif
endif

# ==================== Dependencies ====================
if is_wasm
  opencv_dir = get_option('opencv_dir')
  if opencv_dir == ''
    error('opencv_dir must be set for WASM builds (-Dopencv_dir=/path/to/opencv4)')
  endif
  opencv_wasm_build = opencv_dir / 'opencv-build-wasm' / 'build_wasm'

  # OpenCV WASM header paths: top-level include + all module includes
  opencv_mod_incs = run_command(
    find_program('bash'), '-c', 'echo ' + opencv_dir / 'modules/*/include',
    check: false,
  ).stdout().strip().split()
  opencv_inc_args = [
    '-I' + opencv_dir / 'include',
    '-I' + opencv_wasm_build,
  ]
  foreach d : opencv_mod_incs
    if d != ''
      opencv_inc_args += '-I' + d
    endif
  endforeach

  # OpenCV WASM static libraries
  opencv_lib_dir = opencv_wasm_build / 'lib'
  opencv_a_glob = run_command(
    find_program('bash'), '-c', 'echo ' + opencv_lib_dir + '/*.a',
    check: false,
  ).stdout().strip().split()
  opencv_link_args = []
  foreach f : opencv_a_glob
    if f != ''
      opencv_link_args += f
    endif
  endforeach

  opencv_dep = declare_dependency(
    compile_args: opencv_inc_args,
    link_args: opencv_link_args,
  )
else
  if portable_linux
    opencv_dep = dependency('opencv4', method: 'pkg-config', static: true)
  else
    opencv_dep = dependency('opencv4', method: 'pkg-config', required: false)
    if not opencv_dep.found()
      opencv_dep = dependency('OpenCV', method: 'cmake', required: true)
    endif
  endif
endif

# c++fs for old GCC
old_gcc = cpp.get_id() == 'gcc' and cpp.version().version_compare('<9')
if old_gcc
  cpp_fs_dep = cpp.find_library('stdc++fs', required: false)
else
  cpp_fs_dep = dependency('', required: false)
endif

glfw_dep = dependency('glfw3', required: false)
gl_dep = dependency('gl', required: false)

# ==================== Include paths ====================
src_inc = include_directories('src')
third_party_inc = include_directories('3rdparty')

# ==================== Fuzz mode (libFuzzer, clang only) ====================
# Apply globally so all dependencies have coverage instrumentation.
if get_option('fuzz')
  if cpp.get_id() != 'clang'
    error('fuzz targets require clang (libFuzzer)')
  endif
  add_project_arguments(['-fsanitize=fuzzer,address'], language: ['c', 'cpp'])
  add_project_link_arguments(['-fsanitize=fuzzer,address'], language: ['c', 'cpp'])
endif

# ==================== Subprojects ====================
subdir('3rdparty')
subdir('src/support')
subdir('src/imgproc')
subdir('src/core')
subdir('src/pipeline')
subdir('src/api')
if not is_wasm and not get_option('fuzz')
  subdir('tools')
endif
if not disable_tests and not get_option('fuzz')
  subdir('test')
endif

# ==================== Fuzz targets ====================
if get_option('fuzz')
  fuzz_deps = [
    api_dep,
    core_codec_dep, core_compression_dep,
    pipeline_encode_dep, pipeline_decode_dep,
    imgproc_extract_dep,
    support_os_dep, support_text_dep,
    correct_dep, wirehair_dep, zstd_dep,
    opencv_dep, cpp_fs_dep,
  ]

  executable('cimbar_fuzz_decode',
    'fuzz/cimbar_fuzz_decode.cc',
    include_directories: [src_inc, third_party_inc],
    dependencies: fuzz_deps,
  )

  executable('cimbar_fuzz_fountain',
    'fuzz/cimbar_fuzz_fountain.cc',
    include_directories: [src_inc, third_party_inc],
    dependencies: fuzz_deps,
  )

  executable('cimbar_fuzz_encode',
    'fuzz/cimbar_fuzz_encode.cc',
    include_directories: [src_inc, third_party_inc],
    dependencies: fuzz_deps,
  )
endif
