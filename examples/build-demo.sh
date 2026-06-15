#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# Build cimbar_demo against either an installed libcimbar or a local build tree.
#
# Installed (default -- requires meson install):
#   ./build-demo.sh
#
# In-tree (point BUILD_DIR at a meson build directory):
#   BUILD_DIR=../build ./build-demo.sh
#
# Cross-compile (override CC, CFLAGS, LDFLAGS):
#   CC=aarch64-linux-gnu-gcc ./build-demo.sh
set -e

DEMO_SRC="$(dirname "$0")/cimbar_demo.c"
BUILD="${BUILD_DIR:-}"

if [ -n "$BUILD" ]; then
	# ----- in-tree build (static libs from meson build tree) -----
	CIMBAR_INC="-I$(dirname "$0")/../include"
	CIMBAR_LIB="-L$BUILD/src/api -L$BUILD/src/core -L$BUILD/src/imgproc -L$BUILD/3rdparty"
	# Order matters for static linking:
	CIMBAR_LIBS="-lcimbar_js -lcimb_translator -lextractor -lwirehair -lzstd -lcorrect_static"

	PKGS="opencv4 glfw3 gl"
	CFLAGS="-Wall -Wextra -std=c11 -g -O2 $CIMBAR_INC $(pkg-config --cflags $PKGS)"
	LDFLAGS="$CIMBAR_LIB $CIMBAR_LIBS $(pkg-config --libs $PKGS) -lstdc++ -lm"

	echo "++ gcc $CFLAGS $DEMO_SRC $LDFLAGS -o cimbar-demo"
	gcc $CFLAGS $DEMO_SRC $LDFLAGS -o cimbar-demo
else
	# ----- installed build (assumes meson install to /usr/local) -----
	PREFIX="${CIMBAR_PREFIX:-/usr/local}"
	CIMBAR_INC="-I$PREFIX/include"
	CIMBAR_LIB="-L$PREFIX/lib"

	PKGS="opencv4 glfw3 gl"
	CFLAGS="-Wall -Wextra -std=c11 -g -O2 $CIMBAR_INC $(pkg-config --cflags $PKGS)"
	LDFLAGS="$CIMBAR_LIB $(pkg-config --libs $PKGS) -lcimbar_js -lcimb_translator -lextractor -lwirehair -lzstd -lcorrect_static -lstdc++ -lm"

	echo "++ gcc $CFLAGS $DEMO_SRC $LDFLAGS -o cimbar-demo"
	gcc $CFLAGS $DEMO_SRC $LDFLAGS -o cimbar-demo
fi

echo "++ done: ./cimbar-demo"
