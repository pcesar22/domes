# DOMES Host Tools

This directory contains the supported host CLI plus small trace, visualization, and Linux device
helpers. Device communication belongs in `domes-cli`; do not create one-off protocol clients for
serial, TCP, BLE, or OTA workflows.

## Tool Index

| Path | Purpose |
| --- | --- |
| [`domes-cli/`](domes-cli/) | Supported device CLI for discovery, configuration, diagnostics, OTA, tracing, and multi-device operations |
| [`firmware/flash_and_verify.sh`](firmware/flash_and_verify.sh) | Build, flash, and verify one or more ESP32 devices from captured serial output |
| [`firmware/monitor_serial.py`](firmware/monitor_serial.py) | Monitor and label serial output from one or more attached devices |
| [`trace/trace_merge.py`](trace/trace_merge.py) | Merge multiple CLI trace exports into one Perfetto-compatible timeline |
| [`trace/trace_names.json`](trace/trace_names.json) | Map firmware trace identifiers to readable names |
| [`udev/99-domes-pods.rules`](udev/99-domes-pods.rules) | Stable Linux device aliases for attached pods |
| [`gen_results_svg.py`](gen_results_svg.py) | Generate the deterministic drill-results research graphic |
| [`gen_timeline_svg.py`](gen_timeline_svg.py) | Generate a timeline graphic from simulation trace JSON |

## Device CLI

Build and inspect the current command surface:

```bash
cd tools/domes-cli
cargo build
cargo test
cargo run -- --help
```

Use [`domes-cli/README.md`](domes-cli/README.md) for connection and command examples. The executable's
`--help` output owns exact command syntax. Protocol definitions remain in
[`../firmware/common/proto/`](../firmware/common/proto/) and the shared framing implementation; this
README intentionally does not duplicate message IDs or payload layouts.

## Firmware Hardware Helpers

The canonical flash and serial-monitor helpers live under `tools/firmware/`:

```bash
tools/firmware/flash_and_verify.sh firmware/domes /dev/ttyACM0 "DOMES"
python3 tools/firmware/monitor_serial.py /dev/ttyACM0,/dev/ttyACM1 30
```

Compatibility wrappers under `.codex/` and `.claude/` forward to these files. Update only the
canonical helpers when changing shared behavior.

## Merge Pod Traces

First export one trace per pod with `domes-cli trace dump`, then merge them from the repository root:

```bash
python3 tools/trace/trace_merge.py \
  --pod /tmp/pod1.json --pod-name pod1 \
  --pod /tmp/pod2.json --pod-name pod2 \
  --names tools/trace/trace_names.json \
  --align beacon \
  --output /tmp/domes-merged.json
```

Open the result in [Perfetto](https://ui.perfetto.dev). Run
`python3 tools/trace/trace_merge.py --help` for alignment options.

## Simulation Graphics

The SVG generators reproduce research artifacts; they are not device communication tools:

```bash
python3 tools/gen_results_svg.py /tmp/sim-results.svg
python3 tools/gen_timeline_svg.py /tmp/sim-trace.json /tmp/sim-timeline.svg
```

Generate the input trace with the host test application's `trace_generator`; see
[`../firmware/test_app/README.md`](../firmware/test_app/README.md).

## Platform Setup

BLE, serial permissions, stable udev aliases, and multi-device host requirements are maintained in
[`../.codex/PLATFORM.md`](../.codex/PLATFORM.md). Repository-wide verification requirements are in
[`../docs/TESTING.md`](../docs/TESTING.md).
