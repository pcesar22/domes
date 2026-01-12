# DOMES Unit Tests

Standalone unit test application for the DOMES firmware protocol layer using GoogleTest.

## Quick Start

```bash
cd firmware/test_app
mkdir -p build && cd build
cmake ..
make
./test_app
```

Or run with CTest for detailed output:
```bash
ctest --output-on-failure
```

## Test Suites

| Suite | Tests | Description |
|-------|-------|-------------|
| CRC32 | 11 | IEEE 802.3 CRC32 implementation |
| Frame Codec | 19 | Frame encoding/decoding, state machine |
| OTA Protocol | 21 | OTA message serialization |
| Version Parser | 17 | Firmware version parsing and comparison |

**Total: 68 tests**

## Architecture

Tests run on the host (Linux/macOS) without ESP-IDF dependencies. The `common/` library builds in dual-mode:
- ESP-IDF component mode (when `IDF_TARGET` is set)
- Standard CMake library mode (for host testing)

GoogleTest is fetched automatically via CMake FetchContent.

## Adding New Tests

1. Create `main/test_<module>.cpp` with test functions:
   ```cpp
   #include <gtest/gtest.h>

   TEST(MyModule, DoesSomething) {
       EXPECT_EQ(expected, actual);
   }
   ```

2. Add to `CMakeLists.txt`:
   ```cmake
   add_executable(test_app
       ...
       main/test_<module>.cpp
   )
   ```

Tests are auto-discovered by GoogleTest - no manual registration needed.

## CI Integration

Tests run automatically on every PR via GitHub Actions. See `.github/workflows/test.yml`.
