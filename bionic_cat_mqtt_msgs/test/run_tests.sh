#!/bin/bash

# Script to build and run serializer tests
# Usage: ./run_tests.sh [clean]

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Serializer Test Build Script${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# Check if clean build requested
if [ "$1" == "clean" ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    echo -e "${GREEN}✓ Clean complete${NC}"
    echo
fi

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Configure with tests enabled
echo -e "${YELLOW}Configuring CMake with BUILD_TESTS=ON...${NC}"
cd "$BUILD_DIR"
cmake -DBUILD_TESTS=ON .. > /dev/null 2>&1 || {
    echo -e "${RED}✗ CMake configuration failed${NC}"
    exit 1
}
echo -e "${GREEN}✓ Configuration complete${NC}"
echo

# Build test executable
echo -e "${YELLOW}Building test_serializer...${NC}"
make test_serializer -j$(nproc) 2>&1 | grep -E "(Building|Linking|Built target|error|warning)" || true
if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Build complete${NC}"
echo

# Run tests
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Running Tests${NC}"
echo -e "${BLUE}========================================${NC}"
echo

./bin/test_serializer
TEST_RESULT=$?

echo
if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  All tests completed successfully!${NC}"
    echo -e "${GREEN}========================================${NC}"
else
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}  Tests failed with exit code: $TEST_RESULT${NC}"
    echo -e "${RED}========================================${NC}"
    exit $TEST_RESULT
fi
