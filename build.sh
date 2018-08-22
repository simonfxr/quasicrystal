#!/usr/bin/env bash

set -e

CXX=clang++
type -P clang++ &>/dev/null || CXX=c++

BUILD_TYPE=Release
[[ -z $1 ]] || BUILD_TYPE="$1"

cd "$(dirname "$0")"

d=build/"$BUILD_TYPE"

if [[ ! -d $d ]]; then
    mkdir -p "$d"
    cd "$d"
    cmake -DCMAKE_CXX_COMPILER="$CXX" \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=True \
          -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
          ../..
else
    cd "$d"
fi

exec cmake --build .
