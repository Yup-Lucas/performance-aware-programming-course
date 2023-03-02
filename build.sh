#!/usr/bin/env bash

set -e

if [ -d "build" ]; then
rm -rf build
fi

mkdir build

pushd build

clang -std=c17 -Wall -Werror -g ../disasm8086.c -o disasm8086

popd
