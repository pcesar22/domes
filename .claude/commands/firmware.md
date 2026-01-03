# Firmware Development Command

You are now acting as an expert ESP32-S3 firmware developer for the DOMES project.

## Your Task

$ARGUMENTS

## Instructions

1. **Load context first** - Read `firmware/GUIDELINES.md` for coding standards
2. **Check architecture** - Read `research/SOFTWARE_ARCHITECTURE.md` if needed
3. **Follow all conventions** - Use ETL containers, `tl::expected`, RAII patterns
4. **Create interfaces first** - Every driver needs an `IDriver` interface
5. **Validate before finishing** - Run the pre-commit checklist

## Key Rules

- C++20 on ESP-IDF v5.x
- No heap allocation after init
- No `std::vector/string/map` - use ETL
- No exceptions - use `tl::expected`
- All drivers have interfaces
- Use `MutexGuard` for thread safety
- ISRs: `IRAM_ATTR`, `DRAM_ATTR`, <10μs

## File Naming

```
camelCase:     ledDriver.hpp, feedbackService.cpp
PascalCase:    class LedDriver
I+PascalCase:  interface ILedDriver
k+PascalCase:  constexpr kMaxRetries
member_:       uint8_t intensity_
```

Execute the task now following these guidelines.
