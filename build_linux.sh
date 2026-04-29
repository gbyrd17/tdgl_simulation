#!/bin/bash

set -e  # Exit on error

echo "============================================"
echo "TDGL Simulation - Linux Build Script"
echo "============================================"
echo ""

if [ -z "$VCPKG_ROOT" ]; then
    echo "Error: VCPKG_ROOT environment variable is not set."
    echo "Please install vcpkg and set VCPKG_ROOT to the installation directory."
    echo ""
    echo "Example:"
    echo "  git clone https://github.com/microsoft/vcpkg.git"
    echo "  cd vcpkg"
    echo "  ./bootstrap-vcpkg.sh"
    echo "  export VCPKG_ROOT=\$(pwd)"
    echo "  echo 'export VCPKG_ROOT=\$(pwd)' >> ~/.bashrc"
    echo ""
    exit 1
fi

echo "Using vcpkg from: $VCPKG_ROOT"
echo ""

if command -v apt-get &> /dev/null; then
    echo "Checking system dependencies..."
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        libx11-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libgl1-mesa-dev
    echo ""
fi

mkdir -p build
cd build

echo "Running CMake configuration..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja

echo ""
echo "Building project..."
cmake --build . --config Release

echo ""
echo "============================================"
echo "Build completed successfully!"
echo "Executable: build/tdgl_simulation"
echo "============================================"
echo ""
echo "To run the simulation:"
echo "  ./build/tdgl_simulation"
echo ""

cd ..
