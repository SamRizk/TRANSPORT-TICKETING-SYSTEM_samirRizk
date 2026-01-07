#!/bin/bash
# scripts/build.sh - Build the project

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"
CLEAN_BUILD="${CLEAN_BUILD:-false}"

echo -e "${BLUE}╔════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   Transport Ticketing System Builder  ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════╝${NC}"
echo ""
echo "Build type: $BUILD_TYPE"
echo ""

# Check prerequisites
echo -e "${YELLOW}[1/6] Checking prerequisites...${NC}"

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}✗ CMake is not installed${NC}"
    echo "Install with: sudo apt-get install cmake"
    exit 1
fi
echo -e "${GREEN}✓ CMake: $(cmake --version | head -n1)${NC}"

if ! command -v g++ &> /dev/null; then
    echo -e "${RED}✗ G++ compiler is not installed${NC}"
    echo "Install with: sudo apt-get install build-essential"
    exit 1
fi
echo -e "${GREEN}✓ G++: $(g++ --version | head -n1)${NC}"

echo ""

# Clean build if requested
if [ "$CLEAN_BUILD" = "true" ] || [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}[2/6] Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}✓ Build directory cleaned${NC}"
    echo ""
fi

# Create build directory
echo -e "${YELLOW}[3/6] Creating build directory...${NC}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
echo -e "${GREEN}✓ Build directory ready${NC}"
echo ""

# Run CMake
echo -e "${YELLOW}[4/6] Running CMake configuration...${NC}"
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ..

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ CMake configuration complete${NC}"
else
    echo -e "${RED}✗ CMake configuration failed${NC}"
    exit 1
fi
echo ""

# Build the project
echo -e "${YELLOW}[5/6] Building project...${NC}"
CPU_CORES=$(nproc 2>/dev/null || echo 2)
echo "Using $CPU_CORES CPU cores"

make -j"$CPU_CORES"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build completed successfully${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
echo ""

# Display artifacts
echo -e "${YELLOW}[6/6] Build artifacts:${NC}"
if [ -d "bin" ]; then
    ls -lh bin/
    echo ""
    echo -e "${GREEN}Built binaries:${NC}"
    for binary in bin/*; do
        if [ -x "$binary" ]; then
            echo "  - $(basename $binary)"
        fi
    done
else
    echo -e "${RED}No binaries found${NC}"
fi
echo ""

# Success summary
echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║          Build Successful!             ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
echo ""
echo "Binaries location: $BUILD_DIR/bin/"
echo ""
echo -e "${BLUE}Next steps:${NC}"
echo "1. Start services with Docker:"
echo "   docker-compose up --build"
echo ""
echo "2. Or run manually:"
echo "   Terminal 1: ./build/bin/backoffice"
echo "   Terminal 2: ./build/bin/tvm tcp://localhost:1883 http://localhost:8080"
echo "   Terminal 3: ./build/bin/gate 001 tcp://localhost:1883 http://localhost:8080"
