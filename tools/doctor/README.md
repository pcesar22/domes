# DOMES Capability Doctor

Run `scripts/doctor.sh` before choosing a verification path. The command is read-only: it does not
install packages, change permissions, restart Bluetooth, initialize submodules, open serial ports,
flash devices, register pods, or alter GitHub state.

```bash
scripts/doctor.sh
scripts/doctor.sh --json
```

Exit code `0` means all mandatory repository/software prerequisites are available at their required
versions. Exit code `1` means at least one is missing, mismatched, or failed to report. Optional
hardware and Bluetooth remain `unavailable` without making the command fail. Exit code `2` is
reserved by the argument parser for invalid usage.

## JSON Schema Version 1

The top-level object contains:

| Field | Meaning |
| --- | --- |
| `schema_version` | Integer contract version; currently `1` |
| `generated_at` | UTC timestamp |
| `repository` | Root, submodule revisions/status, and remediation |
| `host` | OS, release, architecture, and native-Linux classification |
| `tools` | Required command status, parsed/expected version, detail, and remediation |
| `devices` | Stable by-id CP2102N and native USB records, access, group, and pod count |
| `bluetooth` | BlueZ query status, version, adapters, power state, and remediation |
| `capabilities` | Feasibility of software, single/two-device, BLE, ESP-NOW, OTA, and hardware CI |
| `summary` | Mandatory failure count, available capability count, and process exit code |

Statuses are `available`, `unavailable`, `failed`, or `not_applicable`. `unavailable` means a
prerequisite is absent; `failed` means an expected probe ran but produced an invalid result;
`not_applicable` means the host platform cannot perform that workflow. Device identity comes only
from `/dev/serial/by-id/`; unstable `ttyUSB` numbers are never reported as pod identity.

Remediation is advisory text for deliberate user action. The doctor never executes it.
