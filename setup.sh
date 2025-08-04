#!/bin/bash

set -e

# Install essential build tools and dependencies
apt-get update
apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    unzip \
    pkg-config \
    libgl1-mesa-dev \
    libglu1-mesa-dev

# Clone and bootstrap vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
cd ..

# Install required libraries using vcpkg
./vcpkg/vcpkg install wxwidgets:x64-linux tinyxml2:x64-linux opengl nanovg:x64-linux wxpdfdoc:x64-linux

# Create build directory and configure the project with CMake
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug

# Build the project
make
