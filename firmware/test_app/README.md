# DOMES Unit Tests

Standalone unit test application for the DOMES firmware protocol layer.

## Quick Start

```bash
cd firmware/test_app
mkdir -p build && cd build
cmake -DIDF_TARGET= ..
make
./test_app
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

## Adding New Tests

1. Create `main/test_<module>.cpp` with test functions:
   ```cpp
   extern "C" void test_MyModule_does_something(void) {
       TEST_ASSERT_EQUAL(expected, actual);
   }
   ```

2. Add to `CMakeLists.txt`:
   ```cmake
   add_executable(test_app
       ...
       main/test_<module>.cpp
   )
   ```

3. Register in `main/test_main_standalone.cpp`:
   ```cpp
   extern "C" void test_MyModule_does_something(void);
   // ...
   RUN_TEST(test_MyModule_does_something);
   ```

## CI Integration

Tests run automatically on every PR via GitHub Actions. See `.github/workflows/test.yml`.
