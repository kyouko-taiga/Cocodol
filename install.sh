#!/bin/sh

CC=cc
LIB_DIR=/usr/local/lib/
BIN_DIR=/usr/local/bin/

# Exit when any command fails
set -e

# Compile the runtime library.
cd Runtime
make
cd ..

# Compile cocodoc.
swift build -c release

# Install the binaries.
cp Runtime/build/libcocodol_rt.a ${LIB_DIR}
cp .build/release/cocodoc ${BIN_DIR}

echo "Runtime library installed at ${LIB_DIR}/libcocodol_rt.a"
echo "Compiler binary installed at ${BIN_DIR}/cocodoc"
