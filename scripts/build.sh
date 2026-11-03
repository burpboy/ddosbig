#!/bin/bash

# BrutalDDoS Build Script
# Ensures dependencies are present and builds the project efficiently.

set -e # Exit on any error

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
INSTALL_DIR="$PROJECT_ROOT"
BINARY_NAME="brutaldDos"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}[BrutalDDoS]${NC} Starting build process..."

# 1. Check for required tools
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}[ERROR]${NC} CMake is not installed. Please install it first."
    exit 1
fi

if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo -e "${RED}[ERROR]${NC} C++ compiler (g++ or clang++) not found."
    exit 1
fi

# 2. Initialize submodules if third_party exists but is empty
if [ -d "$PROJECT_ROOT/third_party" ]; then
    # If nlohmann/json.hpp or spdlog doesn't exist, try to init submodules
    # Assuming you might use git submodules for these libs. 
    # If they are just dropped files, this step is safe to skip or check existence.
    
    if [ ! -f "$PROJECT_ROOT/third_party/nlohmann/json.hpp" ] || [ ! -d "$PROJECT_ROOT/third_party/spdlog/include" ]; then
        echo -e "${YELLOW}[WARNING]${NC} Third-party libraries missing in third_party/. Trying to init submodules..."
        if command -v git &> /dev/null; then
            cd "$PROJECT_ROOT"
            git submodule update --init --recursive
            cd "$PROJECT_ROOT"
        else
            echo -e "${YELLOW}[WARNING]${NC} Git not found or no submodules. Ensure nlohmann/json.hpp and spdlog are manually placed in third_party/."
        fi
    fi
fi

# 3. Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 4. Configure CMake
echo -e "${GREEN}[BrutalDDoS]${NC} Configuring CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DSPDLOG_ROOT="$PROJECT_ROOT/third_party/spdlog" \
    -DJTHREADS_NLOHMANN_JSON_ROOT="$PROJECT_ROOT/third_party" \
    -DBUILD_TESTING=ON

# 5. Build
CORE_COUNT=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
echo -e "${GREEN}[BrutalDDoS]${NC} Compiling with $CORE_COUNT cores..."
cmake --build . --parallel "$CORE_COUNT"

# 6. Install/Copy Binary
if [ -f "./src/brutaldDos" ]; then
    BINARY_PATH="./src/brutaldDos"
elif [ -f "./brutaldDos" ]; then
    BINARY_PATH="./brutaldDos"
else
    echo -e "${RED}[ERROR]${NC} Binary 'brutaldDos' not found after build."
    exit 1
fi

cp "$BINARY_PATH" "$INSTALL_DIR/$BINARY_NAME"
chmod +x "$INSTALL_DIR/$BINARY_NAME"

echo -e "${GREEN}[SUCCESS]${NC} Build complete! Binary located at: $INSTALL_DIR/$BINARY_NAME"
echo -e "Run it with: ./$BINARY_NAME -t <target_ip> -p <port> -m all ..."
