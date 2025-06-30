#!/bin/bash

# Save current working directory
ORIGINAL_DIR=$(pwd)

## If already inside build directory, skip everything
#if [[ $(basename "$PWD") == "build" ]]; then
#    echo "[INFO] Already inside build directory"
#else
#    # Check for CMakeLists.txt in current dir
#    if [[ -f "CMakeLists.txt" ]]; then
#        echo "[INFO] Found CMakeLists.txt"
#        [[ -d build ]] || mkdir build
#        cd build
#    elif [[ -f "build.sh" ]]; then
#        echo "[INFO] Found build.sh, going to project root"
#        cd ../..
#        [[ -d build ]] || mkdir build
#        cd build
#    else
#        echo "[ERROR] Neither CMakeLists.txt nor build.sh found"
#        exit 1
#    fi
#fi

# Determine the directory where the script itself is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Go to the script directory
cd "$SCRIPT_DIR" || exit 1
echo "[INFO] script directory: $(pwd)"
# going to project root
cd ../..
[[ -d build ]] || mkdir build
cd build

# Run cmake and build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Return to original directory
cd "$ORIGINAL_DIR"

echo "[INFO] Building done"