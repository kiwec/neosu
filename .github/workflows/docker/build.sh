#!/bin/bash

echo "Building for target: $HOST"
set -e

export MAKEFLAGS="-j$(nproc)"
export CCACHE_DIR="${CCACHE_DIR:-"/src/.ccache"}"

# this is required for ccache to survive compiler reinstalls
export CCACHE_COMPILERCHECK="none"

# Building..
./autogen.sh
cd build-out
#../configure --disable-system-deps --enable-static --disable-native --with-audio="bass,soloud" --host=$HOST
../configure --host=$HOST
make install

# symlinks turn into copies in .zip files for GHA artifacts, so we need to make a zip of the zip...
zip -r -y -8 bin.zip ./dist/bin-*/
rm -rf dist/*
mv bin.zip dist/

echo "Done!"
