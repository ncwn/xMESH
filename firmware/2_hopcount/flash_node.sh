#!/bin/bash
# Flash script for Protocol 2 - Hop-Count Routing on macOS

# Check if node ID is provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <NODE_ID> <PORT>"
    echo "Example: $0 1 /dev/cu.usbserial-0001"
    echo ""
    echo "Node ID to Role mapping:"
    echo "  1, 2:  Sensor"
    echo "  3, 4:  Relay"
    echo "  5, 6:  Gateway"
    echo ""
    echo "Available ports:"
    ls /dev/cu.usbserial-* 2>/dev/null || echo "No USB serial devices found"
    exit 1
fi

NODE_ID=$1
PORT=$2

echo "======================================="
echo "Flashing Protocol 2 - Node $NODE_ID"
echo "Port: $PORT"
echo "======================================="

# Build with specific NODE_ID
export PLATFORMIO_BUILD_FLAGS="-D NODE_ID=$NODE_ID"

# Clean build cache to prevent NODE_ID conflicts
echo "Cleaning build cache..."
pio run -t clean

# Build and upload
pio run --target upload --upload-port $PORT

if [ $? -eq 0 ]; then
    echo "✅ Successfully flashed Node $NODE_ID"

    # Log to session
    echo "$(date): Flashed Protocol 2 to Node $NODE_ID on $PORT" >> ../../.context/session_log.md

    echo ""
    echo "To monitor output:"
    echo "pio device monitor --port $PORT --baud 115200"
else
    echo "❌ Failed to flash Node $NODE_ID"
    echo "$(date): FAILED to flash Protocol 2 to Node $NODE_ID" >> ../../.context/session_log.md
fi
