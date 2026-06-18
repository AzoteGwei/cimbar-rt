cv_bridge_lib = static_library('cv_bridge',
  files('cv_bridge.cc'),
  pic: true,
  include_directories: include_directories('.'),
  dependencies: [opencv_dep],
)

cv_bridge_dep = declare_dependency(
  link_with: cv_bridge_lib,
  include_directories: include_directories('.'),
)
