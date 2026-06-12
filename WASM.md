## Demo encoder

[cimbar.org](https://cimbar.org)

## Releases

wasm and asm.js releases are available [here](https://github.com/sz3/libcimbar/releases/latest). The wasm build is what cimbar.org uses. [cimbar_js.html](https://github.com/sz3/libcimbar/releases/latest/cimbar_js.html) can be downloaded and opened/run in a local web browser -- no install required.

## Build

To build, use the `scripts/package-wasm.sh` script in a docker container:

```
docker run --mount type=bind,source="$(pwd)",target="/usr/src/app" -it emscripten/emsdk:3.1.39
```
Then, inside the container:
```
bash /usr/src/app/scripts/package-wasm.sh
```

## Alternative build for the adventurous

Alternatively, if you have a local emscripten setup, you can try to run the packaging commands piecemeal:

To build opencv.js:
```
cd /path/to/opencv
mkdir opencv-build-wasm
cd opencv-build-wasm
python3 ../platforms/js/build_js.py build_wasm --build_wasm --emscripten_dir=/path/to/emscripten
```

With opencv.js built:
```
source /path/to/emscripten/emsdk/emsdk_env.sh
meson setup build-wasm --cross-file build/wasm-cross.ini -Dwasm=1 -Dopencv_dir=/path/to/opencv
ninja -C build-wasm install
```

(use `-Dwasm=2` for asm.js instead of wasm)

## What about a WASM cimbar decoder?

Some day!
