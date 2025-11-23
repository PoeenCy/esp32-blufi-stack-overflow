#!/bin/bash
# Script build và flash BLUFI cho ESP32
# Sử dụng: ./build_and_flash.sh [esp32|esp32s2|esp32s3|esp32c3|esp32c6] [PORT]

TARGET=${1:-esp32s3}
PORT=${2:-/dev/ttyUSB0}

echo "========================================="
echo "  ESP32 BLUFI Build & Flash Script"
echo "  Target: $TARGET"
echo "  Port: $PORT"
echo "========================================="

# Kiểm tra ESP-IDF đã được source chưa
if [ -z "$IDF_PATH" ]; then
    echo "ERROR: ESP-IDF chưa được khởi tạo!"
    echo "Vui lòng chạy: . \$IDF_PATH/export.sh"
    exit 1
fi

echo ""
echo "[1/5] Thiết lập target: $TARGET"
idf.py set-target $TARGET
if [ $? -ne 0 ]; then
    echo "ERROR: Không thể thiết lập target!"
    exit 1
fi

echo ""
echo "[2/5] Build project..."
idf.py build
if [ $? -ne 0 ]; then
    echo "ERROR: Build thất bại!"
    exit 1
fi

echo ""
echo "[3/5] Flash firmware vào $PORT..."
idf.py -p $PORT flash
if [ $? -ne 0 ]; then
    echo "ERROR: Flash thất bại!"
    exit 1
fi

echo ""
echo "[4/5] Khởi động serial monitor..."
echo "Nhấn Ctrl+] để thoát monitor"
echo "========================================="
idf.py -p $PORT monitor

