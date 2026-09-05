# DOMES Firmware Runbooks

These procedures complement the verification matrix in [`TESTING.md`](TESTING.md). Start with
[`PLATFORM.md`](PLATFORM.md) for toolchains, serial permissions, and the two NFF USB interfaces.
Run commands from the repository root with the selected test hardware available.

## Shared Setup

Build the CLI if a runbook uses `domes-cli` and `target/debug/domes-cli` does not exist:

```bash
(cd tools/domes-cli && cargo build --locked)
```

Use this CLI variable from the repo root:

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
```

Discover the CP2102N programming/runtime ports and assign stable serial-number paths:

```bash
find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' -print | sort
PORT1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '1p')"
PORT2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Silicon_Labs_CP2102N*' | sort | sed -n '2p')"
```

The numeric variable suffix is sorted discovery order, not a firmware pod ID. Query `system info`
before reassigning an existing board.

CP2102N ports (`/dev/ttyUSB*`) carry flashing, `domes-cli`, and serial OTA. Native ESP32-S3 USB
Serial/JTAG ports (`/dev/ttyACM*`) are separate console/JTAG interfaces. Set `CONSOLE1` and
`CONSOLE2` only when those second USB connections are attached.

## Flash Firmware

```bash
tools/firmware/flash_and_verify.sh firmware/domes "$PORT1"
```

Multiple ports:

```bash
tools/firmware/flash_and_verify.sh firmware/domes "$PORT1,$PORT2"
```

After flashing, run the feature list over the relevant transport:

```bash
tools/domes-cli/target/debug/domes-cli --port "$PORT1" feature list
```

The helper builds and flashes each CP2102N port and verifies the exact embedded version, system
health, and the complete on-device self-test over framed UART. Boot text is not expected on the
protocol UART.

If flashing fails, check USB cable, BOOT button, serial permissions, and whether ESP-IDF was
sourced.

## Monitor Serial

The monitor helper supports finite captures without an interactive terminal:

```bash
CONSOLE1="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' | sort | sed -n '1p')"
CONSOLE2="$(find -L /dev/serial/by-id -maxdepth 1 -type c \
  -name 'usb-Espressif_USB_JTAG_serial_debug_unit*' | sort | sed -n '2p')"
test -n "$CONSOLE1"
CONSOLES=("$CONSOLE1")
if [[ -n "$CONSOLE2" ]]; then
  CONSOLES+=("$CONSOLE2")
fi
CONSOLE_LIST="$(IFS=,; echo "${CONSOLES[*]}")"
python3 tools/firmware/monitor_serial.py "$CONSOLE1" 15
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 30
```

Use the monitor helper for native USB console captures. Framed UART0 traffic must remain under
the CLI transport owner; concurrent readers can consume protocol responses.

Filtered examples:

```bash
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 20 2>&1 | rg -i "espnow|esp-now|beacon|discover"
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 30 2>&1 | rg -i "game|arm|hit|miss|drill|round"
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 10 2>&1 | rg -i "touch|pad"
python3 tools/firmware/monitor_serial.py "$CONSOLE_LIST" 10 2>&1 | rg -i "heap|mem|diag"
```

## Size Analysis

Set `IDF_EXPORT_SCRIPT` to the installed ESP-IDF v5.4.4 `export.sh`. Use a fresh build when
checking the application partition limit after a feature change.

```bash
SIZE_ROOT="$(mktemp -d)"
(cd firmware/domes && \
  . "$IDF_EXPORT_SCRIPT" && \
  idf.py -B "$SIZE_ROOT/build" -D "IDF_TARGET=esp32s3" \
    -D "SDKCONFIG=$SIZE_ROOT/sdkconfig" build && \
  idf.py -B "$SIZE_ROOT/build" -D "SDKCONFIG=$SIZE_ROOT/sdkconfig" size && \
  idf.py -B "$SIZE_ROOT/build" -D "SDKCONFIG=$SIZE_ROOT/sdkconfig" size-components && \
  idf.py -B "$SIZE_ROOT/build" -D "SDKCONFIG=$SIZE_ROOT/sdkconfig" size-files)
```

Report total binary size, IRAM, DRAM, top components by size, and whether the binary fits the app
partition in `partitions.csv`.

## BLE Transport Test

Prerequisites: native Linux, firmware flashed and booted for at least 8 seconds, Bluetooth powered
on with `bluetoothctl power on`. Set `BLE_DEVICE` to the selected scan result.

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
$CLI --scan-ble
$CLI --ble "$BLE_DEVICE" feature list
$CLI --ble "$BLE_DEVICE" system info
$CLI --ble "$BLE_DEVICE" system memory
$CLI --ble "$BLE_DEVICE" led solid --color ff0000
sleep 2
$CLI --ble "$BLE_DEVICE" led off
$CLI --ble "$BLE_DEVICE" feature disable haptic
$CLI --ble "$BLE_DEVICE" feature list
$CLI --ble "$BLE_DEVICE" feature enable haptic
```

Also test connect-by-MAC if scan output provides the address.

Report devices found, feature list, system info, fragmented memory-response completion, LED command
acceptance, haptic feature-state round trip, and MAC connect status. Record visual confirmation for
LEDs. The toggle verifies BLE config behavior; it does not trigger or prove physical haptic output.

## ESP-NOW Integration Test

Requires two selected pods and GNU `timeout`. Run the complete block as one Bash command.
The subshell stops on failed assertions. On success, failure or interruption, bounded cleanup
requests radio disable, simulation off and trace stop on both selected ports; it does not clear
stored data or modify other features. Cleanup failures remain visible and produce a nonzero result;
verify the final device state before another campaign. Evidence files are retained.

```bash
(
set -euo pipefail
: "${PORT1:?Select the first runtime port}"
: "${PORT2:?Select the second runtime port}"
[[ "$PORT1" != "$PORT2" ]] || { echo "Select two distinct ports" >&2; exit 1; }
command -v timeout >/dev/null || { echo "GNU timeout is required" >&2; exit 1; }
CLI="tools/domes-cli/target/debug/domes-cli"
ROUNDS=${ROUNDS:-100}
SESSIONS=${SESSIONS:-3}
[[ "$ROUNDS" =~ ^[1-9][0-9]*$ && "$SESSIONS" =~ ^[1-9][0-9]*$ ]] || {
  echo "ROUNDS and SESSIONS must be positive integers" >&2
  exit 1
}
EVIDENCE_DIR="$(mktemp -d)"

cleanup() {
  local result=$? failed=0 port action output
  local -a arguments
  trap - EXIT INT TERM
  for port in "$PORT1" "$PORT2"; do
    for action in radio simulation trace; do
      case "$action" in
        radio) arguments=(feature disable esp-now) ;;
        simulation) arguments=(espnow sim-mode off) ;;
        trace) arguments=(trace stop) ;;
      esac
      if output=$(timeout --kill-after=1s 8s "$CLI" --port "$port" "${arguments[@]}" 2>&1); then
        :
      else
        printf 'Cleanup failed for %s (%s):\n%s\n' "$port" "$action" "$output" >&2
        failed=1
      fi
    done
  done
  if (( result == 0 && failed != 0 )); then result=1; fi
  printf 'Retained evidence: %s\n' "$EVIDENCE_DIR"
  exit "$result"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

read_status() {
  local port=$1 output
  output=$("$CLI" --port "$port" espnow status) || {
    printf 'Status request failed for %s:\n%s\n' "$port" "$output" >&2
    return 1
  }
  printf '%s\n' "$output"
}

wait_for_disabled() {
  local phase=$1 status1 status2
  for attempt in {1..20}; do
    status1=$(read_status "$PORT1") || return 1
    status2=$(read_status "$PORT2") || return 1
    if grep -Eq 'State:[[:space:]]+disabled[[:space:]]*$' <<< "$status1" &&
       grep -Eq 'State:[[:space:]]+disabled[[:space:]]*$' <<< "$status2"; then
      return 0
    fi
    sleep 1
  done
  printf 'Timed out waiting for %s:\n%s\n%s\n' "$phase" "$status1" "$status2" >&2
  return 1
}

wait_for_peers() {
  local phase=$1 status1 status2 state1 state2
  for attempt in {1..30}; do
    status1=$(read_status "$PORT1") || return 1
    status2=$(read_status "$PORT2") || return 1
    state1=$(awk '/State:/ {print $2; exit}' <<< "$status1")
    state2=$(awk '/State:/ {print $2; exit}' <<< "$status2")
    if [[ "$state1:$state2" == master:slave || "$state1:$state2" == slave:master ]] &&
       grep -Eq 'Peers:[[:space:]]+1[[:space:]]*$' <<< "$status1" &&
       grep -Eq 'Peers:[[:space:]]+1[[:space:]]*$' <<< "$status2"; then
      if [[ "$state1" == master ]]; then
        MASTER_PORT=$PORT1
        SLAVE_PORT=$PORT2
      else
        MASTER_PORT=$PORT2
        SLAVE_PORT=$PORT1
      fi
      printf '%s\n%s\n' "$status1" "$status2"
      return 0
    fi
    sleep 1
  done
  printf 'Timed out waiting for %s:\n%s\n%s\n' "$phase" "$status1" "$status2" >&2
  return 1
}

run_benchmark() {
  local port=$1 output
  output=$("$CLI" --port "$port" espnow bench --rounds "$ROUNDS") || {
    printf 'Benchmark command failed for %s:\n%s\n' "$port" "$output" >&2
    return 1
  }
  printf '%s\n' "$output"
  grep -Eq "Rounds:[[:space:]]+$ROUNDS/$ROUNDS completed \\(0 failed\\)[[:space:]]*$" <<< "$output" || {
    echo "Benchmark did not complete every round without failure" >&2
    return 1
  }
  grep -q 'Mean RTT:' <<< "$output" || { echo "Benchmark omitted RTT results" >&2; return 1; }
}

"$CLI" --port "$PORT1" feature disable esp-now
"$CLI" --port "$PORT2" feature disable esp-now
wait_for_disabled initial || exit 1
"$CLI" --port "$PORT1" espnow sim-mode off
"$CLI" --port "$PORT2" espnow sim-mode off

for session in $(seq 1 "$SESSIONS"); do
  "$CLI" --port "$PORT1" feature enable esp-now
  "$CLI" --port "$PORT2" feature enable esp-now
  wait_for_peers "benchmark session $session" || exit 1
  run_benchmark "$SLAVE_PORT" || exit 1
  run_benchmark "$MASTER_PORT" || exit 1
  "$CLI" --port "$PORT1" feature disable esp-now
  "$CLI" --port "$PORT2" feature disable esp-now
  wait_for_disabled "benchmark session $session" || exit 1
done

# The trace-backed simulated drill is a separate fresh lifecycle.
for port in "$PORT1" "$PORT2"; do
  "$CLI" --port "$port" trace stop
  "$CLI" --port "$port" trace clear
  "$CLI" --port "$port" trace start
  "$CLI" --port "$port" espnow sim-mode on --delay-ms 100 --pad 0
done
"$CLI" --port "$PORT1" feature enable esp-now
"$CLI" --port "$PORT2" feature enable esp-now
wait_for_peers "simulated drill" || exit 1
sleep 35

for port in "$PORT1" "$PORT2"; do
  status=$(read_status "$port") || exit 1
  printf '%s\n' "$status"
  grep -Eq 'State:[[:space:]]+disabled[[:space:]]*$' <<< "$status"
  grep -Eq 'Peers:[[:space:]]+1[[:space:]]*$' <<< "$status"
  grep -Eq 'TX fails:[[:space:]]+0[[:space:]]*$' <<< "$status"
done
"$CLI" --port "$PORT1" trace stop
"$CLI" --port "$PORT2" trace stop
"$CLI" --port "$PORT1" trace dump --output "$EVIDENCE_DIR/pod1.json" \
  --names tools/trace/trace_names.json
"$CLI" --port "$PORT2" trace dump --output "$EVIDENCE_DIR/pod2.json" \
  --names tools/trace/trace_names.json
python3 tools/trace/trace_merge.py \
  --pod "$EVIDENCE_DIR/pod1.json" --pod-name pod1 \
  --pod "$EVIDENCE_DIR/pod2.json" --pod-name pod2 \
  --names tools/trace/trace_names.json --align zero \
  --output "$EVIDENCE_DIR/merged.json"
)
```

`stopping` is a transitional state: the previous discovery or game loop still owns lifecycle state, so do not
re-enable until both pods report exact `disabled`. A valid result requires complementary roles, one
peer on each pod, complete zero-loss benchmark cardinality, and trace events for the drill. The merge
groups local timelines by capture start; it does not correlate clocks. Native USB console logs are
optional supporting evidence, not a replacement for these assertions.

Do not use WiFi feature commands as ESP-NOW setup. The default build omits the WiFi client feature,
while a `CONFIG_DOMES_WIFI_AUTO_CONNECT` build treats it as independent stored-credential client
state and preserves it across mode changes.

## IMU Test

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
$CLI --port "$PORT1" imu triage --enable
python3 tools/firmware/monitor_serial.py "$CONSOLE1" 5 2>&1 | \
  rg -i "imu|lis2dw12|accel|tap|mag"
```

Expected:

- LIS2DW12 data streaming at about 5 Hz in logs.
- Stationary magnitude around 0.95 g to 1.05 g.
- Flat board Z axis dominates near 1 g.

For tap detection, apply a physical tap while capturing the output:

```bash
python3 tools/firmware/monitor_serial.py "$CONSOLE1" 10 2>&1 | rg -i "tap|triage|mode"
$CLI --port "$PORT1" imu triage --disable
```

Hardware details: LIS2DW12 at I2C address 0x19, SDA GPIO8, SCL GPIO9, INT1 GPIO5.

## LED Test

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT="$PORT1"

for color in ff0000 00ff00 0000ff ffffff; do
  $CLI --port "$PORT" led solid --color "$color"
  sleep 1
done

$CLI --port "$PORT" led breathing --color ff00ff
sleep 3
$CLI --port "$PORT" led cycle
sleep 4
$CLI --port "$PORT" led off
$CLI --port "$PORT" led get
```

Record whether all 16 LEDs show the correct colors, with no dead or dim LEDs.

## Touch Test

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT="$PORT1"

python3 tools/firmware/monitor_serial.py "$CONSOLE1" 6 2>&1 | rg -i "TouchService|touch"

for pad in 0 1 2 3; do
  $CLI --port "$PORT" touch simulate --pad "$pad"
  sleep 0.5
done
```

Expected pad mapping:

| Pad | GPIO | Touch channel |
| --- | --- | --- |
| 0 | 1 | 1 |
| 1 | 2 | 2 |
| 2 | 4 | 4 |
| 3 | 6 | 6 |

Warn if baseline values are below 10000 or above 40000. Normal NFF board baselines are often
around 20000 to 28000.

## Baseline Hardware Smoke Test

This is a fast programming and command-path smoke test, not a full readiness result. Run it on each
attached board before the feature-specific workflows listed under Full Readiness Evaluation.

For each port:

```bash
CLI="tools/domes-cli/target/debug/domes-cli"
PORT="$PORT1"

$CLI --port "$PORT" system self-test
$CLI --port "$PORT" system info
$CLI --port "$PORT" feature list
$CLI --port "$PORT" led solid --color ff0000
sleep 1
$CLI --port "$PORT" led solid --color 00ff00
sleep 1
$CLI --port "$PORT" led solid --color 0000ff
sleep 1
$CLI --port "$PORT" led solid --color ffffff
sleep 1
$CLI --port "$PORT" led off
for pad in 0 1 2 3; do $CLI --port "$PORT" touch simulate --pad "$pad"; done
$CLI --scan-ble
```

For two ports, also run the ESP-NOW integration test above.

Report a pass/fail table for self-test, system info, feature list, LED command acceptance, touch
simulation, and BLE scan. For two ports, add ESP-NOW discovery and drill results from the dedicated
integration test.

Command acceptance and self-test initialization do not prove LED color, physical touch sensing,
haptic output, or audio output. Record visual/tactile/audible confirmation separately.

## Full Readiness Evaluation

Use [`hardware/nff-devboard/BRING_UP_CHECKLIST.md`](../hardware/nff-devboard/BRING_UP_CHECKLIST.md)
as the per-board evidence record. The checks below select the runbooks; they do not replace the
checklist's physical observations, programming/OTA acceptance sequence, or explicit unverified rows.

After the baseline passes on each board, run every applicable dedicated workflow in this reference:

- BLE Transport Test, including a real config response over BLE rather than scan-only evidence.
- IMU Test with live stationary readings and a physical tap. Confirm the tap's LED, haptic, and
  audio feedback separately.
- LED Test and Touch Test with physical visual/input confirmation.
- ESP-NOW Integration Test on two boards, including peer packet evidence and drill execution.
- Serial and BLE OTA from [`TESTING.md`](TESTING.md#hardware-verification), followed by reconnect,
  version, health, self-test, a second reboot, and repeated confirmation. Exercise
  invalid/interrupted recovery and forced failed-self-test rollback separately. The declared
  version must be the parser-valid, at-most-31-byte value embedded in the exact image. Raw TCP OTA
  is not a supported path.
- ESP-IDF panic-dump retrieval from the
  [debugging guide](DEBUGGING.md) when a controlled crash test is scheduled; a CLI
  clean-restart snapshot is not equivalent.
- Final `system health`, `system memory`, and `feature list` captures on each board after the test
  sequence.

Mark a row unverified rather than passed when the workflow lacks its required physical observation,
transport, matching firmware image, or second device.
