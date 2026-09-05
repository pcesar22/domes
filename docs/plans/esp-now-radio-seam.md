# ESP-NOW production radio seam

Issue #123 implements FS-WP-002E at specification revision
`8ed71e4a9adadbfddbde1548ef7060bcf79a76e9`.

## Implemented boundary

`EspNowTransport` owns queues, semaphores, serialization-neutral metadata, send serialization,
timeouts, poisoning and recovery. It depends only on the project-owned `IEspNowRadio` contract.
`PhysicalEspNowRadio` is the production ESP-IDF adapter and owns vendor initialization, callback
registration, peer conversion, synchronous submission results, asynchronous completion and the
copy of receive metadata out of the vendor callback lifetime.

Opaque nonzero 32-bit tokens are monotonic modulo their bounded representation. RX tokens originate
at the physical callback and travel in the internal ring item through queue send, semaphore give,
semaphore take, queue receive and dispatch trace events. TX tokens originate at submission and are
matched exactly at asynchronous completion. Tokens are never appended to the ESP-NOW payload and
the 16-byte `TraceEvent` ABI is unchanged.

## Maximum-frame capacity

ESP-IDF no-split ring items use an 8-byte item header and four-byte payload alignment. The 2,048-byte
RX ring therefore has these maximum-payload calculations:

| Layout | Metadata | Aligned item storage | Maximum 250-byte frames |
| --- | ---: | ---: | ---: |
| Before correlation | 9 bytes | `8 + align4(9 + 250) = 268` bytes | `2048 / 268 = 7` |
| With correlation | 13 bytes | `8 + align4(13 + 250) = 272` bytes | `2048 / 272 = 7` |

The calculation is encoded as compile-time constants and a static assertion. The host regression
fills seven maximum frames, verifies the eighth is dropped, and drains all seven with their original
tokens.

## Trace capture capacity

Kernel trace capture uses two fixed `DRAM_ATTR` arrays of 1,024 16-byte events, for an actual
allocation-free capture capacity of 32 KiB. The downstream 96 KiB no-split ring accounts for its
eight-byte item header and retains 4,096 events, at least twice the 2,048-event capture maximum.
Compile-time invariants tie the per-core arrays, reported capture bytes, and downstream capacity;
a focused host regression exposes the same calculation. The earlier 512-event arrays retained only
16 KiB and could overflow before the downstream ring was used.

## Verification boundary

Host tests cover adapter and transport lifecycle, peer operations, synchronous and asynchronous
send outcomes, receive metadata ownership, every correlation trace boundary, saturation, timeout
poisoning and lifecycle recovery. The physical-profile image contains `PhysicalEspNowRadio`; there
is no QEMU radio implementation in this package. Two-board behavior and physical equivalence remain
deferred to the separate verification of the exact reviewed firmware commit.
