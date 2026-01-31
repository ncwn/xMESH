#!/bin/bash

# xMESH Daily Test Runner
# Automatically executes unit tests and build verification

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")
REPORT_FILE="test-report-$(date +%Y%m%d).log"

echo "========================================"
echo " xMESH Daily Test Run - $TIMESTAMP"
echo "========================================"
echo "Starting test run at $TIMESTAMP" > "$REPORT_FILE"

FAILED=0

# 1. Run Unit Tests
echo -n "Running Unit Tests (native)... "
python3 -m platformio test -e native >> "$REPORT_FILE" 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}PASSED${NC}"
    echo "Unit Tests: PASSED" >> "$REPORT_FILE"
else
    echo -e "${RED}FAILED${NC}"
    echo "Unit Tests: FAILED" >> "$REPORT_FILE"
    FAILED=1
fi

# 2. Run Build Verification
echo -n "Running Build Verification (hardware)... "
python3 -m platformio run -e heltec_wifi_lora_32_V3 >> "$REPORT_FILE" 2>&1
if [ $? -eq 0 ]; then
    echo -e "${GREEN}PASSED${NC}"
    echo "Build Verification: PASSED" >> "$REPORT_FILE"
else
    echo -e "${RED}FAILED${NC}"
    echo "Build Verification: FAILED" >> "$REPORT_FILE"
    FAILED=1
fi

echo "========================================"
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}OVERALL RESULT: PASS${NC}"
    echo "OVERALL RESULT: PASS" >> "$REPORT_FILE"
else
    echo -e "${RED}OVERALL RESULT: FAIL${NC}"
    echo "OVERALL RESULT: FAIL" >> "$REPORT_FILE"
fi
echo "Report generated: firmware/production/$REPORT_FILE"
echo "========================================"

exit $FAILED
