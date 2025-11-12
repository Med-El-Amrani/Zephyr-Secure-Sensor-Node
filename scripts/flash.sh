#!/bin/bash
# Flash script for Secure Sensor Node (ESP32-S3)

set -e

# Configuration
PORT="/dev/ttyUSB0"
BAUD="921600"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -b|--baud)
            BAUD="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-p|--port PORT] [-b|--baud BAUDRATE]"
            exit 1
            ;;
    esac
done

echo "========================================="
echo " Flashing Secure Sensor Node"
echo "========================================="
echo "Port: $PORT"
echo "Baudrate: $BAUD"
echo ""

# Check if build exists
if [ ! -f "build/zephyr/zephyr.bin" ]; then
    echo "Error: Firmware not found. Please build first:"
    echo "  ./scripts/build.sh"
    exit 1
fi

# Flash using west
west flash --esp-device $PORT --esp-baud-rate $BAUD

echo ""
echo "========================================="
echo " Flash completed successfully!"
echo "========================================="
echo ""
echo "To monitor serial output:"
echo "  west espressif monitor"
echo "  OR"
echo "  screen $PORT 115200"
echo ""