---
description: Analyze ESP32 firmware binary size by component
argument-hint: [project-path]
allowed-tools: Bash, Read, Glob
---

# Analyze Firmware Size

Analyze the binary size of the ESP32 firmware, breaking down by component to identify optimization opportunities.

## Arguments

- `$1` - Project path (optional, defaults to `firmware/domes/`)

## Instructions

1. **Determine project path:**
   - If `$1` is provided, use it
   - Otherwise, default to `firmware/domes/`
   - Verify it's a valid ESP-IDF project

2. **Source ESP-IDF environment:**
   ```bash
   . ~/esp/esp-idf/export.sh
   ```

3. **Run size analysis commands:**

   **Overall size:**
   ```bash
   cd $PROJECT_PATH && idf.py size
   ```

   **Component breakdown:**
   ```bash
   idf.py size-components
   ```

   **File-level details (if requested or size is concerning):**
   ```bash
   idf.py size-files
   ```

4. **Parse and analyze results:**

   Key metrics to extract:
   - Total binary size vs available flash
   - IRAM usage (limited to ~128KB on ESP32-S3)
   - DRAM usage (limited to ~320KB on ESP32-S3)
   - Flash usage breakdown

5. **Compare against partition table:**
   - Read `partitions.csv` if present
   - Check if binary fits in allocated partition
   - For OTA: each app partition is typically 1856KB

6. **Report findings:**

   Format output as:
   ```
   ## Binary Size Report

   ### Summary
   - Total: XXX KB / 1856 KB (XX% of OTA partition)
   - IRAM: XXX KB / 128 KB (XX%)
   - DRAM: XXX KB / 320 KB (XX%)

   ### Top 5 Components by Size
   1. component_name: XXX KB
   2. ...

   ### Recommendations
   - [Any size optimization suggestions]
   ```

7. **Optimization suggestions based on findings:**
   - If WiFi/BLE unused but large: suggest disabling in menuconfig
   - If logging is large: suggest reducing log level for production
   - If IRAM tight: suggest moving functions to flash with `IRAM_ATTR` review
   - If DRAM tight: suggest moving const data to flash with `DRAM_ATTR` review

## Reference: ESP32-S3 Memory Limits

| Memory | Size | Notes |
|--------|------|-------|
| IRAM | 128 KB | Instruction RAM, for ISRs and hot code |
| DRAM | 320 KB | Data RAM, for runtime variables |
| Flash | 16 MB | Total flash (shared: bootloader, NVS, OTA, SPIFFS, app) |
| PSRAM | 8 MB | External, slower, for large buffers |
