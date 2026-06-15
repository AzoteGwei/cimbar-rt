#!/bin/bash
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

#docker run --mount type=bind,source="$(pwd)",target="/usr/src/app" -it emscripten/emsdk:5.0.0 bash

SKIP_JS=${SKIP_JS:-}
CIMBAR_ROOT=${CIMBAR_ROOT:-/usr/src/app}
cd $CIMBAR_ROOT

apt update
apt install python3 python3-pip ninja-build ccache -y

pip3 install meson

if ls opencv4/opencv-build-wasm/build_wasm/lib/*.a >/dev/null 2>&1; then
    echo "OpenCV WASM build artifacts found, skipping OpenCV build..."
else
    cd opencv4/
    mkdir -p opencv-build-wasm
    cd opencv-build-wasm
    python3 ../platforms/js/build_js.py build_wasm --emscripten_dir=/emsdk/upstream/emscripten
    cd $CIMBAR_ROOT
fi

# --- WASM build ---
cd $CIMBAR_ROOT
meson setup build-wasm \
  --cross-file config/wasm-ccache.ini \
  --prefix $CIMBAR_ROOT/dist \
  -Dwasm=1 \
  -Dopencv_dir=$CIMBAR_ROOT/opencv4
ninja -C build-wasm install
cp $CIMBAR_ROOT/dist/bin/cimbar_js.js $CIMBAR_ROOT/web/
cp $CIMBAR_ROOT/dist/bin/cimbar_js.wasm $CIMBAR_ROOT/web/ 2>/dev/null || true
(cd $CIMBAR_ROOT/web/ && bash wasmgz.sh)

if [ -n "$SKIP_JS" ]; then
	echo "early exit"
	exit 0
fi

# --- asmjs build ---
cd $CIMBAR_ROOT
meson setup build-asmjs \
  --cross-file config/wasm-ccache.ini \
  --prefix $CIMBAR_ROOT/dist \
  -Dwasm=2 \
  -Dopencv_dir=$CIMBAR_ROOT/opencv4
ninja -C build-asmjs install
cp $CIMBAR_ROOT/dist/bin/cimbar_js.js $CIMBAR_ROOT/web/
(cd $CIMBAR_ROOT/web/ && zip cimbar.asmjs.zip cimbar_js.js index.html main.js)

(cd $CIMBAR_ROOT && python3 scripts/package-html.py)
