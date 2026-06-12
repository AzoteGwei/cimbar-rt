project('libcimbar', 'cpp', 'c',
  version: '0.1.0',
  meson_version: '>=0.56.0',
  default_options: [
    'cpp_std=c++17',
    'c_std=c99',
    'buildtype=debugoptimized',
    'b_lto=true',
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
  opencv_libs = []
  opencv_inc = []
  # We handle OpenCV for WASM via custom find
  opencv_dep = dependency('', required: false)
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

# ==================== Subprojects ====================
subdir('3rdparty')
subdir('src/support')
subdir('src/imgproc')
subdir('src/core')
subdir('src/pipeline')
subdir('src/api')
if not is_wasm
  subdir('tools')
endif
#if not disable_tests
#  subdir('test')
#endif
