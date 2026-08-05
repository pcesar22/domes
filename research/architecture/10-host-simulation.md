# 10 - Host Simulation Architecture

> **Document status: Current as-built reference.** This chapter describes the standalone host test
> project under `firmware/test_app/`. The earlier serial-relay, standalone OTA sender, simulator CLI,
> and native-USB runtime topology proposals were removed because they were never implemented. Git
> history retains that proposal.

Last checked against the repository: 2026-08-04.

## Purpose And Boundary

The host project provides deterministic, fast coverage for firmware-compatible logic without an
ESP32 or radio. It is not a firmware emulator and it does not connect to physical pods.

Host simulation can verify:

- shared frame, CRC, OTA, and protobuf contracts;
- feature, mode, and per-pod game state transitions;
- deterministic multi-pod message routing, hits, misses, and timeouts;
- ESP-NOW packed-message size, sender, and round-token helpers;
- trace recording and Perfetto-compatible export; and
- release metadata, version parsing, and OTA session state.

It cannot establish ESP-IDF integration, flash layout, UART/BLE/WiFi behavior, radio timing,
FreeRTOS scheduling, watchdog behavior, or physical peripheral output. Those require the builds and
hardware gates in [`../../docs/TESTING.md`](../../docs/TESTING.md).

The target architecture for adding deterministic ESP32-S3/IDF FreeRTOS execution without
misrepresenting this host model is
[`13-deterministic-virtual-platform.md`](13-deterministic-virtual-platform.md). Its QEMU,
target-scheduler, virtual-radio, sanitizer, and predictive-qualification tiers are not implemented
unless `PROGRAM_STATUS.md` records direct evidence otherwise.

## Project Composition

```text
firmware/test_app/
  CMakeLists.txt       GoogleTest/CTest project and trace_generator target
  main/                Unit, contract, integration-style simulation tests
  sim/                 Pods, in-memory bus, drivers, orchestration, trace export
  stubs/               Narrow ESP-IDF and FreeRTOS host compatibility surface
```

The test executable links selected production sources instead of copying their behavior. Production
sources are included only when the host stubs can represent their dependencies faithfully.

| Host component | Responsibility |
| --- | --- |
| `SimClock` | Owns explicit virtual time while keeping production timer calls synchronized |
| `SimOrchestrator` | Owns simulated pods, the clock, and the event log |
| `PodInstance` | Composes per-pod feature, mode, game, and fake-driver state |
| `SimEspNowBus` | Applies deterministic delivery decisions and records delivery and flow events |
| `PodCommandHandler` | Applies simulated peer commands and returns events |
| `DrillOrchestrator` | Executes a deterministic sequence of target, delay, touch, and timeout steps |
| `HostTraceBuffer` and `PerfettoExporter` | Capture host trace events and export trace-event JSON |

The simulator's internal message variant is test infrastructure, not the on-air ESP-NOW wire
contract. The live packed contract remains
[`espNowProtocol.hpp`](../../firmware/domes/main/services/espNowProtocol.hpp), with focused host
contract coverage in `main/test_esp_now_protocol.cpp`.

## Deterministic Delivery Replay

`SimEspNowBus` accepts a delivery policy for each intended recipient. A policy can pass, delay,
drop, or duplicate a message without wall-clock sleeps. Delayed messages remain pending until
`SimClock` reaches their virtual deadline.

Every decision records the send time, source, addressed and resolved destinations, message variant,
type, canonical payload, sequence, action, and delay. Deadlines are anchored to send time, and drill
time advances through due deliveries before applying pod inputs. A fresh bus can consume the record
with `setReplayRecord()`. Replay checks the complete message identity and fails closed rather than
applying a decision to different input. Empty records are valid replays that reject unexpected
traffic, and replay completes only after every recorded decision is consumed and both delivery
queues are empty. Host tests cover the default lossless behavior, virtual delay boundary, drop,
duplicate, exact flow replay, complete variant identity, payload mismatch, empty replay, delayed
arm/touch/timeout ordering, and cross-round response correlation.

Simulated arm commands carry a unique round token that the pod returns unchanged in touch and
timeout events. Drill scoring accepts only an event from the targeted pod with the active round's
token, so delayed traffic from an earlier round cannot be credited to a later one.

This is deterministic fault-injection infrastructure, not a calibrated radio model. Delivery
policies and timing distributions must be derived from physical evidence and validated against
held-out device runs before the simulator can support a real-world prediction claim.

## Build And Test

From the repository root:

```bash
cmake -S firmware/test_app -B firmware/test_app/build
cmake --build firmware/test_app/build
ctest --test-dir firmware/test_app/build --output-on-failure
```

The first configure fetches the pinned GoogleTest revision and therefore needs network access.
CTest discovers individual GoogleTest cases at build time; inspect the live count rather than
copying a number into another document:

```bash
ctest --test-dir firmware/test_app/build -N
```

Use test filters for local iteration, then run the full suite before reporting a result:

```bash
ctest --test-dir firmware/test_app/build -R 'EspNow|MultiPod|SimDrill' --output-on-failure
firmware/test_app/build/test_app --gtest_filter='GameEngineTest.*'
```

## Simulation Trace

The `trace_generator` executable builds with the host project. It runs a deterministic five-pod,
15-round drill containing hits and misses, then exports Perfetto-compatible JSON:

```bash
firmware/test_app/build/trace_generator /tmp/domes-sim-trace.json
```

Open the result in [Perfetto](https://ui.perfetto.dev). This proves the host exporter and simulated
flow model; it does not prove on-device trace transport, buffer behavior, or synchronized physical
pod clocks.

## Physical-Device Workflows

Physical pods are operated with `domes-cli`, not a simulator relay. On the NFF DevKit:

- the CP2102N `/dev/serial/by-id/` path carries flashing, framed UART commands, and serial OTA;
- native ESP32-S3 USB Serial/JTAG is the separate console/JTAG interface;
- BLE config and OTA use the firmware GATT service; and
- ESP-NOW verification requires at least two physical pods.

Use [`../../hardware/nff-devboard/BRING_UP_CHECKLIST.md`](../../hardware/nff-devboard/BRING_UP_CHECKLIST.md)
for programming and device acceptance. The removed standalone sender and simulator CLI are not
repository tools and must not be reintroduced in current instructions.

## Extension Rules

1. Prefer tests against production source or a shared codec over a second protocol implementation.
2. Keep simulated time deterministic; do not add wall-clock sleeps to the host model.
3. Assert externally visible state and wire compatibility rather than private implementation detail.
4. Treat the internal simulator protocol as test-only and keep the live ESP-NOW packed contract
   independently covered.
5. Complete ESP-IDF and hardware verification whenever a change crosses the host boundary.
