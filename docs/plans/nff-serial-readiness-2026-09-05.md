# Two-NFF firmware and serial readiness

Status: complete
Current phase: requested programming/communications verified; full readiness failed
Repository state: local `codex/feat/development-dashboard` at `8d7b5db`; preserve existing edits in
`.codex/PLATFORM.md` and `tools/agent_control/test_control.py`. Hardware host checkout is clean at
latest `origin/main`, `3b62a6c82160d0271b276bb42894e3d3bb69761e`.
Last updated: 2026-09-05 00:26 UTC (2026-09-04 Pacific).

## Result

Both NFFs were programmed from the same fresh latest-main image and successfully answered
`domes-cli` queries independently and in explicit two-target commands. Programming and UART
communications passed. Full device readiness failed on both: 9/10 self-tests, with only the internal
Heap test failing. This condition was directly confirmed on untouched Pod 2 before programming.

The O2 serial-access prerequisite is established, not complete O2/NFF qualification. Repair and
reverify the internal-memory shortfall before claiming full readiness or proceeding to broader
campaigns that require it. No firmware change, threshold waiver, or program-gate transition was made.

## Objective and observable outcome

Build the latest main firmware using ESP-IDF v5.4.4 and a fresh isolated SDKCONFIG, program both
registered NFF boards, and verify each exact firmware identity and framed UART responses through
a CLI built from the same revision. Retain build and device logs. This is communications readiness
for the requested O2 workstream, not complete NFF characterization or a program-gate transition.

## Authorities and contracts

- `firmware/AGENTS.md`, `firmware/README.md`, `docs/TESTING.md`: clean build and runtime verification.
- `.codex/PLATFORM.md`: use the operator's `ministrom` alias; CP2102N for flash/framed UART.
- `tools/firmware/flash_and_verify.sh`: version, health and complete self-test acceptance.
- Preserve NVS identities and unrelated checkouts; no full-flash erase, forced rollback, hardware
  load campaign, host-service change, GitHub publication, or dashboard/gate update.

## Stages and dependencies

- [x] Latest main fetched; clean hardware-host checkout matches `3b62a6c`.
- [x] Both historical CP2102N serial identities found on `ministrom`.
- [x] Confirm toolchains, serial permissions, port ownership and pre-flash identity.
- [x] Build CLI and retained firmware; verify NFF 8 MB profile, image metadata and hashes.
- [x] Program both boards sequentially; retain exact version, identity, health and self-test results.
- [x] Verify repeated independent and explicit two-target CLI responses; retain final evidence.
- [ ] Full readiness remains failed: investigate internal allocations and restore >=30 KiB headroom.
  This follow-on firmware change is not implemented by the present build/flash request.

## Verification

| Evidence | Required result | State |
| --- | --- | --- |
| Source/toolchain | Exact main SHA, ESP-IDF 5.4.4, Rust 1.92.0, fresh build/SDKCONFIG | Passed; remote build logs retained |
| Pod 1 | USB serial `5edf3f45576def11a245cea7c169b110`, WiFi MAC `94:a9:90:0a:eb:c0` | MAC/8 MB flash matched; retained ID 1, post-flash boot 37 |
| Pod 2 | USB serial `002a9f8e536def119f38c1a7c169b110`, WiFi MAC `94:a9:90:0a:ea:50` | MAC/8 MB flash matched; retained ID 2, post-flash boot 90 |
| Firmware programming | Flash verification and exact running build version on both | Both passed; all four written-region hashes verified |
| UART command acceptance | `system info`, `system health`, `feature list` | Both passed; three final two-target info rounds returned 6/6 expected identities/versions |
| Device self-test | All discovered tests must pass | Both failed 9/10, solely Heap (`22KB free, 17KB min`) |
| Full readiness | Explicit two-target `system readiness` | FAIL (0/2 targets ready; 2 failed), exit 1; both health PASS/self-test FAIL |
| Physical outputs/RF/power | Observer/instrument evidence and broader campaigns | Not exercised in this task |

## Decisions, discoveries, and deviations

- Build the latest main revision, not the older firmware base beneath the dashboard branch.
- `ministrom` lacks the optional `hostname` command; SSH and Linux host identity are confirmed.
- Use the documented isolated build plus explicit flash commands to retain binaries and metadata;
  the convenience helper removes its temporary build on exit.
- Both pre-flash boards already reported `v0.1.0-267-g3b62a6c`; this task freshly rebuilds and
  reprograms that latest main revision rather than claiming a firmware-version upgrade.
- Retained remote run: `/home/pncosta/domes-nff-readiness.laujhy`. App image is 1,486,176 bytes,
  fits each 1,966,080-byte app slot with 479,904 bytes free (24%). Physical NFF profile selected;
  QEMU profile and destructive rollback override are not enabled.
- App SHA-256: `ce924e2f54625fe00a3aa3102b06ff03291a0e6a9c887ad883e97f462358ac42`.
- Doctor's all-software result is unavailable because Flutter, Dart/protoc plugin, pre-commit and
  ShellCheck are absent on the hardware host. Pinned firmware/Rust and two UART devices are
  available. Native USB console/JTAG cables are absent; none of those broader gaps is a UART pass.
- Pod 1 flash completed with all written-region hashes verified. Existing partition bytes matched
  the candidate exactly; current chip MAC and 8 MB flash matched the historical identity.
  After flash it retained ID 1, reached boot 37 and the expected version, and passed health with
  23,459 bytes free / 18,259 minimum. Self-test reported 9/10: Heap failed (`22KB free, 17KB min`).
  Running App passed on `ota_0`, `rollback=no`; every other discovered self-test passed.
  This is a real failing acceptance result, not complete readiness.
- Untouched Pod 2 also passed health but failed only Heap in the full 9/10 self-test, with the same
  `22KB free, 17KB min` result. Its firmware already had this condition before programming.
  Both targets answered explicit two-port `feature list`. Proceeding with Pod 2's requested standard
  flash preserves the same candidate and does not waive the failure or authorize O2 load tests.
- Health and self-test use different documented thresholds: CLI health requires current/minimum
  internal heap >=16 KiB; the firmware self-test requires current internal heap >=30 KiB.
  Do not reduce either threshold. Stable Pod 1 memory samples report approximately 23,331 bytes
  current free, 18,259 minimum, and a 7,680-byte largest contiguous internal block.
- Pod 2 also verified its expected MAC, 8 MB flash and exact existing partition-table bytes before
  programming. Its post-flash health measured 23,459 free / 18,259 minimum and passed. Its sole
  self-test failure was unchanged. Both boards remain on `ota_0`, `rollback=no`; no OTA or forced
  rollback success is established by standard programming.
- Final three-round identity queries kept boot counts 37 and 90 unchanged, and both versions were
  `v0.1.0-267-g3b62a6c`. Final feature queries succeeded on both; ESP-NOW remains disabled.
- The self-test is classified as activity: it temporarily changes IDLE to TRIAGE, enabling touch,
  haptic and audio gates. `system readiness` therefore has this expected transient mode side effect
  even though it does not send feature-enable commands. After the 30-second inactivity timeout,
  passive handoff checks confirmed both returned to IDLE, mask `0x6`, LED/BLE enabled and
  ESP-NOW/touch/haptic/audio disabled. Boot counts remained 37/90; no explicit restoration was needed.
  This mode transition is not physical peripheral validation.
- The same 30 KiB internal-heap floor is used by OTA image verification in
  `firmware/domes/main/main.cpp`; do not assume a direct-flash pass implies OTA acceptance.
  Exact allocation attribution remains unproven; no memory-threshold reduction is authorized.

## Retained evidence and reproducible access

Remote retained build and logs: `/home/pncosta/domes-nff-readiness.laujhy` on `ministrom`.
Local evidence copy: `.artifacts/nff-serial-readiness-2026-09-05/` (ignored, not published).
Both copies retain the exact application, bootloader, partition table, initial OTA data, ELF,
SDKCONFIG, metadata, programming script and logs. The remote copy also retains the complete build
tree and matching freshly built CLI. Local `flash_args` keeps the original build-relative paths;
the local flat bundle is a retention copy, not a directly runnable provisioning directory.

Key records:

- `firmware-build.log`, `cli-build.log`, `artifact-sha256.txt`.
- `pod1-flash.log`, `pod2-flash.log`, per-board chip identity and partition-read logs.
- `pod1-postflash-info.log`, `pod2-postflash-info.log`, `pod1-health.log`, `pod2-health.log`.
- `pod1-self-test.log`, `pod2-self-test.log`, and `pod2-preflash-self-test.log`.
- `final-three-rounds-two-target-info.log`, `final-two-target-features.log`.
- `final-two-target-readiness.log`, `final-readiness-exit.log`.
- `handoff-two-target-info.log`, `handoff-two-target-features.log`: both returned to IDLE.
- `pod1-memory.json`, `pod2-memory.json`.

Run on the hardware host, not the local workstation:

```bash
ssh ministrom
CLI=/home/pncosta/domes-nff-readiness.laujhy/cli-target/debug/domes-cli
PORT1=/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_5edf3f45576def11a245cea7c169b110-if00-port0
PORT2=/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_002a9f8e536def119f38c1a7c169b110-if00-port0
$CLI --port "$PORT1" --port "$PORT2" system info
$CLI --port "$PORT1" --port "$PORT2" feature list
# Expected to FAIL until the memory shortfall is fixed and reverified:
$CLI --port "$PORT1" --port "$PORT2" system readiness
```

## Resume checkpoint

Primary owns all device access. Remote root is `/home/pncosta/domes`; do not change its checkout.
Both flash scripts ended with status 1 at the expected, unwaived Heap self-test failure after
successful programming. Final communication checks passed and the exact readiness failure was
retained. No hardware campaign is active. Remote checkout remains clean at `3b62a6c`; local user
edits are preserved and this report is intentionally uncommitted. Next action is a separately scoped
internal-memory investigation/repair, not additional flashing or a threshold waiver.
