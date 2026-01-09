#!/bin/bash

set -e

SCRIPT_DIR=$(dirname "$(realpath "$0")")

# build sqlite
cd $SCRIPT_DIR/../third_parties/sqlite
if [ -d "build" ]; then
    echo "sqlite already built"
else
    mkdir -p build && cd build
    ../configure --enable-all CFLAGS='-O3'
    make -j
    # make install
fi
