# PowerShell setup script for Windows
$ErrorActionPreference = 'Stop'

# Clone and bootstrap vcpkg
if (-Not (Test-Path 'vcpkg')) {
    git clone https://github.com/microsoft/vcpkg.git
}
cd vcpkg
if (-Not (Test-Path '.\vcpkg.exe')) {
    .\bootstrap-vcpkg.bat
}
cd ..

# Install required libraries
.\vcpkg\vcpkg.exe install wxwidgets:x64-windows tinyxml2:x64-windows opengl nanovg:x64-windows

# Configure and build with CMake
if (-Not (Test-Path 'build')) {
    New-Item -ItemType Directory -Path 'build' | Out-Null
}
cd build
cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
