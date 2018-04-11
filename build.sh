#!/bin/bash
set -e

git submodule update --init --recursive
mkdir -p build
cd build
echo
echo cmake ...
cmake ..
echo
echo make ...
NPROC=${NPROC:-$(nproc)}
make -j$NPROC
