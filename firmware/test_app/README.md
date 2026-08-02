# DOMES Host Firmware Tests

This standalone CMake project exercises firmware-compatible logic on a development host with
GoogleTest and CTest. It builds selected production sources against host stubs and includes a
deterministic multi-pod simulator.

Host tests do not validate ESP-IDF integration, radio behavior, timing on a device, or physical
peripherals. Follow [`../../docs/TESTING.md`](../../docs/TESTING.md) for the checks required by each
change type.

## Build And Run

From the repository root:

```bash
cmake -S firmware/test_app -B firmware/test_app/build
cmake --build firmware/test_app/build
ctest --test-dir firmware/test_app/build --output-on-failure
```

The first configure fetches GoogleTest and therefore needs network access. Use CTest to inspect the
live discovered set instead of copying a count into documentation:

```bash
ctest --test-dir firmware/test_app/build -N
```

Run a focused test through CTest or GoogleTest:

```bash
ctest --test-dir firmware/test_app/build -R 'Frame|Ota' --output-on-failure
firmware/test_app/build/test_app --gtest_filter='GameEngineTest.*'
```

## Coverage Areas

| Area | Representative sources |
| --- | --- |
| Shared protocol support | CRC32, frame codec, OTA codec, version parsing, protobuf config messages and device notifications |
| OTA and release state | Session coordination, transfer state machine, embedded version parsing, and release metadata |
| Runtime state | `FeatureManager` and `ModeManager` |
| Game behavior | Per-pod `GameEngine` state transitions and feedback behavior |
| ESP-NOW contract | Exact packed-message sizes, discovery classification, authenticated sender matching, and per-round token matching |
| Multi-pod behavior | In-memory ESP-NOW bus, pod command handlers, orchestration, hits, misses, and timeouts |
| Observability | Trace buffer/snapshot behavior, stream writer integrity, host trace recorder, flow events, and Perfetto-compatible export |

Test cases live under [`main/`](main/). Simulation fakes and orchestration support live under
[`sim/`](sim/). The executable source list is maintained in [`CMakeLists.txt`](CMakeLists.txt).

## Generate A Simulation Trace

The build also produces `trace_generator`, which runs a deterministic multi-pod drill and writes a
Perfetto-compatible trace:

```bash
firmware/test_app/build/trace_generator /tmp/domes-sim-trace.json
```

Open the JSON in [Perfetto](https://ui.perfetto.dev), or pass it to
[`../../tools/gen_timeline_svg.py`](../../tools/gen_timeline_svg.py).

## Adding Tests

1. Add a `test_<area>.cpp` file under `main/`.
2. Add production sources only when the host stubs can represent their dependencies faithfully.
3. Register the file in `CMakeLists.txt`.
4. Prefer behavior and wire-compatibility assertions over implementation-detail assertions.
5. Run the full CTest suite, then complete any stronger build or hardware checks from the repository
   verification matrix.
