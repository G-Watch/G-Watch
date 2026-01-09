#!/bin/bash

set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")

# build pybind11
cd $SCRIPT_DIR/../third_parties/pybind11
if [ -d "build" ]; then
    echo "pybind11 already built and installed"
else
    rm -rf build
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j
    make install
fi

# build and install nlohmann_json
cd $SCRIPT_DIR/../third_parties/json
if [ -d "build" ]; then
    echo "nlohmann_json already built and installed"
else
    rm -rf build
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j
    make install
fi

# build and install pybind11_json
cd $SCRIPT_DIR/../third_parties/pybind11_json
if [ -d "build" ]; then
    echo "pybind11_json already built and installed"
else
    rm -rf build
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j
    make install
fi
