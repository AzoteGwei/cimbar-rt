#!/bin/bash
## targeting old glibc
# docker run --mount type=bind,source="$(pwd)",target="/usr/src/app" -it ubuntu:16.04

SCRIPT_DIR=$(dirname "${BASH_SOURCE[0]}")
cd $SCRIPT_DIR/..

# https://gist.github.com/jlblancoc/99521194aba975286c80f93e47966dc5
apt update
apt install -y software-properties-common
add-apt-repository -y ppa:ubuntu-toolchain-r/test

apt update
apt install -y pkgconf g++-7 python3-pip ninja-build
apt install -y libgles2-mesa-dev libglfw3-dev

# meson + cmake (via pip)
pip3 install meson cmake

# use gcc7
update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-7 100
update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-7 100

cd opencv4/
mkdir build-portable/ && cd build-portable/
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DBUILD_SHARED_LIBS=OFF -DOPENCV_GENERATE_PKGCONFIG=YES -DOPENCV_FORCE_3RDPARTY_BUILD=YES
make -j5 install

cd $SCRIPT_DIR/..
meson setup build-portable -Dportable_linux=true
ninja -C build-portable install
