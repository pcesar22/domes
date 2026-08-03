# Change-Aware Verification

`scripts/verify.sh` remains the complete local software gate with no arguments. Scoped modes reduce
iteration time; they do not weaken the final gate or replace hardware evidence.

```bash
scripts/verify.sh
scripts/verify.sh --quick
scripts/verify.sh --changed origin/main
scripts/verify.sh --component firmware
scripts/verify.sh --component cli --component docs
scripts/verify.sh --changed HEAD~1 --json-summary /tmp/verify.json
scripts/verify.sh --component flutter --keep-artifacts /tmp/domes-verify
```

`--dry-run` resolves the plan and writes summaries/artifacts without executing checks. Components
are `firmware`, `cli`, `flutter`, `docs`, `tooling`, `protocol`, and `workflow`.

## Selection Contract

| Scope | Checks |
| --- | --- |
| Full | Generated bindings, host firmware, CLI, host tooling, Flutter, clean ESP-IDF build/package |
| `--quick` | Full gate except the ESP-IDF build/package; not final firmware evidence |
| Docs or ordinary tooling | Host tooling and documentation |
| Firmware | Host firmware, host tooling, clean isolated ESP-IDF build/package |
| CLI | Rust CLI and host tooling |
| Flutter | Flutter restore/analysis/tests/build and host tooling |
| Protocol, transport, runtime config, or OTA | Every firmware, Rust, and Flutter consumer plus tooling |
| Workflow or unknown path | Full software gate |

`--changed <base>` compares the base commit with the current worktree and includes untracked files.
Deleted paths are also classified. A protobuf, shared frame/protocol, transport, config handler,
OTA, or workflow path can never select only one language consumer.

## JSON Schema Version 1

`--json-summary <path>` writes an atomic JSON object with:

- `schema_version`, timestamps, mode, base, components, changed paths, and quick status;
- `artifacts`, or `null` when logs were not retained;
- every check's ID/title, matching required CI job, selection boolean and reasons, status, exit
  code, duration, and log path;
- `hardware` entries with `outstanding`, `not_assessed`, or no selected obligation;
- pass/fail/skip counts and the reliable process exit code under `summary`.

Check statuses are `passed`, `failed`, and `skipped`. A selected check that fails to record a result
is treated as failed. `--keep-artifacts <directory>` creates a unique timestamped child containing
the selection plan, result ledger, complete per-check logs, host-test build, and firmware build and
release artifacts when those checks run. Without retention, the temporary tree is removed.

## Hardware Boundary

Structured hardware entries state what remains. Software checks, including a clean firmware build,
cannot mark physical behavior complete. Follow `docs/TESTING.md` for single-pod, transport, BLE,
multi-device, OTA success, and separately forced rollback evidence.
