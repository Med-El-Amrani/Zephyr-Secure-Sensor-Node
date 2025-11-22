#!/bin/bash
# Build script for Secure Sensor Node

set -e

# Configuration
BOARD="esp32s3_devkitc/esp32s3/procpu"
BUILD_DIR="build"
PRISTINE=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--pristine)
            PRISTINE="-p"
            shift
            ;;
        -b|--board)
            BOARD="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-p|--pristine] [-b|--board BOARD]"
            exit 1
            ;;
    esac
done

echo "========================================="
echo " Building Secure Sensor Node"
echo "========================================="
echo "Board: $BOARD"
echo "Build dir: $BUILD_DIR"
echo ""

# Build with west
if [ -n "$PRISTINE" ]; then
    echo "Performing pristine build..."
    west build -b $BOARD $PRISTINE
else
    west build -b $BOARD
fi

echo ""
echo "========================================="
echo " Build completed successfully!"
echo "========================================="
echo ""
echo "Firmware located at: $BUILD_DIR/zephyr/zephyr.bin"
echo ""
echo "Next steps:"
echo "  1. Flash: ./scripts/flash.sh"
echo "  2. Monitor: west espressif monitor"
echo ""