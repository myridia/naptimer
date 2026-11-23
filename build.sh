#!/bin/sh
set -e

rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local/nappgui
cmake --build build

case "$(uname)" in
  Darwin) open build/Release/bin/naptimer.app ;;
  *) build/Release/bin/naptimer ;;
esac
