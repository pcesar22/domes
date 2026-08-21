# Make Guarded Firmware Updates Reliably Reach A Healthy Runtime

Status: active
Current phase: reconcile the guarded-update repair on the controller-required `main` base
Repository state: issue [121](https://github.com/pcesar22/domes/issues/121) inherits the completed
implementation from required base `8ed71e4a9adadbfddbde1548ef7060bcf79a76e9`; exact-head
software checks, independent review, and separately brokered registered-pod verification gate
completion
Last updated: 2026-08-17; earlier candidate hardware evidence is historical and must not be treated
as exact-head evidence for this review artifact

## Objective and observable outcome

A valid serial firmware update must survive first-boot verification without weakening the existing
30 KiB internal-memory floor or rollback protection. If verification ultimately fails, the prior
image must return and `system crash-dump` must identify the failed verification check. Success
requires the exact updated version, health, and all self-tests after the update and after a separate
second reset.

## Authorities and contracts

- Authority: `firmware/domes/main/main.cpp` and `services/otaManager.cpp` - current boot
  verification, confirmation, and rollback path.
- Authority: `firmware/domes/main/infra/crashDumpHandler.*` - retained clean-restart evidence.
- Authority: `docs/TESTING.md` - update success, second-boot, and rollback evidence boundaries.
- Preserve: serial/BLE image bytes and framing, exact image version/hash checks, the 30 KiB internal
  heap floor, full runtime/peripheral checks, and ESP-IDF rollback on a persistent failure.
- Stop before: lowering the safety floor, confirming before complete runtime initialization,
  changing the host protocol, erasing NVS, or claiming the separately forced failure path without
  executing it.

## Affected components and generated consumers

| Component | Files or generated output | Required change |
| --- | --- | --- |
| Boot verification | `firmware/domes/main/main.cpp`, a small service helper if needed | Return the failed check, retry only within a bounded policy, then confirm or roll back |
| Restart evidence | `firmware/domes/main/infra/crashDumpHandler.*` | Retain a bounded caller-supplied reason for a clean rollback restart |
| Host tests | `firmware/test_app/main/`, `firmware/test_app/CMakeLists.txt` | Cover result mapping, retry bounds, recovery, and persistent failure |
| Program status | `PROGRAM_STATUS.md` | Record the reproduced defect, exact repair evidence, remaining limits, and next pointer |

## Stages and dependencies

- [x] Reproduce the failure with exact image version `v0.1.0-26-gd9a22c6`, binary SHA-256
  `04321210...2c6`, and ELF SHA-256 `84459e06...bd98` on registered pod 2.
- [x] Verify the retained boot-28 snapshot against the exact ELF; its backtrace resolves through
  `handleOtaVerification()` and `OtaManager::rollback()`, proving deliberate self-test rollback.
- [x] Persist the exact failed self-test check and replay the guarded update: boot 30 retained
  `ota verify failed: internal-heap=26499`, then rolled back cleanly to the healthy prior image.
- [x] Implement and host-test a heap-only bounded recovery policy while preserving immediate
  rollback for every non-memory failure and final rollback after the retry limit.
- [x] Dispatch verification after `app_main` returns so the test measures steady-state memory
  without allocating another task stack. Boot 34 passed the unchanged heap floor at 33,003 bytes.
- [x] Execute that verification on the existing LED task so the active LED-frame check
  is serialized with animation updates instead of racing the same hardware channel.
- [x] Run 12 focused host tests and a clean isolated ESP-IDF v5.4.4 build.
- [x] Replay exact serial update on registered pod 2, verify version/health/self-test, reset again,
  and repeat the checks without claiming the separately forced failure path.
- [x] Reapply the focused change to controller base
  `d58c1a2df84d8d0a3257ff65057e1a3f32033e2f` without prerequisite history.
- [x] Reconcile the implementation onto controller base `8ed71e4`, which already contains the
  merged guarded-update repair from commit `2be8679` without weakening its safety contracts.
- [ ] **Current:** Pass exact-head software checks and CI, publish one PR against `main`, then stop
  for independent review and registered-pod verification.

## Verification

| Evidence level | Command or observation | Status and artifact |
| --- | --- | --- |
| Automated | focused host tests plus fresh isolated ESP-IDF v5.4.4 build | historical candidate passed; exact review-head rerun pending |
| Reproduction | exact serial update followed by `system info`, `system self-test`, and SHA-matched restart-snapshot verification | passed: transfer completed; boot count 27 to 29; old `ota_0` image returned with software-restart reason; rollback backtrace verified |
| Accepted command | serial update of the repaired image, reconnect, version, health, self-test, second reset, repeat | passed on pod 2: boot 36 ran exact `v0.1.0-27-g434d11f` from `ota_1` with 31,575 bytes free and 10/10 self-test; external reset produced boot 37 on the same version/slot with 31,587 bytes free and 10/10 self-test |
| Physical confirmation | LED/touch/motion/haptic/audio behavior | outside this implementation role; no physical-output claim |
| Forced rollback | purpose-built failure image selects the previous slot | not scheduled in this package unless separately authorized; remains unverified |

## Decisions, discoveries, and deviations

- This repair outranks FS-WP-002E because a supported field-update path reproducibly rejects a
  valid image, while the radio seam is planned work with no current failure.
- Transfer, embedded-version matching, full-file SHA-256, and image validation all passed. The new
  image booted and invoked explicit rollback rather than crashing, so transport and panic handling
  are outside the repair.
- The restart snapshot's original 28,123-byte value is sampled after its own NVS handle opens, so it
  did not identify the earlier check. The diagnostic replay retained the exact result: internal heap
  was 26,499 bytes at the self-test, while the same hardware and runtime later settle above the
  unchanged 30 KiB floor.
- Waiting inside `app_main` did not recover memory: clean candidate `84fa19b` remained around 26 KiB
  through all three attempts and rolled back at boot 32. While it was pending, `system health`
  showed the temporary `main` task; after rollback that task was absent and free heap was 31,795
  bytes. The floor is valid, but its old call site included a 4,096-byte startup stack that cannot
  be released while the check blocks that same task.
- Dispatching from the existing diagnostics task proved the lifecycle fix: boot 34 reached 33,003
  bytes of internal heap. It then retained `ota verify failed: led-output`; the exact ELF backtrace
  placed `performSelfTest()` on `diagnostics` while the independent `led_svc` task was also running.
  Both paths drove the same RMT-backed LED driver, so the next iteration preserves the active output
  test but runs the whole callback on `led_svc`, the hardware-channel owner. No task or stack is
  added, and the diagnostics service returns to periodic reporting only.
- Clean candidate `434d11f` embedded `v0.1.0-27-g434d11f`, binary SHA-256
  `862efe38c9bdb92f436587c619732c78f2bba3669012fa7687fe529db1ac24da`, and ELF SHA-256
  `125faaa010c45b18f0e92ac85cd6b4129643fa1de8d909ce8d2f4383836c5f63`. Its guarded serial
  transfer completed, boot 36 remained on `ota_1`, and a separate CP2102N reset reached boot 37 on
  the same version and slot. This proves image confirmation and second-boot survival; the command
  suite establishes driver readiness, not observed light, touch, motion, vibration, or sound.
- The deterministic controller supplied reconciled base `8ed71e4`; this issue branch must descend
  from that exact commit and target `main`. This package also keeps ordinary boot completion on the
  LED-owner task and makes dispatch failure explicit: a pending image rolls back with retained
  diagnostics, while an already-valid image remains incomplete without off-owner LED access.

## Resume checkpoint

Run focused host tests, a fresh isolated ESP-IDF v5.4.4 build, and repository verification. Publish
one PR for issue 121 against `main`, monitor required software CI, and stop at independent review.
Registered-pod update and second-reset evidence belongs to the separate verification worker
exercising the immutable reviewed commit.
