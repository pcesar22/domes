# Trace Architecture

> **Document status: Current as-built reference.** The earlier trace-overhaul proposal was retired
> because it treated logs and framed traffic as one USB-CDC stream and proposed unimplemented log
> forwarding and clock-beacon alignment. Git history retains that design record. This document now
> describes the checked-in firmware, protobuf, CLI, and merge behavior.

Last checked against the repository: 2026-08-02.

## Runtime Boundary

The active NFF board has separate host-facing paths:

- CP2102N-backed UART0 carries framed config, trace control/dump, and serial OTA.
- Native ESP32-S3 USB Serial/JTAG carries ESP-IDF console logs and JTAG.
- BLE GATT carries framed config, trace control/dump, and BLE OTA.
- Optional WiFi builds expose config on TCP port 5000 and a dedicated live trace stream on port
  5001. Generic trace commands are not routed through the TCP config server.

There is no shared UART log/trace contention in the current composition. Do not redirect console
logs onto UART0 while framed traffic is active.

## Source Ownership

| Concern | Authority |
| --- | --- |
| Trace message IDs, categories, event types, and protobuf messages | [`../../firmware/common/proto/trace.proto`](../../firmware/common/proto/trace.proto) |
| Compact event layout | [`../../firmware/domes/main/trace/traceEvent.hpp`](../../firmware/domes/main/trace/traceEvent.hpp) |
| Recording and task-name registration | `firmware/domes/main/trace/traceRecorder.*` |
| Bounded initialization-time ring buffer and retained dump snapshot | `firmware/domes/main/trace/traceBuffer.*` |
| Start, stop, clear, status, and dump handling | `firmware/domes/main/trace/traceCommandHandler.*` |
| Optional live TCP streaming | `firmware/domes/main/trace/traceStreamServer.*` |
| Rust decoding and Perfetto export | `tools/domes-cli/src/commands/trace.rs` |
| Multi-pod grouping | [`../../tools/trace/trace_merge.py`](../../tools/trace/trace_merge.py) |
| Stable trace-name generation | [`../../tools/trace/generate_trace_names.py`](../../tools/trace/generate_trace_names.py) |

`trace.proto` owns assigned frame types `0x10-0x1B`. A trace frame uses the common outer envelope:

```text
[0xAA][0x55][LenLE16][Type][Payload][CRC32LE]
```

Control, metadata, dump, and streaming payloads are protobuf-encoded. The start, stop, dump, clear,
and status requests require an empty payload; firmware returns an error acknowledgment for any
nonempty request. High-frequency events remain a bounded fixed-binary exception: each `TraceEvent`
is exactly 16 bytes and is carried inside the protobuf `bytes` fields of `TraceDataChunk` and
`StreamBatch`.

## Event Model

Each compact event contains:

| Field | Width | Meaning |
| --- | --- | --- |
| timestamp | 32 bits | Local `esp_timer` microseconds, truncated to the wire width |
| task ID | 16 bits | FreeRTOS task number |
| event type | 8 bits | Synchronization, span, instant, counter, or complete type |
| flags | 8 bits | Category in the high nibble |
| arg1 / arg2 | 32 bits each | Type-specific ID, value, wait time, or duration |

Firmware call sites use `TRACE_SCOPE`, `TRACE_BEGIN`, `TRACE_END`, `TRACE_INSTANT`, and
`TRACE_COUNTER` from `traceApi.hpp`. Human-readable span names are FNV-derived IDs resolved by the
generated `tools/trace/trace_names.json` mapping. The generator detects collisions and rejects
stale output in CI.

Supported trace event types are explicit mutex lock/unlock/contention, semaphore take/give, and
application span/instant/counter/complete instrumentation. The current firmware does not wire
FreeRTOS scheduler, ISR, or queue trace macros into this recorder. Their former event IDs are
reserved rather than advertised as implemented. The `kKernel` category remains available for
explicitly emitted diagnostics and memory counters.
`CONFIG_FREERTOS_USE_TRACE_FACILITY` supports task-number and task-health introspection; it is not
evidence of scheduler event capture.

## Recording And Dumping

The recorder initializes during firmware startup but recording remains disabled until an explicit
start command or a live stream connection enables it. Its fixed-capacity FreeRTOS ring buffer
requests a bounded 32 KiB allocation during initialization; it is not statically backed. When full
or paused, new events are counted as dropped rather than blocking a producer.

A dump does the following:

1. serializes trace commands so only one handler mutates trace state at a time;
2. claims the one retained dump snapshot for the requesting transport;
3. pauses recording and captures the current events;
4. sends `TraceSessionInfo`, ordered `TraceDataChunk` messages, and `TraceDumpComplete`;
5. includes event counts, offsets, task names, dropped count, and a byte-sum checksum; and
6. releases the snapshot only after the final marker is delivered.

If transport delivery fails, the snapshot remains available to retry instead of silently draining
the data. `trace clear` is the explicit destructive operation.

The CLI validates frame CRC, session ordering, chunk offsets and lengths, final event count, and
checksum before writing Perfetto JSON.

## Service Workflow

Use a UART or BLE target for trace control and dump. The UART form is:

```bash
CLI=tools/domes-cli/target/debug/domes-cli
$CLI --port "$PORT" trace clear
$CLI --port "$PORT" trace start
$CLI --port "$PORT" trace status
$CLI --port "$PORT" system health
$CLI --port "$PORT" trace stop
$CLI --port "$PORT" trace dump \
  --output /tmp/domes-trace.json \
  --names tools/trace/trace_names.json
```

Open `/tmp/domes-trace.json` in [Perfetto](https://ui.perfetto.dev). A successful command sequence
proves transport and export behavior only when run against the built image; host tests alone do not
establish on-device buffer integrity.

## Live Streaming

`TraceStreamServer` is compiled and started only in a `CONFIG_DOMES_WIFI_AUTO_CONNECT` build that
has connected WiFi. It accepts one TCP client on port 5001, uses a bounded 256-event buffer, and
sends batches of at most 16 events with sequence and dropped-event counters.

```bash
domes-cli trace stream --wifi 192.168.1.100:5001
```

The stream CLI is intentionally single-target. It detects sequence gaps and invalid event batch
sizes. This endpoint is separate from TCP config port 5000; it does not make generic `--wifi`
trace-control commands valid.

## Multi-Pod Grouping

Capture and dump each pod independently, then group the Perfetto JSON files:

```bash
python3 tools/trace/trace_merge.py \
  --pod /tmp/pod1.json --pod-name "Pod 1" \
  --pod /tmp/pod2.json --pod-name "Pod 2" \
  --names tools/trace/trace_names.json \
  --align zero \
  --output /tmp/domes-two-pod.json
```

The only alignment modes are:

| Mode | Behavior |
| --- | --- |
| `zero` | Shift each pod's earliest local event to zero so captures can be viewed together |
| `raw` | Preserve each file's local timestamps unchanged |

Neither mode correlates clocks. `TraceSessionInfo` deliberately reserves the removed
`clock_offset_us` field, and the firmware does not emit a validated shared synchronization marker.
Do not use grouped traces as cross-pod latency evidence.

## Verification

Run the host trace and tooling tests:

```bash
cmake -S firmware/test_app -B firmware/test_app/build
cmake --build firmware/test_app/build
ctest --test-dir firmware/test_app/build -R Trace --output-on-failure
python3 -m unittest tools.trace.test_trace_merge tools.trace.test_generate_trace_names
tools/generate_protocols.sh --check all
```

Firmware-facing changes also require an ESP-IDF v5.4.4 build and the device workflow in
[`../../docs/TESTING.md`](../../docs/TESTING.md). Final readiness requires a bounded two-pod capture
on the exact candidate image and honest reporting that the timelines are local.

## Open Work

- Emit and validate a shared synchronization marker before offering cross-clock alignment.
- Exercise the final two-pod drill and retain a low-noise hardware trace as release evidence.
- Validate the WiFi-only stream endpoint on a provisioned auto-connect build.

The retired design's USB log forwarding, automatic cross-clock alignment, beacon-derived offset,
and version-adaptation proposals are not current features.
