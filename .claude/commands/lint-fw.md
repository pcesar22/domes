---
description: Check firmware code against DOMES coding standards
argument-hint: [file-or-directory]
allowed-tools: Read, Glob, Grep
---

# Lint Firmware Code

Check firmware code against the DOMES project coding standards defined in `firmware/GUIDELINES.md`.

## Arguments

- `$1` - File or directory to check (optional, defaults to `firmware/domes/main/`)

## Instructions

1. **Determine scope:**
   - If `$1` is provided, use it
   - Otherwise, check all files in `firmware/domes/main/`
   - Find all `.cpp`, `.hpp`, `.c`, `.h` files

2. **Read the guidelines:**
   - Read `firmware/GUIDELINES.md` for the authoritative rules

3. **Check each file for violations:**

### Critical Violations (Must Fix)

**Memory Allocation (after init):**
- Search for `new `, `malloc`, `calloc`, `realloc` outside of init functions
- Search for `std::vector`, `std::string`, `std::map`, `std::list` (should use ETL)
- Pattern: `\b(new|malloc|calloc|realloc)\b`
- Pattern: `std::(vector|string|map|list|set|unordered_map)`

**Forbidden Headers:**
- Search for `#include <iostream>`
- Search for `#include <fstream>`
- Pattern: `#include\s*<(iostream|fstream)>`

**Exception Usage:**
- Search for `throw`, `try`, `catch` keywords
- Pattern: `\b(throw|try|catch)\b`

**RTTI Usage:**
- Search for `typeid`, `dynamic_cast`
- Pattern: `\b(typeid|dynamic_cast)\b`

### ISR Safety Violations

**In functions marked IRAM_ATTR:**
- Check for `ESP_LOG` calls (forbidden in ISR)
- Check for heap allocation
- Check for FreeRTOS API calls without `FromISR` suffix
- Pattern in IRAM functions: `ESP_LOG[EWIDV]?\s*\(`

### Naming Convention Violations

**Files:**
- Should be camelCase: `ledDriver.hpp`, not `led_driver.hpp` or `LedDriver.hpp`

**Classes:**
- Should be PascalCase: `LedDriver`, not `led_driver` or `ledDriver`

**Interfaces:**
- Should start with `I`: `ILedDriver`, not `LedDriverInterface`

**Member Variables:**
- Should have trailing underscore: `ledCount_`, not `ledCount` or `m_ledCount`

**Constants:**
- Should start with `k`: `kMaxLedCount`, not `MAX_LED_COUNT` or `maxLedCount`

### Documentation Violations

**Public APIs without Doxygen:**
- Check for public methods without `@brief` documentation
- Check for missing `@param` and `@return` tags

### Thread Safety

**Missing const:**
- Getters that don't modify state should be `const`

**Unprotected shared state:**
- Look for member variables accessed without mutex in multi-threaded contexts

4. **Report findings:**

Format output as:
```
## Firmware Lint Report

### Critical Violations (X found)
- [file:line] Description of violation

### Warnings (X found)
- [file:line] Description of warning

### Style Issues (X found)
- [file:line] Description of style issue

### Summary
- Critical: X (must fix before commit)
- Warnings: X (should fix)
- Style: X (optional)
```

5. **Provide fix suggestions:**
   - For each violation, suggest the correct pattern
   - Reference the specific section in GUIDELINES.md

## Quick Reference: Allowed Patterns

```cpp
// Memory: Use ETL containers
#include <etl/vector.h>
etl::vector<int, 10> buffer;  // Fixed capacity, no heap

// Errors: Use expected
std::expected<int, esp_err_t> getValue();

// Constants
static constexpr size_t kBufferSize = 256;

// Member variables
class Foo {
    int count_;  // trailing underscore
};

// Logging (not in ISR)
ESP_LOGI(TAG, "Message");
```
