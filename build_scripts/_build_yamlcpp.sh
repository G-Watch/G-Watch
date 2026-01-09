#!/bin/bash

set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")

# build yaml-cpp
cd $SCRIPT_DIR/../third_parties/yaml-cpp
if [ -d "build" ]; then
    echo "yaml-cpp already built"
else
    mkdir build
    cd build
    cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release ..
    make -j
fi
