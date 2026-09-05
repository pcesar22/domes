# NFF internal-memory repair

## Scope and evidence boundary

This repair is tied to specification revision
`3b62a6c82160d0271b276bb42894e3d3bb69761e` and the physical NFF runtime profile generated from
`firmware/domes/profiles/runtime_profiles.json`. It does not change that profile, task stacks,
features, protocols, partitions, or readiness thresholds.

The September 5 two-NFF results are retained historical measurements, not measurements made by
this implementation worker. Both pods reported 9/10 self-tests with only `Heap` failing. Stable
samples reported 322,571 bytes of internal-capability heap in total, 23,331 bytes free, 18,259
bytes minimum-free, and a 7,680-byte largest block. The current free value was 7,389 bytes below
the unchanged 30 KiB firmware readiness floor. The measured application SHA-256 was
`ce924e2f54625fe00a3aa3102b06ff03291a0e6a9c887ad883e97f462358ac42`; the ELF SHA-256 was
`2714d986ebbcd46e9b05e9c8f3b39620737069f2d1cb989e9c711c00c56ccbab`.

Everything below is source/build analysis or a prediction until the candidate is separately
flashed and measured on both NFFs.

## Allocation attribution

| Owner | Source-level amount | Capability and lifetime | Attribution |
| --- | ---: | --- | --- |
| Generated-tone PCM | 32,000 B plus allocator metadata | Explicit `MALLOC_CAP_INTERNAL`, allocated when the audio task starts and held until it exits | Proven direct consumer of the measured capability pool; allocated even while audio is disabled |
| Audio task stack | 4,096 B plus task control metadata | FreeRTOS task stack, audio-service lifetime | Generated physical-profile value; retained unchanged because there is no measured stack-watermark basis for shrinking it |
| I2S DMA | 4 buffers x 256 frames; at least 2,048 B of 16-bit mono sample storage, plus descriptors/driver state | DMA-capable memory, driver lifetime | Source/config accounting; exact allocator overhead is ESP-IDF-owned |
| MAX98357A scaling scratch | 512 B | Automatic storage on the unchanged audio task stack during `write()` | Source accounting; it does not consume a separate persistent heap block |
| Trace ring request | 98,304 B plus FreeRTOS ring metadata | Default allocator, recorder lifetime | Source accounting only. The NFF config routes sufficiently large ordinary allocations toward PSRAM, but the resolved capability and fragmentation contribution were not measured, so it is not assigned as the physical shortfall's cause |
| Trace retained-snapshot table | 4,096 target pointers, 16,384 B on ESP32-S3 | Part of the trace-buffer object allocated by the default allocator | Source accounting only; allocator placement was not measured and ISR/cache safety prevents moving trace state by assumption |
| Memory-profiler samples | 38 x 16 B = 608 B, plus indexes and synchronization state | Static storage for the runtime lifetime | Exact source/generated-schema accounting |
| Memory-profiler task stack | 4,096 B plus task control metadata | FreeRTOS task stack, runtime lifetime | Generated physical-profile value; retained unchanged without stack-watermark evidence |

The 32,000-byte tone buffer is the smallest safe repair target because it alone exceeds the
observed 7,389-byte deficit, has an explicit internal-only allocation, and is neither ISR nor
cache-critical state. Changing trace capacity or either task stack would carry unrelated trace or
stack-safety risk and is unnecessary for this candidate.

## Repair design and accounting

The audio service now owns one 256-sample (512-byte) PCM chunk as part of its static service
object. A one-second tone remains capped at 16,000 samples, but it is generated and synchronously
copied to the audio driver in at most 63 chunks. Phase and the existing global 10 ms fade rules are
preserved across chunk boundaries. Asset playback, feature checks, driver start/stop order, and the
50 ms post-write settling delay are unchanged.

`IAudioDriver::write()` synchronously accepts/copies the supplied samples into driver-managed DMA
storage before returning, so the source chunk itself does not require DMA allocation capabilities.
The buffer has one owner (the audio task), no ISR access, no allocation call, and no fragmentation
path. Driver errors propagate immediately; an `ESP_OK` result that accepts fewer than the offered
samples fails closed rather than silently truncating a tone. Generation and copying remain bounded
at 16,000 samples and 63 writes.

| Item | Before | Candidate | Difference |
| --- | ---: | ---: | ---: |
| Persistent tone workspace | 32,000 B explicitly internal, plus heap metadata | 512 B in the static audio-service object | At least 31,488 B removed from the persistent internal-heap demand, before allocator metadata |
| Maximum generated samples | 16,000 | 16,000 | No change |
| Audio task stack | 4,096 B | 4,096 B | No change |
| Trace ring request | 98,304 B | 98,304 B | No change |
| Memory-profiler stack | 4,096 B | 4,096 B | No change |

Applying only the 31,488-byte source-level difference to the retained 23,331-byte free sample
predicts 54,819 bytes free, or 24,099 bytes above the 30 KiB floor. That arithmetic is a design
margin, not physical proof: allocator metadata, boot-to-boot placement, fragmentation, minimum
headroom, and peripheral-active behavior must be measured after a separately authorized reflash.

The firmware self-test/OTA floor remains `30 * 1024` internal-capability bytes. The CLI health
floor remains `16 * 1024` bytes. No capability or readiness check is disabled. Issue #106 is
related first-boot history only; it is not a dependency and is outside this repair.

## Verification contract

Host regression tests cover the fixed chunk bound, complete one-second generation, duration cap,
short-tone behavior, cross-chunk continuity, driver-error propagation, and partial-write rejection.
A final candidate also requires a fresh physical-profile build with ESP-IDF v5.4.4 and an isolated
`SDKCONFIG`, with binary, ELF, config identities and size retained in the pull request.

Physical acceptance remains pending by design. After independent safety review and separate
authorization, both NFFs must be reflashed with the exact reviewed image and demonstrate complete
readiness, the peripheral-active self-test, and stable current/minimum/largest-block headroom after
the test. Build results and static accounting do not establish that outcome.
