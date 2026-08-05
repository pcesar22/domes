# FS-WP-002B ESP32-S3 QEMU Feasibility Evidence

**Disposition:** `Viable`

The pinned Espressif ESP32-S3 QEMU executes a bounded, standalone ESP-IDF v5.4.4 dual-core
FreeRTOS probe reproducibly without a DOMES patch to the Espressif QEMU source. All FS-WP-002B
criteria passed against implementation commit `b12dbddeac64397b0b15e9970251694f40632cf9`.

This result makes FS-WP-002D eligible for separate selection. It does not select or activate D, and
it does not establish production-firmware execution, scheduler-trace coverage, ESP-NOW or RF
fidelity, simultaneous CPU execution, cycle accuracy, required CI admission, or predictive trust.

## Retained Package

- Machine-readable report: [`report.json`](b12dbddeac64397b0b15e9970251694f40632cf9/report.json)
- Artifact hashes: [`artifact-manifest.sha256`](b12dbddeac64397b0b15e9970251694f40632cf9/artifact-manifest.sha256)
- HMP inspection: [`monitor.txt`](b12dbddeac64397b0b15e9970251694f40632cf9/inspection/attempt-1/monitor.txt)
- GDB inspection: [`gdb.txt`](b12dbddeac64397b0b15e9970251694f40632cf9/inspection/attempt-1/gdb.txt)
- Retained ELF and SDKCONFIG: [`artifacts/`](b12dbddeac64397b0b15e9970251694f40632cf9/artifacts/)
- Raw target logs: [`run-001/qemu.log`](b12dbddeac64397b0b15e9970251694f40632cf9/run-001/qemu.log)
  through `run-100/qemu.log`
- Runner unit tests: [`runner-unit-tests-b12dbdd.log`](runner-unit-tests-b12dbdd.log)
- Scoped repository verification:
  [`repository-verification-b12dbdd.log`](repository-verification-b12dbdd.log)

The immutable campaign package contains 113 files totaling 8,269,630 bytes: 112 retained artifacts
plus the manifest itself. `sha256sum -c artifact-manifest.sha256` passed for all 112 entries; the
manifest cannot include its own digest.

## Acceptance Result

| Criterion | Result | Evidence |
| --- | --- | --- |
| Immutable implementation | `Pass` | Clean commit `b12dbddeac64397b0b15e9970251694f40632cf9`; source hashes revalidated after run 100 |
| Engine identity | `Pass` | Exact ESP-IDF, compiler, QEMU package/source/binary, GDB, and `libslirp` identities retained and revalidated |
| DOMES engine modification | `Pass` | Zero DOMES changes to the pinned Espressif QEMU source |
| Target execution | `Pass` | 100/100 fresh boots reached one complete result marker before timeout |
| Dual-core task behavior | `Pass` | Controller and main task on CPU0; second pinned task and IRQ consumer on CPU1 |
| Synchronization | `Pass` | Bidirectional block/wake completed with the exact expected phase, block, wake, and handoff counters |
| Time and interrupt | `Pass` | Target tick advanced; one CPU0 GPTimer ISR woke the blocked CPU1 consumer; zero drops |
| Determinism | `Pass` | One structural signature and one normalized observation signature across 100 runs |
| Introspection | `Pass` | HMP reported CPU0 and CPU1; GDB stopped at `domesQemuProbeComplete` and read complete `gProbeState` |
| Media integrity | `Pass` | Debug and campaign flash/eFuse hashes match; snapshot-backed inputs were unchanged after every process |
| Termination | `Pass` | Firmware reported `probe_state=complete`; runner then used SIGTERM; no SIGKILL, reset, panic, or early exit |
| Adoption boundary | `Pass` | Numeric and structural budget below accepted; no production composition, transport, device, or CI work entered |

## Reproduction Identity

The run started at `2026-08-05T01:16:21Z` on
`Linux-6.19.9-arch1-1-x86_64-with-glibc2.43` with Python 3.14.3.

| Input | Pinned identity |
| --- | --- |
| ESP-IDF | v5.4.4, commit `296b6eab9445fd720e71aecab961e2d3fbca9944`, clean tree |
| Compiler package | `esp-14.2.0_20260121`; archive SHA-256 `da31f36d79d4e99f24e55a90a71e65d5694714f16199960bf7885724b706a48c` |
| Compiler executable | `xtensa-esp32s3-elf-g++`; SHA-256 `004e294577ab054218508eaba90f92f9c2d504217ba6b78ecbd1d81f58f6ae73` |
| QEMU package | `esp_develop_9.2.2_20250817`; release tag `esp-develop-9.2.2-20250817` |
| QEMU tag/source | tag object `bd84389ad04f4c8532c12f0c7e622035cf6f9fad`; source `4f4148e2f68689eb8861bf9fce0b46ada9200fef` |
| QEMU archive | SHA-256 `588bfaccd0f929650655d10a580f020c6ba9c131712d8fa519280081b8d126eb` |
| QEMU executable | QEMU 9.2.2 Espressif build; SHA-256 `57cd2d1909c08c2b810f4bf7f6fb2c1d2523fc8d3b564e9d5e871c0f471381f7` |
| GDB | Espressif GDB `16.3_20250913` |
| `libslirp` | Arch package library `libslirp.so.0.4.0`; SHA-256 `2f71a82b07ecf02e9787b28d7f0a4691c9e3b4285b68828048dd147077fcfa24` |
| Probe ELF | SHA-256 `915b094e4af4048eb2533d62264efafda2e072fa39d59c193c58f64f870df836` |
| SDKCONFIG | SHA-256 `f4a3b2027da2ecf25a99d9c8e9c767f6080e31efc59e99a2eb1d8324350d5252` |
| Flash/eFuse | SHA-256 `ef95159a5d3d7de007512e15f3ee71ce4c9be49dbb464be2f4a713ca40dd0543` / `2054600a17c72426ac024ae851e7ea26f9cf612f31140b445ff713ba15ac09c8` |

The QEMU capability statement uses the
[Espressif feature matrix at commit `cdee381d`](https://github.com/espressif/esp-toolchain-docs/blob/cdee381dce7b88b5207ec48c72984ca31d495dd3/qemu/README.md).
The target and GDB workflow follows the
[ESP-IDF v5.4.4 QEMU guide](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/api-guides/tools/qemu.html).

The acceptance command was:

```bash
export LD_LIBRARY_PATH=/tmp/domes-qemu-libs/usr/lib
. ~/esp/esp-idf/export.sh
python3 tools/simulation/qemu_feasibility.py \
  --runs 100 \
  --build-dir /tmp/domes-qemu-b12dbdd/build \
  --artifact-dir \
    docs/evidence/qemu-feasibility/b12dbddeac64397b0b15e9970251694f40632cf9
```

The temporary library directory contained the unmodified Arch `libslirp` package because this host
did not permit a non-interactive system package install. The runner retained and revalidated the
resolved library path and digest. Normal operator setup remains installation through the host package
manager.

## Measurements And Signatures

| Measurement | Result |
| --- | --- |
| Cold build | 7.096 s |
| Cached build | 0.560 s |
| Execution minimum | 2.429 s |
| Execution median | 2.448 s |
| Execution p95 | 2.518 s |
| Execution maximum | 2.733 s |
| Execution total | 246.239 s |
| Structural signature | `9cbd8d77e8d2877198f0ff0c9a995d934ab29e41df0f267f6a440e3ce600a343` |
| Observation signature | `a38af14972481b1a2b4ac4e5cb2350196a3e9a7b5d5bbb0c5e7053455f5553ed` |

All 100 raw observations were identical: CPU0 wait 2 ticks, CPU1 wait 4 ticks, IRQ consumer wait 2
ticks, tick delta 2, GPTimer alarm 2000, observed count 2001, and count delta 1. Absolute tick start
and end were 6 and 8 in every retained run. The normalized observation removes only absolute tick
start/end; it retains every relative delay, count, task, core, IRQ, failure, cleanup, and termination
field.

## Fidelity Inventory

`Supported` describes the pinned engine documentation. `Production`, `modeled`, `adapter`,
`synthetic-load`, and `disabled` describe this run. Engine support does not mean a component was
exercised.

| Component | Run state | Exact claim |
| --- | --- | --- |
| Xtensa CPU0 and CPU1 | `modeled` | Both target CPUs executed the probe and were visible through HMP/GDB; QEMU serializes them and is not silicon or cycle accurate |
| ESP-IDF FreeRTOS target kernel/port | `production` | Real ESP-IDF v5.4.4 ESP32-S3 dual-core kernel/port executed; Amazon SMP was disabled; the probe topology is not DOMES production |
| Probe controller/tasks | `synthetic-load` | `main`, `probe_core0`, and `probe_core1` exist only to exercise B's bounded kernel behavior |
| SysTimer and target tick | `modeled` hardware plus production target path | Delay, timeout, block/wake, and tick progress were exercised; no latency prediction is made |
| Timer Group/GPTimer | `modeled` hardware plus production driver | One GPTimer alarm and ISR path were exercised |
| Interrupt matrix | `modeled` | One GPTimer route from CPU0 to a CPU1 task handoff was observed; other priorities, masks, and sources were not covered |
| NOR flash/MMU | `modeled` | Exact boot images executed; broader flash behavior was not tested |
| eFuse | `modeled`, not behaviorally verified | Default ESP-IDF image supplied; no identity or security claim |
| UART console | `modeled`; host stdio is an `adapter` | Probe logs only; no DOMES framed configuration or OTA transport |
| PSRAM | `not exercised` | The probe did not allocate or qualify external RAM |
| QEMU Timer Group watchdog model | `disabled` | The QEMU command disabled this model to isolate the bounded probe |
| ESP-IDF watchdogs | `configured`, not qualified | Bootloader, interrupt, and task watchdog options remained enabled; no watchdog timing or recovery claim is made |
| RNG | `boot initialization only`, not qualified | The boot log initialized and then disabled the early-entropy source; the probe did not consume or qualify RNG output |
| WiFi/ESP-NOW, BLE, USB, RMT, I2C, I2S, GPIO matrix | `disabled`; documented as not modeled by this engine | The complete radio, coexistence, USB, LED, sensor, audio, and GPIO defect classes remain outside B |
| NVS, DOMES trace, random source, virtual link/backplane | `disabled` | No production state, scheduler trace, deterministic radio, peer actor, or scenario claim |
| LED, touch, IMU, audio, and haptic services | `disabled`, not linked | No peripheral or service behavior claim |
| DOMES application tasks | `disabled`, not linked | `mode_tick`, `led_svc`, `imu_svc`, `audio_svc`, `touch_svc`, `game_tick`, `espnow_svc`, `serial_ota`, `ble_ota`, `tcp_config`, `trace_stream`, `diagnostics`, `mem_prof`, and `ota_check` did not execute |
| DOMES transports, services, runtime, CLI, and app | `disabled`, not linked | B did not compile or execute `firmware/domes` and did not exercise any production communication path |

## QEMU Adoption Budget

FS-WP-002B consumed **0 DOMES changes to QEMU source files and 0 changed lines** against the pinned
Espressif QEMU source. Any later DOMES QEMU extension must remain within all of these limits:

- One immutable upstream base and a single DOMES patch series of at most three commits.
- At most 10 non-generated QEMU files: no more than 6 new DOMES-owned files and 4 modified upstream
  files.
- At most 2,500 changed lines, additions plus deletions, with at most 200 changed lines in pre-existing
  upstream files.
- The only permitted new files are `hw/misc/domes_link.c`,
  `hw/misc/domes_link_backend.c`, `include/hw/misc/domes_link.h`,
  `include/hw/misc/domes_link_backend.h`, `tests/qtest/domes_link-test.c`, and
  `docs/system/domes-link.rst`.
- The only permitted existing-file changes are `hw/xtensa/esp32s3.c`,
  `hw/misc/meson.build`, `tests/qtest/meson.build`, and `docs/system/target-xtensa.rst`.
- Changes under `target/xtensa/**`, `accel/tcg/**`, `tcg/**`, `hw/timer/**`, interrupt-controller
  implementation, `replay/**`, replay headers/QAPI, or generic QEMU clock/timer behavior are
  prohibited.
- No new third-party dependency, protocol stack, scheduler, timer source, or host-network timing
  path.
- Maintenance is capped at 3 engineer-days per adopted QEMU revision and 8 engineer-days per rolling
  12 months.

Any budget breach reopens the engine decision and cannot be approved as incidental scope. Re-evaluate
the adoption on an ESP-IDF baseline or package change, relevant high/critical security issue, artifact
unavailability, host incompatibility, unexplained deterministic or hardware divergence, or any budget
breach.

## Architecture Disposition

The result is `Viable` because the unmodified pinned engine satisfies the complete bounded B probe,
repeatability, introspection, identity, artifact, and adoption criteria. This retires the engine
feasibility question and permits FS-WP-002D to be considered as the next target-platform package.

The result is invalidated by any change to the implementation commit, probe schema or semantics,
ESP-IDF/compiler/QEMU identities, deterministic QEMU controls, signature normalization, or adopted
budget, or by unexplained divergence on a supported host. It never substitutes for the later D/C/E/F/G/H
packages, independent VC-WP-002A qualification, native sanitizers, parallel stress, or physical NFF
and product-hardware evidence.
