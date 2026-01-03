#!/bin/bash
# DOMES Firmware Development Environment Setup Script

set -e

ESP_IDF_VERSION="v5.2.2"
ESP_IDF_PATH="${HOME}/esp/esp-idf"

echo "========================================="
echo "DOMES Firmware Development Setup"
echo "========================================="

# Check prerequisites
echo "Checking prerequisites..."

if ! command -v git &> /dev/null; then
    echo "ERROR: git is not installed"
    exit 1
fi

if ! command -v python3 &> /dev/null; then
    echo "ERROR: python3 is not installed"
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "ERROR: cmake is not installed"
    exit 1
fi

echo "Prerequisites OK"

# Install ESP-IDF
if [ -d "$ESP_IDF_PATH" ]; then
    echo "ESP-IDF already exists at $ESP_IDF_PATH"
    echo "To reinstall, remove the directory first: rm -rf $ESP_IDF_PATH"
else
    echo "Cloning ESP-IDF ${ESP_IDF_VERSION}..."
    mkdir -p "${HOME}/esp"
    git clone -b ${ESP_IDF_VERSION} --recursive https://github.com/espressif/esp-idf.git "$ESP_IDF_PATH"

    echo "Installing ESP-IDF tools for ESP32-S3..."
    cd "$ESP_IDF_PATH"
    ./install.sh esp32s3
fi

echo ""
echo "========================================="
echo "Setup Complete!"
echo "========================================="
echo ""
echo "To use ESP-IDF, run this in your terminal:"
echo ""
echo "    . ${ESP_IDF_PATH}/export.sh"
echo ""
echo "Then navigate to the firmware directory and build:"
echo ""
echo "    cd firmware"
echo "    idf.py set-target esp32s3"
echo "    idf.py build"
echo ""
