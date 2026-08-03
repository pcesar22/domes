# DOMES Host Tools

This directory contains the supported host CLI and repository-owned protocol generation, firmware
verification, CI contract, agent-evaluation, trace, visualization, and Linux device tools. Device
communication belongs in `domes-cli`; do not create one-off protocol clients for serial, TCP, BLE,
or OTA workflows.

## Tool Index

| Path | Purpose |
| --- | --- |
| [`agent_eval/`](agent_eval/) | Reproducible, contained coding-agent repository-understanding evaluations; never a substitute for human or hardware review |
| [`domes-cli/`](domes-cli/) | Supported device CLI for discovery, configuration, diagnostics, OTA, tracing, and multi-device operations |
| [`generate_protocols.sh`](generate_protocols.sh) | Generate or drift-check nanopb and Dart bindings from the authoritative protobuf schemas |
| [`ci/test_release_contract.py`](ci/test_release_contract.py) | Assert release, CI, programming, OTA, and hardware-workflow contracts |
| [`firmware/flash_and_verify.sh`](firmware/flash_and_verify.sh) | Build, flash, and verify framed UART operation on one or more ESP32 devices |
| [`firmware/verify_restart_snapshot.sh`](firmware/verify_restart_snapshot.sh) | Validate a clean-restart snapshot against its boot count and version-matched pre-restart ELF |
| [`firmware/monitor_serial.py`](firmware/monitor_serial.py) | Monitor and label serial output from one or more attached devices |
| [`docs/check_markdown_links.py`](docs/check_markdown_links.py) | Check tracked Markdown files for broken repository-relative links |
| [`trace/trace_merge.py`](trace/trace_merge.py) | Merge multiple CLI trace exports into one Perfetto-compatible timeline |
| [`trace/generate_trace_names.py`](trace/generate_trace_names.py) | Generate and drift-check trace IDs from production `TRACE_ID` literals |
| [`trace/trace_names.json`](trace/trace_names.json) | Map firmware trace identifiers to readable names |
| [`udev/99-domes-pods.rules`](udev/99-domes-pods.rules) | Linux access policy for CP2102N and native USB devices; identity remains under `/dev/serial/by-id/` |
| [`gen_results_svg.py`](gen_results_svg.py) | Generate the deterministic drill-results research graphic |
| [`gen_timeline_svg.py`](gen_timeline_svg.py) | Generate a timeline graphic from simulation trace JSON |

## Device CLI

Build and inspect the current command surface:

```bash
(cd tools/domes-cli && cargo build --locked)
(cd tools/domes-cli && cargo test --locked --all-targets --all-features)
(cd tools/domes-cli && cargo run -- --help)
```

Use [`domes-cli/README.md`](domes-cli/README.md) for connection and command examples. The executable's
`--help` output owns exact command syntax. Protocol definitions remain in
[`../firmware/common/proto/`](../firmware/common/proto/) and the shared framing implementation; this
README intentionally does not duplicate message IDs or payload layouts.

## Firmware Hardware Helpers

The canonical flash and serial-monitor helpers live under `tools/firmware/`:

```bash
PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
tools/firmware/flash_and_verify.sh firmware/domes "$PORT1"

# Optional: two separately connected native USB console interfaces.
mapfile -t CONSOLES < <(
  find -L /dev/serial/by-id -maxdepth 1 -type c \
    -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' | sort
)
test "${#CONSOLES[@]}" -ge 2
python3 tools/firmware/monitor_serial.py \
  "${CONSOLES[0]},${CONSOLES[1]}" 30
```

The flash helper and `domes-cli` use NFF CP2102N ports (`/dev/ttyUSB*`, preferably
`/dev/serial/by-id/`). The monitor reads separately connected native USB console ports
(`/dev/ttyACM*`). Over framed UART, the helper verifies the exact built version through `system
info` and requires `system health` plus the complete `system self-test` to pass; it does not search
console text on that protocol port.

Compatibility wrappers under `.codex/` and `.claude/` forward to these files. Update only the
canonical helpers when changing shared behavior.

## Merge Pod Traces

First export one trace per pod with `domes-cli trace dump`, then merge them from the repository root:

```bash
python3 tools/trace/trace_merge.py \
  --pod /tmp/pod1.json --pod-name pod1 \
  --pod /tmp/pod2.json --pod-name pod2 \
  --names tools/trace/trace_names.json \
  --align zero \
  --output /tmp/domes-merged.json
```

Zero alignment groups each local timeline by its capture start. It is not cross-pod clock
correlation. Use `--align raw` only to retain the original local timestamps; neither mode
synchronizes pod clocks. Open the result in [Perfetto](https://ui.perfetto.dev). Run
`python3 tools/trace/trace_merge.py --help` for the exact interface.

Refresh the trace-name registry after adding, renaming, or removing a production `TRACE_ID`:

```bash
python3 tools/trace/generate_trace_names.py
python3 tools/trace/generate_trace_names.py --check
```

## Simulation Graphics

The SVG generators reproduce research artifacts; they are not device communication tools:

```bash
python3 tools/gen_results_svg.py /tmp/sim-results.svg
python3 tools/gen_timeline_svg.py /tmp/sim-trace.json /tmp/sim-timeline.svg
```

Generate the input trace with the host test application's `trace_generator`; see
[`../firmware/test_app/README.md`](../firmware/test_app/README.md).

## Platform Setup

BLE, serial permissions, stable `/dev/serial/by-id/` identities, and multi-device host requirements
are maintained in [`.codex/PLATFORM.md`](../.codex/PLATFORM.md). Repository-wide verification
requirements are in [`docs/TESTING.md`](../docs/TESTING.md).
