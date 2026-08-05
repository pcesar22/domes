# ESP32-S3 QEMU Feasibility Probe

This standalone ESP-IDF application is the bounded target-execution probe for `FS-WP-002B`. It
answers whether the ESP-IDF-pinned ESP32-S3 QEMU can execute the target FreeRTOS port on both CPUs,
perform real blocking handoffs, advance the 1 kHz target tick, and route one GPTimer interrupt from
CPU0 to a task blocked on CPU1 reproducibly.

It does not link `firmware/domes`, emulate DOMES peripherals or transports, provide scheduler-trace
coverage, model simultaneous CPU execution, establish cycle accuracy, or support a predictive claim.

## Host Setup

Use native x86_64 Linux, ESP-IDF v5.4.4, and the QEMU package selected by that IDF release.

```bash
# Arch Linux
sudo pacman -S --needed libslirp

. ~/esp/esp-idf/export.sh
python "$IDF_PATH/tools/idf_tools.py" install qemu-xtensa
. ~/esp/esp-idf/export.sh
qemu-system-xtensa --version
```

Other Linux distributions must provide `libslirp.so.0` through their system package manager. The
runner rejects a different IDF commit, compiler package, QEMU version, or QEMU executable digest.

## Run

```bash
# Development smoke run. Committed probe and runner sources are required by default.
python3 tools/simulation/qemu_feasibility.py --runs 1

# Package acceptance.
python3 tools/simulation/qemu_feasibility.py \
  --runs 100 \
  --build-dir /tmp/domes-qemu-feasibility/build \
  --artifact-dir /tmp/domes-qemu-feasibility/evidence
```

Each invocation uses a fresh build directory and isolated `SDKCONFIG`, measures cold and cached
builds, generates new flash and eFuse inputs for every target process, and starts QEMU with fixed
single-thread TCG, instruction-count time, VM clock, RTC base, seed, network, and snapshot settings.
An invocation reports `SUCCEEDED` when its requested work passes, but only a clean, committed,
exactly 100-run execution reports `acceptance.status=PASS`; build-only, dirty, and shorter runs report
`acceptance.status=NOT_ELIGIBLE` even when they succeed.
The acceptance run also requires:

- one exact structural signature across every run;
- one exact normalized observation signature, including relative task/tick/timer values;
- HMP visibility of CPU0 and CPU1;
- a GDB breakpoint at `domesQemuProbeComplete` with the complete `gProbeState`;
- no panic, reset, timeout, duplicate marker, early exit, dropped interrupt, or media mutation; and
- runner SIGTERM only after the complete result marker is captured; and
- matching source, toolchain, SDKCONFIG, ELF, boot-image, inspection-media, and campaign-media hashes
  when revalidated after the last run.

`--allow-dirty` exists only for developing the probe or runner. Evidence produced with it cannot
close the package. The output directory contains the report, raw logs, ELF, SDKCONFIG, reference
flash/eFuse images, monitor and GDB transcripts, and `artifact-manifest.sha256`.

## Probe Contract

`DOMES_QEMU_OBSERVATION` retains absolute and relative timer observations. `DOMES_QEMU_RESULT`
contains the structural pass contract. Any field addition or semantic change requires a schema
increment and matching strict parser/test update. The target remains alive after
`probe_state=complete` so GDB can inspect the terminal state; the Linux runner owns process
termination and records the action separately.
