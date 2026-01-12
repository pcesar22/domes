# Claude Code Documentation

This project was created and is maintained with assistance from Claude Code, Anthropic's official CLI for Claude.

## MANDATORY: Verification After Implementation

**IMPORTANT**: After ANY code implementation, you MUST verify your changes work END-TO-END. Unit tests alone are NOT sufficient. Do NOT consider a task complete until the feature actually works on hardware.

### Verification Levels (ALL required for new features)

#### Level 1: Unit Tests
```bash
cd firmware/test_app && mkdir -p build && cd build && cmake .. && make && ctest --output-on-failure
```

#### Level 2: Build ALL affected components
```bash
# Firmware
cd firmware/domes && . ~/esp/esp-idf/export.sh && idf.py build

# Host tools (if modified)
cd tools/domes-cli && cargo build
```

#### Level 3: End-to-End Verification (REQUIRED for new features)
```bash
# Flash firmware to device
/flash

# Test the actual feature works by:
# 1. Running the host tool against the device
# 2. Checking serial output/logs for expected behavior
# 3. Verifying the feature does what it's supposed to do
```

### Feature-Specific Verification

| Feature Type | Verification Method |
|--------------|---------------------|
| Protocol/transport | Flash firmware, run host tool, verify communication works |
| Config/runtime | Flash firmware, use CLI to change settings, verify settings applied |
| LED/display | Flash firmware, visually confirm behavior |
| Sensors/input | Flash firmware, trigger input, verify response in logs |

### Available Skills & Commands

| Command | Purpose | When to Use |
|---------|---------|-------------|
| `/flash` | Build, flash, monitor | **ALWAYS** after firmware changes |
| `/lint-fw` | Check coding standards | Before committing firmware code |
| `/size` | Analyze binary size | After adding new features |
| `/debug-esp32` | GDB debugging | When investigating runtime issues |

### Test Files

- **Firmware unit tests**: `firmware/test_app/main/test_*.cpp`
- **Protocol tests**: `test_frame_codec.cpp`, `test_ota_protocol.cpp`, `test_config_protocol.cpp`
- **Feature tests**: `test_feature_manager.cpp`

**DO NOT** mark a task as complete if:
- Build fails
- Unit tests fail
- Firmware doesn't flash successfully
- **You haven't verified the feature works on actual hardware**
- Host tool doesn't communicate with device (for host<->device features)

---

## About Claude Code

Claude Code is an interactive CLI tool that helps with software engineering tasks, including:
- Code generation and refactoring
- Bug fixing and debugging
- Documentation creation
- Test development
- Code review and optimization

## Working with Claude

When working on this project with Claude Code:

1. **Clear Communication**: Provide specific, detailed requirements for the best results
2. **Iterative Development**: Claude can help break down complex tasks into manageable steps
3. **Code Review**: Always review Claude's suggestions before committing changes
4. **Context Awareness**: Claude understands the project structure and can maintain consistency
5. **Always Verify**: Run tests after every implementation - no exceptions

## Project Sessions

Claude Code sessions are tracked to maintain context and ensure continuity across development tasks.

### Current Branch
This documentation was created on branch: `claude/create-documentation-files-gJwGB`

## Available Skills

The following skills are available in `.claude/skills/`:

### debug-esp32 (GDB Debugging)
**Trigger**: Use when user asks to "debug", "gdb", "breakpoint", "step through", "backtrace", "info stack", "inspect variable", or investigate crashes.

Provides ESP32-S3 GDB debugging via MCP tools. Key features:
- Start/stop debug sessions with OpenOCD + GDB
- Set breakpoints at functions or file:line
- Step through code (step into, step over, finish)
- Inspect variables, memory, registers
- View FreeRTOS tasks
- Full call stack (backtrace)

**IMPORTANT**: Always read `.claude/skills/debug-esp32/SKILL.md` before debugging to follow the correct workflow (async continue, reset halt, etc.)

### esp32-firmware
**Trigger**: Use when user asks to "build", "flash", "monitor", or work with ESP-IDF.

Provides ESP32 firmware build/flash/monitor commands.

### github-workflow
**Trigger**: Use for GitHub Actions and CI/CD tasks.

## Performance Tracing

The firmware includes a lightweight tracing framework for performance profiling. Traces are captured to a ring buffer and exported via USB serial in Chrome JSON format for Perfetto visualization.

### Quick Usage

```bash
# Dump traces
python tools/trace/trace_dump.py -p /dev/ttyACM0 -o trace.json -n tools/trace/trace_names.json

# Visualize at https://ui.perfetto.dev
```

### Adding Trace Points

```cpp
#include "trace/traceApi.hpp"

void myFunction() {
    // RAII scope trace - automatically records begin/end
    TRACE_SCOPE(TRACE_ID("MyModule.Function"), domes::trace::TraceCategory::kGame);

    // Point-in-time event
    TRACE_INSTANT(TRACE_ID("MyModule.Event"), domes::trace::TraceCategory::kGame);

    // Counter value
    TRACE_COUNTER(TRACE_ID("MyModule.Value"), someValue, domes::trace::TraceCategory::kGame);
}
```

### Key Files

| File | Purpose |
|------|---------|
| `firmware/domes/main/trace/traceApi.hpp` | User-facing macros |
| `firmware/domes/main/trace/traceRecorder.hpp` | Singleton recorder |
| `tools/trace/trace_dump.py` | Host dump/convert tool |
| `tools/trace/trace_names.json` | Span ID → name mappings |
| `research/architecture/07-debugging.md` | Full documentation |

## Platform Requirements

**READ FIRST: `.claude/PLATFORM.md`**

- Host machine for BLE testing: **Native Linux only** (Arch, Ubuntu, etc.)
- Do NOT use WSL2 for BLE - USB passthrough is broken
- Do NOT use Raspberry Pi - BCM Bluetooth has firmware bugs
- Recommended BLE adapter: Intel AX200/AX210 or Realtek RTL8761B

## Best Practices

- Keep commits atomic and well-described
- Review all AI-generated code for security and correctness
- Use Claude for exploration, implementation, and documentation
- Maintain clear documentation for human developers
- **For debugging**: Always use the debug-esp32 skill and follow its workflow
- **For BLE testing**: Use native Linux, never WSL2

## Resources

- [Claude Code GitHub](https://github.com/anthropics/claude-code)
- [Anthropic Documentation](https://docs.anthropic.com)

---

*Last updated: 2026-01-02*
