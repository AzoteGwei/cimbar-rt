#!/bin/sh
# Build the C API demo against the installed library.
# Requires meson install to have run, or be in-tree.
set -e

DEMO="$(dirname "$0")/cimbar_demo.c"
BUILD="${BUILD_DIR:-build}"

# in-tree lib paths
CIMBAR_INC="-I$(dirname "$0")/../include"
CIMBAR_LIB="-L$(dirname "$0")/../$BUILD/src/api"
CIMBAR_LIBS="-lcimbar_js -lzstd -lwirehair -lfmt -lpopcnt"

PKGS="opencv4 glfw3 gl"

CFLAGS="-Wall -Wextra -std=c11 -g -O2"
LDFLAGS="$CIMBAR_LIB $CIMBAR_LIBS $(pkg-config --libs $PKGS) -lstdc++"

echo "++ gcc $CFLAGS $CIMBAR_INC $DEMO $LDFLAGS -o cimbar-demo"
gcc $CFLAGS $CIMBAR_INC $DEMO $LDFLAGS -o cimbar-demo
echo "++ done: ./cimbar-demo"
