# DOMES Trace Overhaul: Architecture Design

**Author:** Research Agent (Task #3)
**Date:** 2026-02-10
**Status:** Design Proposal

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Current System Analysis](#2-current-system-analysis)
3. [Industry Research & Best Practices](#3-industry-research--best-practices)
4. [New Architecture Design](#4-new-architecture-design)
5. [Protocol Design (trace.proto)](#5-protocol-design-traceproto)
6. [Transport Separation](#6-transport-separation)
7. [Dual-Mode Operation](#7-dual-mode-operation)
8. [Host Integration (domes-cli)](#8-host-integration-domes-cli)
9. [Multi-Pod Correlation](#9-multi-pod-correlation)
10. [Migration Path](#10-migration-path)
11. [References](#11-references)

---

## 1. Executive Summary

The current DOMES tracing system is functional but has four architectural pain points:

1. **Shared serial contention** — Traces share USB-CDC with ESP-IDF logs; during binary dump, ALL logging must be suppressed
2. **Hand-rolled wire protocol** — Metadata/control structs use packed binary, not protobuf (inconsistent with config/OTA)
3. **Post-mortem only** — No real-time trace streaming; must stop tracing to dump
4. **No cross-pod correlation** — Multi-device traces produce separate files with no timestamp alignment

This document proposes a new architecture that:

- Migrates control messages and metadata to protobuf (keeping 16-byte events binary)
- Separates logs from traces using frame-wrapped log forwarding
- Adds real-time streaming with category-based filtering and flow control
- Enables cross-pod trace correlation via ESP-NOW time-sync beacons
- Consolidates all host tooling into domes-cli (deprecating Python tool)

**Key design constraint:** The 16-byte `TraceEvent` struct is excellent and should NOT be changed. It is compact, ISR-safe, and cache-friendly. The overhaul targets everything around it.

---

## 2. Current System Analysis

### 2.1 What Works Well

```
┌─────────────────────────────────────────────────────────────────┐
│  STRENGTHS (keep these)                                         │
├─────────────────────────────────────────────────────────────────┤
│  - 16-byte TraceEvent: fixed-size, ISR-safe, no allocation     │
│  - FNV-1a compile-time hashing: zero runtime cost for names    │
│  - FreeRTOS ring buffer (NOSPLIT): thread-safe, zero-copy read │
│  - Category system (13 categories): good granularity           │
│  - trace.proto as source of truth for enums                    │
│  - Frame protocol (0xAA 0x55 framing, CRC32): robust           │
│  - Perfetto output: industry-standard visualization            │
│  - 2048 events in 32KB: efficient memory use                    │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 What Needs Improvement

```
┌─────────────────────────────────────────────────────────────────┐
│  PAIN POINTS (fix these)                                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. LOG SUPPRESSION DURING DUMP                                 │
│     - esp_log_level_set("*", ESP_LOG_NONE) during dump         │
│     - If crash during dump → lost logs AND incomplete dump     │
│     - Logs and trace share USB-CDC byte stream                  │
│     - No way to interleave structured trace with text logs     │
│                                                                 │
│  2. CONTROL MESSAGES USE PACKED BINARY, NOT PROTOBUF           │
│     - TraceMetadata (17 bytes), TraceDataHeader (6 bytes),     │
│       TraceDumpEnd (8 bytes), TraceStatusResponse (14 bytes)   │
│     - Manually kept in sync across C++, Rust, and Python       │
│     - Fragile: endianness, alignment, size assertions           │
│     - Inconsistent: config/OTA use protobuf, trace doesn't    │
│                                                                 │
│  3. POST-MORTEM ONLY                                            │
│     - Must stop recording → dump → resume                      │
│     - Cannot observe events as they happen                      │
│     - 20ms delay between chunks (flow control by sleeping!)    │
│     - No filtering: always dumps entire buffer                  │
│                                                                 │
│  4. NO CROSS-POD CORRELATION                                    │
│     - Multi-device dump creates separate JSON files             │
│     - No timestamp alignment between devices                    │
│     - No merged view of distributed game sessions               │
│     - No pod ID in trace events                                 │
│                                                                 │
│  5. DUPLICATE HOST TOOLS                                        │
│     - Python: trace_dump.py (original, full features)          │
│     - Rust: trace.rs in domes-cli (newer, but span names       │
│       not loaded from trace_names.json)                         │
│     - Both reimplement frame protocol, event parsing            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 Current Data Flow

```
  Application Code                FreeRTOS Hooks
  TRACE_SCOPE()                   domes_trace_task_switched_in()
  TRACE_INSTANT()                 domes_trace_isr_enter()
       │                                │
       ▼                                ▼
  ┌─────────────────────────────────────────┐
  │         TraceRecorder (singleton)        │
  │  record() / recordFromIsr()             │
  └────────────────┬────────────────────────┘
                   ▼
  ┌─────────────────────────────────────────┐
  │     FreeRTOS Ring Buffer (32KB)         │
  │   NOSPLIT mode, ~2048 events            │
  └────────────────┬────────────────────────┘
                   │ (DUMP command received)
                   ▼
  ┌─────────────────────────────────────────┐
  │     TraceCommandHandler                  │
  │  1. Suppress ALL logs                   │
  │  2. Send metadata (binary packed)       │
  │  3. Stream 8-event chunks               │
  │  4. 20ms sleep between chunks           │
  │  5. Send end marker                     │
  │  6. Restore logs                        │
  └────────────────┬────────────────────────┘
                   │ Frame protocol (0xAA 0x55)
                   ▼
  ┌─────────────────────────────────────────┐
  │  USB-CDC Serial (shared with ESP logs!) │
  └────────────────┬────────────────────────┘
                   │
                   ▼
  ┌─────────────────────────────────────────┐
  │  Host: Python (trace_dump.py)           │
  │     or Rust (domes-cli trace dump)      │
  │  → Chrome Trace JSON → Perfetto         │
  └─────────────────────────────────────────┘
```

### 2.4 Current Wire Overhead

| Item | Format | Size | Notes |
|------|--------|------|-------|
| TraceEvent | Binary packed | 16 bytes | Optimal, keep as-is |
| Frame overhead | Binary | 9 bytes | 2 start + 2 len + 1 type + 4 CRC |
| TraceMetadata | Binary packed | 17 + 18*N bytes | N = task count |
| TraceDataHeader | Binary packed | 6 bytes | Per chunk |
| Data chunk | Binary | 6 + 128 bytes = 134 | 8 events per chunk |
| Frame + chunk | Binary | 134 + 9 = 143 bytes | Per chunk |
| Per-event amortized | - | ~18 bytes | 143/8 = 17.9 bytes |

---

## 3. Industry Research & Best Practices

### 3.1 ESP-IDF app_trace

ESP-IDF provides an Application Level Tracing library ([docs](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/app_trace.html)) with two modes:

- **Post-mortem mode**: Overwrites old data in ring buffer (like our current approach)
- **Streaming mode**: Waits for host to consume before overwriting (backpressure)
- Supports JTAG, UART, and USB transports
- Separates trace data from logs by routing logs through `esp_apptrace_vprintf()` to host

**Relevance to DOMES:** The key insight is using a *separate channel* for traces. ESP-IDF uses JTAG for this, which we can't (single USB port). Instead, we'll use frame-type multiplexing on the same serial port.

### 3.2 defmt (Rust Embedded) and Trice (C Embedded)

Both frameworks demonstrate the **deferred formatting** pattern:

**defmt** ([GitHub](https://github.com/knurling-rs/defmt)):
- Device sends compact tokens (string table indices) + raw argument values
- Host reconstructs full messages using ELF-embedded string table
- Result: ~9x reduction in binary/wire size vs traditional printf
- 1.6 KB total vs 13.8 KB for equivalent functionality

**Trice** ([Docs](https://rokath.github.io/trice/)):
- C-native approach: each log gets a numeric ID, stored in `til.json`
- Wire format: ~4 bytes per message (ID + inline params)
- Sub-microsecond execution, safe in ISR context
- Host Go tool resolves IDs to format strings

**Relevance to DOMES:** We already use a similar pattern! Our FNV-1a hash IDs + trace_names.json is conceptually equivalent to Trice's ID-based approach. No change needed — but we should:
- Embed span names in the binary (ELF section) for automatic extraction
- OR auto-generate trace_names.json from source at build time

### 3.3 Pigweed pw_trace

Google's Pigweed ([pw_trace](https://pigweed.dev/pw_trace/), [pw_trace_tokenized](https://pigweed.dev/pw_trace_tokenized/)):
- Clean separation: facade (macros) → backend (storage/transport)
- Tokenized backend: compresses all compile-time data into uint32 token
- Ring buffer storage with de-ring for bulk transfer
- Sink callback API: `SinkStartBlock`, `SinkAddBytes`, `SinkEndBlock`
- Supports both ring buffer (post-mortem) and streaming (via sinks)

**Key insight: The sink callback pattern.** Events flow to both ring buffer (for post-mortem dump) AND optional streaming sinks (for real-time). This is the dual-mode architecture we should adopt.

### 3.4 Common Trace Format (CTF)

Linux kernel's CTF ([spec](https://diamon.org/ctf/)):
- Transport-agnostic binary format
- Per-stream packet headers with timestamps
- Variable-length events with typed fields
- Ring buffer per CPU core

**Relevance:** CTF's per-stream approach maps to per-pod traces. Its packet structure (header + events) is similar to our chunk format. Key takeaway: include pod-ID and clock metadata in trace session headers.

### 3.5 Protobuf Overhead Analysis

For a 16-byte TraceEvent, protobuf encoding would produce:

```
Field 1 (timestamp uint32):  1 tag + 1-5 varint = 2-6 bytes
Field 2 (task_id uint32):    1 tag + 1-3 varint = 2-4 bytes
Field 3 (event_type enum):   1 tag + 1 varint   = 2 bytes
Field 4 (category enum):     1 tag + 1 varint   = 2 bytes
Field 5 (arg1 uint32):       1 tag + 1-5 varint = 2-6 bytes
Field 6 (arg2 uint32):       1 tag + 1-5 varint = 2-6 bytes
                                        TOTAL: 12-26 bytes
```

**Typical case:** ~18-20 bytes (timestamps and arg1 are large values that use more varint bytes).

**Verdict:** Protobuf encoding of individual trace events adds 25-60% overhead vs the current 16-byte binary format. For high-frequency events (thousands per second), this is unacceptable.

**Recommendation:** Keep 16-byte binary events. Use protobuf only for:
- Control messages (start/stop/dump/status/filter)
- Metadata (session info, task names, pod info)
- Event wrappers (transport envelope carrying binary event batches)

This is a **hybrid approach**: protobuf for structure, binary for payload. nanopb's `bytes` type makes this natural — a protobuf message can carry a `bytes` field containing raw 16-byte events.

### 3.6 USB-CDC Bandwidth

ESP32-S3 USB-CDC is Full-Speed USB (12 Mbps, ~1 MB/s theoretical). Practical throughput:

| Scenario | Data Rate | Feasibility |
|----------|-----------|-------------|
| Dump 2048 events | 32 KB one-shot | Easy (32ms) |
| Stream 1000 events/sec | 16 KB/sec + framing ~20 KB/s | Easy (2% bandwidth) |
| Stream 10,000 events/sec | ~200 KB/s | Feasible (20% bandwidth) |
| ESP logs + streaming | ~5-50 KB/s combined | Needs multiplexing |

**Conclusion:** Bandwidth is NOT the bottleneck. The problem is multiplexing logs and trace data on a single byte stream. Frame-wrapping solves this.

### 3.7 Multi-Device Time Synchronization

For ESP-NOW-connected devices without NTP:

**ESPNowMeshClock** ([GitHub](https://github.com/Hemisphere-Project/ESPNowMeshClock)):
- Fully distributed (no master election needed)
- 10-byte sync beacon: 3-byte magic + 7-byte timestamp
- Broadcast every ~1000ms with ±10% jitter to avoid collisions
- Forward-only monotonic updates (clocks never go backward)
- Slew-rate smoothing (alpha=0.25) for gradual convergence
- Microsecond-level accuracy achievable

**FTSP (Flooding Time Synchronization Protocol):**
- [Research paper](https://dl.acm.org/doi/10.1145/3626641.3626683) evaluated with ESP-NOW
- Linear regression for clock skew compensation
- Sub-millisecond accuracy

**Our approach:** We already have ESP-NOW beacons (discovery). Add a timestamp field to existing beacons. Each pod computes offset to master. Include offset in trace metadata so host can align timestamps.

---

## 4. New Architecture Design

### 4.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│  FIRMWARE (ESP32-S3)                                                │
│                                                                     │
│  ┌──────────────────┐   ┌──────────────────┐                       │
│  │ Application Code │   │ FreeRTOS Hooks   │                       │
│  │ TRACE_SCOPE()    │   │ task_switched_in │                       │
│  └────────┬─────────┘   └────────┬─────────┘                       │
│           │                      │                                   │
│           ▼                      ▼                                   │
│  ┌───────────────────────────────────────────┐                      │
│  │          TraceRecorder (singleton)         │                      │
│  │  record() / recordFromIsr()               │                      │
│  └──────┬────────────────────────┬───────────┘                      │
│         │                        │                                   │
│         ▼                        ▼                                   │
│  ┌──────────────┐    ┌────────────────────┐                         │
│  │  Ring Buffer  │    │  StreamingSink     │  ◄── NEW               │
│  │  (post-mortem)│    │  (real-time)       │                        │
│  │  32KB, 2048   │    │  category filter   │                        │
│  │  events       │    │  backpressure      │                        │
│  └──────┬────────┘    └────────┬───────────┘                        │
│         │                      │                                     │
│         │    ┌─────────────────┘                                     │
│         │    │                                                       │
│         ▼    ▼                                                       │
│  ┌──────────────────────────────────────┐                           │
│  │     TraceCommandHandler (protobuf!)  │  ◄── MIGRATED            │
│  │  - DumpRequest/Response (protobuf)   │                           │
│  │  - StreamConfig (protobuf)           │                           │
│  │  - Binary events in bytes field      │                           │
│  └──────────────┬───────────────────────┘                           │
│                 │                                                     │
│                 ▼                                                     │
│  ┌──────────────────────────────────────┐                           │
│  │     Transport Multiplexer            │  ◄── NEW                  │
│  │  MsgType routing:                    │                           │
│  │    0x10-0x1F → trace frames          │                           │
│  │    0x20-0x3F → config frames         │                           │
│  │    0x01-0x05 → OTA frames            │                           │
│  │    0x40      → log-forwarded frames  │  ◄── NEW                  │
│  └──────────────┬───────────────────────┘                           │
│                 │                                                     │
│                 ▼                                                     │
│  ┌──────────────────────────────────────┐                           │
│  │     USB-CDC / TCP / BLE              │                           │
│  │  (frame protocol, all multiplexed)   │                           │
│  └──────────────────────────────────────┘                           │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│  HOST (domes-cli)                                                    │
│                                                                     │
│  ┌──────────────────────────────────────┐                           │
│  │     Frame Decoder / Demultiplexer    │                           │
│  │  Route by MsgType:                   │                           │
│  │    0x10-0x1F → trace handler         │                           │
│  │    0x40      → log display           │  ◄── NEW                  │
│  │    0x20-0x3F → config handler        │                           │
│  └──────────┬───────────────────────────┘                           │
│             │                                                        │
│     ┌───────┴─────────────────────┐                                 │
│     ▼                             ▼                                  │
│  ┌──────────────┐    ┌────────────────────┐                         │
│  │ Trace Dump   │    │ Trace Stream       │  ◄── NEW                │
│  │ (post-mortem)│    │ (real-time TUI)    │                         │
│  │ → JSON file  │    │ → live display     │                         │
│  │ → Perfetto   │    │ → recording        │                         │
│  └──────────────┘    └────────────────────┘                         │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 Design Principles

1. **Binary events stay binary** — 16-byte TraceEvent is not protobuf-encoded
2. **Control plane goes protobuf** — All metadata, commands, config use protobuf
3. **Frame multiplexing** — Logs become frames, eliminating log/trace conflict
4. **Dual-mode by default** — Ring buffer always captures; streaming is opt-in
5. **Category filtering on device** — Reduce wire traffic at source
6. **Pod-aware from the ground up** — Pod ID in session metadata and events

---

## 5. Protocol Design (trace.proto)

### 5.1 Current vs Proposed

| Component | Current | Proposed |
|-----------|---------|----------|
| Control commands | Frame MsgType byte only | Protobuf request/response messages |
| Metadata | Binary packed struct (17 bytes) | Protobuf `TraceSessionInfo` message |
| Data chunks | Binary header (6 bytes) + events | Protobuf wrapper with `bytes` payload |
| Events | 16-byte binary (GOOD) | 16-byte binary (UNCHANGED) |
| Status | Binary packed (14 bytes) | Protobuf `TraceStatusResponse` |
| Filtering | None (dump all) | Protobuf `StreamConfig` with category bitmask |
| Log forwarding | Not possible | New MSG_TYPE_LOG with protobuf wrapper |

### 5.2 Proposed trace.proto Additions

```protobuf
// ============================================================
// NEW: Trace protocol v2 — protobuf control plane
// ============================================================

// --- Session & Metadata ---

// Info about the trace session (replaces binary TraceMetadata)
message TraceSessionInfo {
    uint32 pod_id = 1;              // Pod identity (from NVS)
    uint32 event_count = 2;         // Events in this dump
    uint32 dropped_count = 3;       // Events dropped (buffer full)
    uint64 start_timestamp_us = 4;  // First event timestamp
    uint64 end_timestamp_us = 5;    // Last event timestamp
    repeated TaskInfo tasks = 6;    // Registered task names
    uint32 buffer_size_bytes = 7;   // Ring buffer size
    uint32 firmware_version = 8;    // For compatibility
    int64 clock_offset_us = 9;      // Offset from master clock (multi-pod sync)
}

// Task name mapping (replaces binary TraceTaskEntry)
message TaskInfo {
    uint32 task_id = 1;
    string name = 2;       // Max 16 chars (matches kMaxTaskNameLength)
}

// --- Data Transfer ---

// Chunk of binary trace events (replaces binary TraceDataHeader + raw events)
message TraceDataChunk {
    uint32 offset = 1;      // Event offset in dump (0-based)
    uint32 count = 2;       // Number of 16-byte events in data
    bytes events = 3;       // Raw binary: count * 16 bytes of TraceEvent[]
}

// End-of-dump marker (replaces binary TraceDumpEnd)
message TraceDumpComplete {
    uint32 total_events = 1;
    uint32 checksum = 2;     // Simple byte-sum checksum of all event data
}

// --- Streaming Control (NEW) ---

// Configure real-time streaming (host -> device)
message StreamConfig {
    bool enable = 1;          // true = start streaming, false = stop
    uint32 category_mask = 2; // Bitmask of categories to stream (0 = all)
    uint32 max_rate_hz = 3;   // Max events/sec (0 = unlimited)
    bool include_kernel = 4;  // Include task switch/ISR events (noisy)
}

// Streamed event batch (device -> host, sent periodically)
message StreamBatch {
    uint32 sequence = 1;     // Monotonic sequence number (detect gaps)
    uint32 dropped = 2;      // Events dropped since last batch (overflow)
    bytes events = 3;        // Raw binary: N * 16 bytes of TraceEvent[]
}

// --- Status (replaces binary TraceStatusResponse) ---

message TraceStatusRequest {
    // Empty — just a request
}

message TraceStatusResponse {
    bool initialized = 1;
    bool enabled = 2;          // Recording to ring buffer
    bool streaming = 3;        // Real-time streaming active
    uint32 event_count = 4;    // Events in buffer
    uint32 dropped_count = 5;  // Total drops since last clear
    uint32 buffer_size = 6;    // Buffer capacity in bytes
    uint32 stream_category_mask = 7; // Active stream filter
}

// --- Log Forwarding (NEW) ---

// Forwarded ESP log line (device -> host)
// Replaces direct serial text output during trace operations
message LogEntry {
    uint32 timestamp_us = 1;  // When the log was generated
    uint32 level = 2;         // ESP_LOG_ERROR=1..ESP_LOG_VERBOSE=5
    string tag = 3;           // ESP_LOG tag
    string message = 4;       // Log message text
}
```

### 5.3 Message Type Allocation

```
┌────────────────────────────────────────────────┐
│  Frame MsgType byte allocation:                │
│                                                │
│  0x01-0x05  OTA protocol (existing)            │
│  0x10-0x1F  Trace protocol:                    │
│     0x10    TRACE_START                        │
│     0x11    TRACE_STOP                         │
│     0x12    TRACE_DUMP_REQUEST                 │
│     0x13    TRACE_DATA         (protobuf now)  │
│     0x14    TRACE_DUMP_END     (protobuf now)  │
│     0x15    TRACE_CLEAR                        │
│     0x16    TRACE_STATUS_REQ   (protobuf now)  │
│     0x17    TRACE_STATUS_RESP  (protobuf now)  │
│     0x18    TRACE_STREAM_CFG   (NEW)           │
│     0x19    TRACE_STREAM_DATA  (NEW)           │
│  0x20-0x3F  Config protocol (existing)         │
│  0x40       LOG_FORWARD        (NEW)           │
│                                                │
└────────────────────────────────────────────────┘
```

### 5.4 Protobuf Encoding Size Estimates

| Message | Binary (current) | Protobuf (proposed) | Delta |
|---------|-------------------|---------------------|-------|
| StatusResponse | 14 bytes | ~15-20 bytes | +30% |
| Metadata (4 tasks) | 17 + 72 = 89 bytes | ~60-80 bytes | -10% to +10% |
| DataChunk (8 events) | 6 + 128 = 134 bytes | 8 + 128 = ~136 bytes | +1% |
| DumpEnd | 8 bytes | ~10 bytes | +25% |
| StreamConfig | N/A (new) | ~8-12 bytes | N/A |
| StreamBatch (8 events) | N/A (new) | ~10 + 128 = ~138 bytes | N/A |

**Key insight:** For data chunks, the overhead is negligible because the events themselves (the `bytes` field) are raw binary. The protobuf wrapper only adds ~2-8 bytes for field tags and the bytes-length prefix.

---

## 6. Transport Separation

### 6.1 The Problem

Currently, ESP-IDF's `ESP_LOGx` macros write directly to the USB-CDC serial port as UTF-8 text. The frame protocol also writes to the same port. When both are active:

```
ESP_LOGI output:  "I (1234) wifi: Connected to AP\r\n"
Frame protocol:    [0xAA][0x55][len][type][payload][CRC]
```

These byte streams collide. The frame decoder ignores non-0xAA bytes (noise resilience), but if a log byte happens to be 0xAA 0x55, it triggers a false frame start.

Current workaround: **Suppress ALL logs during trace dump.** This is fragile and lossy.

### 6.2 Recommended Solution: Frame-Wrapped Log Forwarding

**Approach:** Redirect ESP logs through the frame protocol as `LOG_FORWARD` (0x40) messages.

```
┌──────────────────────────────────────────────────────┐
│  BEFORE (collision-prone):                            │
│                                                      │
│  ESP_LOGI ──► vprintf ──► USB-CDC ◄── Frame protocol │
│                              ▲                        │
│                              │ COLLISION!              │
│                                                      │
│  AFTER (multiplexed, clean):                          │
│                                                      │
│  ESP_LOGI ──► custom_vprintf ──┐                     │
│                                ▼                      │
│                     ┌──────────────────┐              │
│                     │ Log → Frame      │              │
│                     │ MsgType = 0x40   │              │
│                     │ payload = proto  │              │
│                     │ LogEntry msg     │              │
│                     └────────┬─────────┘              │
│                              │                        │
│  Trace frames ───────────────┤                        │
│  Config frames ──────────────┤                        │
│  OTA frames ─────────────────┤                        │
│                              ▼                        │
│                    ┌──────────────────┐               │
│                    │ USB-CDC Serial   │               │
│                    │ (all framed!)    │               │
│                    └──────────────────┘               │
└──────────────────────────────────────────────────────┘
```

**Implementation:**

1. **On device:** Install custom vprintf via `esp_log_set_vprintf(domes_log_vprintf)`:
   ```cpp
   int domes_log_vprintf(const char* fmt, va_list args) {
       // Format the message
       char buf[128];
       int len = vsnprintf(buf, sizeof(buf), fmt, args);

       // If log forwarding is active, wrap in frame
       if (g_logForwardEnabled) {
           // Encode as LogEntry protobuf
           domes_trace_LogEntry entry = domes_trace_LogEntry_init_zero;
           entry.timestamp_us = (uint32_t)esp_timer_get_time();
           entry.level = currentLogLevel;
           strncpy(entry.tag, currentTag, sizeof(entry.tag));
           strncpy(entry.message, buf, sizeof(entry.message));

           // Encode and send as frame
           uint8_t pb_buf[256];
           pb_ostream_t stream = pb_ostream_from_buffer(pb_buf, sizeof(pb_buf));
           pb_encode(&stream, domes_trace_LogEntry_fields, &entry);
           sendFrame(0x40, pb_buf, stream.bytes_written);
       } else {
           // Direct UART output (boot/fallback)
           uart_write_bytes(UART_NUM_0, buf, len);
       }
       return len;
   }
   ```

2. **On host (domes-cli):** The frame demuxer routes 0x40 frames to a log display handler that decodes the protobuf and prints with color/formatting.

**When to enable log forwarding:**
- **Off at boot** — early boot logs go directly to UART (needed for debugging init)
- **On after USB-CDC init** — once the frame protocol is active, switch to forwarded logs
- **Always on during trace operations** — no more log suppression needed

### 6.3 Alternatives Considered

| Approach | Pros | Cons | Verdict |
|----------|------|------|---------|
| Frame-wrapped logs | Clean, no collision, works on all transports | Adds ~30 bytes overhead per log line | **RECOMMENDED** |
| Separate USB endpoints | True hardware separation | ESP32-S3 USB peripheral config complexity, breaks existing tools | Rejected |
| Log-as-trace-events | Unified data model | Pollutes trace buffer, logs are large/variable-length | Rejected |
| Keep suppressing | No code changes | Lossy, fragile, no real-time | Rejected |

### 6.4 Log Forwarding Toggle

The host can control log forwarding:

```
domes-cli log forward --on         # Enable frame-wrapped logs
domes-cli log forward --off        # Disable (revert to direct UART)
domes-cli log forward --level warn # Only forward WARN and above
```

This could use an existing config MsgType or a new one. Since it's a simple toggle, it could be a protobuf message on the config channel.

---

## 7. Dual-Mode Operation

### 7.1 Mode Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  TraceRecorder                                                   │
│                                                                 │
│  record(event) ─────┬──────────────────────────┐               │
│                     │                          │               │
│                     ▼                          ▼               │
│         ┌──────────────────┐      ┌────────────────────┐       │
│         │   Ring Buffer    │      │   StreamingSink     │       │
│         │   (always on)    │      │   (opt-in)          │       │
│         │                  │      │                     │       │
│         │ - Circular       │      │ - Category filter   │       │
│         │ - 32KB fixed     │      │ - Rate limiter      │       │
│         │ - Post-mortem    │      │ - Batch accumulator │       │
│         │ - Full history   │      │ - Backpressure      │       │
│         └──────┬───────────┘      └──────────┬──────────┘       │
│                │                             │                  │
│  (DUMP cmd)    │              (periodic)     │                  │
│                ▼                             ▼                  │
│    ┌─────────────────┐         ┌────────────────────┐          │
│    │ Dump Handler    │         │ Stream Handler     │          │
│    │ TraceSessionInfo│         │ StreamBatch msg    │          │
│    │ + TraceDataChunk│         │ every ~50ms        │          │
│    │ + DumpComplete  │         │ if events pending  │          │
│    └─────────────────┘         └────────────────────┘          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 Post-Mortem Mode (Improved)

Same as current, but with protobuf control messages:

```
Host                                Device
  │                                   │
  │──── TRACE_DUMP_REQUEST ──────────►│
  │     (empty, or with options)      │
  │                                   │
  │◄─── TRACE_DATA ──────────────────│  TraceSessionInfo (protobuf)
  │     MsgType=0x13                  │
  │                                   │
  │◄─── TRACE_DATA ──────────────────│  TraceDataChunk { offset=0, count=8,
  │     MsgType=0x13                  │    events=<128 bytes binary> }
  │                                   │
  │◄─── TRACE_DATA ──────────────────│  TraceDataChunk { offset=8, count=8, ... }
  │     ...                           │
  │                                   │
  │◄─── TRACE_DUMP_END ─────────────│  TraceDumpComplete { total, checksum }
  │     MsgType=0x14                  │
  │                                   │
```

**Improvements over current:**
- No log suppression needed (logs are frame-wrapped)
- Protobuf metadata includes pod_id, clock_offset, firmware_version
- Host can request filtered dump (category mask in request)

### 7.3 Real-Time Streaming Mode (New)

```
Host                                Device
  │                                   │
  │──── TRACE_STREAM_CFG ───────────►│  StreamConfig {
  │     MsgType=0x18                  │    enable=true,
  │                                   │    category_mask=0x0090, // game+touch
  │                                   │    max_rate_hz=500,
  │                                   │    include_kernel=false
  │                                   │  }
  │                                   │
  │◄─── TRACE_STATUS_RESP ──────────│  (ACK with streaming=true)
  │                                   │
  │                                   │  ┌───────────────────────┐
  │                                   │  │ StreamingSink active: │
  │                                   │  │ - filters by mask     │
  │                                   │  │ - accumulates events  │
  │                                   │  │ - sends every 50ms    │
  │                                   │  └───────────────────────┘
  │                                   │
  │◄─── TRACE_STREAM_DATA ─────────│  StreamBatch { seq=1, dropped=0,
  │     MsgType=0x19                  │    events=<N*16 bytes> }
  │                                   │
  │◄─── TRACE_STREAM_DATA ─────────│  StreamBatch { seq=2, ... }
  │     ...                           │
  │                                   │
  │──── TRACE_STREAM_CFG ───────────►│  StreamConfig { enable=false }
  │     MsgType=0x18                  │
  │                                   │
  │◄─── TRACE_STATUS_RESP ──────────│  (ACK with streaming=false)
  │                                   │
```

### 7.4 StreamingSink Implementation

```cpp
class StreamingSink {
public:
    void configure(const StreamConfig& config);
    bool isActive() const { return enabled_; }

    // Called by TraceRecorder::record() — MUST be fast
    void onEvent(const TraceEvent& event) {
        // 1. Category filter (single bitmask check)
        if (categoryMask_ && !(categoryMask_ & (1 << event.category()))) {
            return;
        }

        // 2. Kernel filter
        if (!includeKernel_ && event.eventType < 0x20) {
            return;
        }

        // 3. Rate limiter (token bucket)
        if (maxRateHz_ > 0 && !rateLimiter_.tryConsume()) {
            droppedCount_++;
            return;
        }

        // 4. Accumulate in batch buffer (fixed-size, no alloc)
        if (batchCount_ < kMaxBatchSize) {
            batchBuffer_[batchCount_++] = event;
        } else {
            droppedCount_++;
        }
    }

    // Called periodically from a low-priority task (~50ms interval)
    // Sends accumulated batch as protobuf StreamBatch
    void flush();

private:
    bool enabled_ = false;
    uint32_t categoryMask_ = 0;
    uint32_t maxRateHz_ = 0;
    bool includeKernel_ = false;
    uint32_t droppedCount_ = 0;
    uint32_t sequence_ = 0;

    static constexpr size_t kMaxBatchSize = 64;  // 1KB batch
    TraceEvent batchBuffer_[kMaxBatchSize];
    size_t batchCount_ = 0;

    TokenBucket rateLimiter_;
};
```

### 7.5 Coexistence

Both modes operate simultaneously and independently:

| Aspect | Post-Mortem (Ring Buffer) | Real-Time (Streaming Sink) |
|--------|--------------------------|---------------------------|
| Always active? | Yes | Only when configured |
| Filters | None (captures all) | Category mask, rate limit |
| Storage | Ring buffer (overwrites old) | Batch buffer (flushes periodically) |
| Triggered by | DUMP command | STREAM_CFG command |
| Output | Complete historical trace | Live event stream |
| Overhead | Negligible (existing) | One bitmask check + optional copy |

The ring buffer continues to capture ALL events regardless of streaming state. This ensures post-mortem analysis always has complete data, even if streaming is filtered or rate-limited.

---

## 8. Host Integration (domes-cli)

### 8.1 Command Structure

```
domes-cli trace <subcommand>

Subcommands:
  start              Enable trace recording on device
  stop               Disable trace recording
  clear              Clear trace buffer
  status             Show trace system status

  dump [OPTIONS]     Dump trace buffer to file
    -o, --output     Output file (default: trace.json)
    -f, --format     Output format: perfetto|csv|raw (default: perfetto)
    --names          Span name mapping file (auto-discovers trace_names.json)
    --category       Filter by category (comma-separated)

  stream [OPTIONS]   Real-time trace streaming (NEW)
    --category       Categories to stream (comma-separated, default: all)
    --rate           Max events/sec (default: unlimited)
    --no-kernel      Exclude kernel (task switch/ISR) events
    --output         Record stream to file (default: display only)
    --duration       Stream for N seconds then stop
    --tui            Interactive TUI mode (default when no --output)
```

### 8.2 Multi-Device Trace Commands

```bash
# Dump from all connected devices, merge into single trace
domes-cli --all trace dump -o trace.json

# Stream from specific devices
domes-cli --target pod1 --target pod2 trace stream --category game,touch

# Dump with cross-pod timestamp alignment
domes-cli --all trace dump --align-timestamps -o merged-trace.json
```

### 8.3 Perfetto JSON Export (Enhanced)

Current export uses flat Chrome JSON. Enhanced export should include:

```json
{
  "traceEvents": [...],
  "metadata": {
    "domes_version": "1.0.0",
    "pods": [
      {
        "pod_id": 1,
        "pid": 1,
        "clock_offset_us": 0,
        "firmware_version": "v2.1.0"
      },
      {
        "pod_id": 2,
        "pid": 2,
        "clock_offset_us": -1234,
        "firmware_version": "v2.1.0"
      }
    ]
  }
}
```

Multi-pod events use `pid` = pod_id for separation in Perfetto UI:

```json
{"name": "Game.Tick", "ph": "B", "pid": 1, "tid": 3, "ts": 1000000, "cat": "game"}
{"name": "Game.Tick", "ph": "B", "pid": 2, "tid": 3, "ts": 1001234, "cat": "game"}
```

Perfetto displays these as separate "processes" (pods), with aligned timestamps.

### 8.4 Live TUI (New)

For `domes-cli trace stream --tui`:

```
┌─ DOMES Trace Stream ──────────────────────────────────────┐
│ Pod-1 ● streaming | Pod-2 ● streaming | Rate: 47 evt/s    │
├────────────────────────────────────────────────────────────┤
│ Timestamp  Pod  Task       Category  Event                 │
│ 1.234567   P1   GameTask   game      Game.Tick ──────┐     │
│ 1.234600   P2   GameTask   game      Game.Tick ──┐   │     │
│ 1.234890   P1   GameTask   game      Game.Tick ──┘   │     │
│ 1.235100   P1   TouchTask  touch     Touch.Hit (23ms)│     │
│ 1.235200   P1   GameTask   game      Game.Score ◆    │     │
│ 1.235400   P2   TouchTask  touch     Touch.Miss      │     │
│ 1.235500   P2   GameTask   game      Game.Score ◆    │     │
│ 1.235890   P1   GameTask   game      Game.Tick ──────┘     │
│                                                            │
│ [q]uit  [p]ause  [c]ategory filter  [r]ecord to file      │
└────────────────────────────────────────────────────────────┘
```

This is a stretch goal — initial implementation can be simple text output.

### 8.5 Deprecation of Python Tool

Once domes-cli trace commands are feature-complete:

1. Add deprecation notice to `tools/trace/trace_dump.py`
2. Ensure CLI loads `trace_names.json` (or auto-discovers it)
3. Move `trace_names.json` to `firmware/common/trace/trace_names.json` (closer to source)
4. Eventually auto-generate trace_names.json from `TRACE_ID()` macro invocations

---

## 9. Multi-Pod Correlation

### 9.1 Timestamp Alignment Strategy

ESP32-S3 uses `esp_timer_get_time()` — a monotonic 64-bit microsecond clock from the RTC. Without synchronization, each pod's clock drifts independently (~10 ppm = ~36ms/hour).

**Approach: ESP-NOW beacon timestamp sync** (inspired by ESPNowMeshClock)

```
┌─────────────────────────────────────────────────────────────────┐
│  MULTI-POD TIME SYNC PROTOCOL                                   │
│                                                                 │
│  Pod-1 (master)                    Pod-2 (slave)                │
│  ┌──────────────┐                 ┌──────────────┐              │
│  │ local clock   │                 │ local clock   │              │
│  │ T1 = 1000000  │                 │ T2 = 1000450  │              │
│  └──────┬───────┘                 └──────┬───────┘              │
│         │                                │                       │
│         │  ┌──────────────────────────┐  │                       │
│         └─►│  ESP-NOW Beacon          │  │                       │
│            │  { type: BEACON,         │  │                       │
│            │    timestamp: 1000000,   │◄─┘                       │
│            │    pod_id: 1 }           │                           │
│            └──────────────────────────┘                           │
│                                                                 │
│         Pod-2 calculates:                                        │
│         offset = T1_received - T2_local = 1000000 - 1000450     │
│                = -450 µs                                         │
│                                                                 │
│         → Pod-2's trace metadata includes:                      │
│           clock_offset_us = -450                                │
│                                                                 │
│         Host aligns: T2_aligned = T2_raw + offset               │
│                      1000450 + (-450) = 1000000 ✓               │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Implementation details:**

1. **Discovery beacons already exist** — The ESP-NOW discovery beacon (`MsgHeader`, 11 bytes) already has `timestampUs`. We augment the beacon handler to compute clock offset.

2. **Master selection** — The pod with the lowest pod_id is the time reference. No election needed (pod_ids are pre-assigned).

3. **Offset computation** — On receiving a beacon, the slave computes: `offset = sender_timestamp - local_timestamp_at_receive`. This is smoothed with exponential moving average (alpha=0.25) to handle jitter.

4. **Accuracy** — ESP-NOW frame delivery is ~100-300µs. With smoothing, we achieve ~100µs alignment — sufficient for game-level analysis (reaction times are 100-500ms).

5. **No clock adjustment** — We do NOT adjust the local clock. The offset is metadata-only, applied on the host during trace merging. This avoids disrupting FreeRTOS timers.

### 9.2 Pod ID in Traces

The `TraceEvent` struct is 16 bytes and packed. Adding a pod_id field would break the format. Instead:

- **Pod ID is session-level metadata** — included in `TraceSessionInfo.pod_id`
- **In Perfetto JSON** — each pod gets a different `pid` value
- **For streaming** — each `StreamBatch` implicitly comes from the connected pod

For the host simulation (multi-pod-sim), the existing `sim_trace::currentPodId` mechanism handles pod tagging.

### 9.3 Merged Trace Export

```
domes-cli --all trace dump --align-timestamps -o merged.json
```

Algorithm:
1. Dump each pod's trace buffer (sequentially or in parallel)
2. Each pod reports `TraceSessionInfo` with `clock_offset_us`
3. Host adjusts all timestamps: `aligned_ts = raw_ts + clock_offset_us`
4. Merge events from all pods, sorted by aligned timestamp
5. Each pod's events use `pid = pod_id` in the JSON

Result: A single Perfetto file showing all pods on a shared timeline:

```
  Process 1 (Pod-1)  ═══════════════════════════════════
    GameTask         ▓▓▓░░▓▓▓░░▓▓▓░░
    TouchTask              ▓░░

  Process 2 (Pod-2)  ═══════════════════════════════════
    GameTask           ▓▓▓░░▓▓▓░░▓▓▓░░
    TouchTask                    ▓░░

  Timeline (µs)      0    100  200  300  400  500  600
```

---

## 10. Migration Path

### 10.1 Phased Approach

```
Phase 1: Protobuf Migration (control plane only)
├── Replace binary metadata/status/chunk structs with protobuf
├── Keep 16-byte binary events (in protobuf bytes field)
├── Update domes-cli trace.rs to decode protobuf
├── Update Python tool for compatibility (or deprecate)
├── Add pod_id to TraceSessionInfo
├── Add span name loading to domes-cli
└── Tests: unit tests + hardware verification

Phase 2: Log Forwarding
├── Implement custom vprintf with frame-wrapped log output
├── Add MSG_TYPE_LOG_FORWARD (0x40)
├── Add LogEntry protobuf message
├── Update domes-cli to demux and display forwarded logs
├── Remove log suppression from trace dump
└── Tests: verify trace dump works with logging active

Phase 3: Real-Time Streaming
├── Implement StreamingSink in TraceRecorder
├── Add StreamConfig/StreamBatch protobuf messages
├── Add TRACE_STREAM_CFG (0x18) and TRACE_STREAM_DATA (0x19) message types
├── Implement category filtering and rate limiting
├── Add `domes-cli trace stream` command
├── Simple text output first (TUI later)
└── Tests: streaming unit tests + hardware verification

Phase 4: Multi-Pod Correlation
├── Add clock_offset_us computation in ESP-NOW beacon handler
├── Include clock_offset_us in TraceSessionInfo
├── Implement merged trace export in domes-cli
├── Add --align-timestamps flag to dump command
└── Tests: two-device trace with timestamp verification

Phase 5: Polish (Stretch)
├── TUI for live streaming
├── Auto-generate trace_names.json from TRACE_ID() macros
├── Add trace category to `domes-cli trace stream --tui` filter
└── Deprecate and remove Python trace_dump.py
```

### 10.2 Backward Compatibility

During migration, the firmware should support BOTH old and new wire formats:

- **Phase 1:** Old binary metadata detection (check payload size < protobuf minimum) → fall back to binary parsing
- **Phase 2+:** New firmware always uses protobuf; old CLI gets a "firmware too new" error suggesting update

The domes-cli should detect firmware version from `TraceSessionInfo.firmware_version` and adapt.

### 10.3 Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| nanopb encoding adds latency to ISR-context recording | Low | High | Events stay binary; protobuf only for control plane (not ISR path) |
| Log forwarding fills USB-CDC bandwidth | Medium | Medium | Log level filtering; rate limiting; disable during OTA |
| Stream batching adds memory pressure | Low | Medium | Fixed-size batch buffer (1KB); dropped counter |
| Clock sync jitter exceeds game requirements | Low | Low | 100µs accuracy is fine for 100ms+ reaction times |
| Protobuf code size increase | Low | Low | nanopb is ~2-10KB; we already use it for config |

---

## 11. References

1. [ESP-IDF Application Level Tracing Library](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/app_trace.html) — ESP-IDF's built-in tracing, transport separation concepts
2. [defmt - Efficient Rust embedded logging](https://ferrous-systems.com/blog/defmt/) — Deferred formatting pattern, string compression
3. [Trice - C embedded trace IDs](https://rokath.github.io/trice/) — ID-based logging, sub-microsecond overhead
4. [Pigweed pw_trace](https://pigweed.dev/pw_trace/) — Google's embedded trace framework, sink architecture
5. [Pigweed pw_trace_tokenized](https://pigweed.dev/pw_trace_tokenized/) — Tokenized backend with ring buffer + sinks
6. [Common Trace Format v2](https://diamon.org/ctf/) — Linux kernel trace format, per-stream design
7. [nanopb benchmark](https://jpa.kapsi.fi/nanopb/benchmark/) — Embedded protobuf encoding overhead
8. [ESPNowMeshClock](https://github.com/Hemisphere-Project/ESPNowMeshClock) — Distributed time sync over ESP-NOW
9. [FTSP on ESP-NOW WSN](https://dl.acm.org/doi/10.1145/3626641.3626683) — Flooding time sync protocol research
10. [ESP32 System Time](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html) — esp_timer microsecond clock, 10ppm drift
11. [Protobuf Encoding Spec](https://protobuf.dev/programming-guides/encoding/) — Wire format details for overhead analysis

---

## Appendix A: Protobuf vs Binary Size Comparison

Detailed analysis of encoding overhead for the TraceEvent message:

```
Current binary TraceEvent (16 bytes, fixed):
  [timestamp:u32][taskId:u16][eventType:u8][flags:u8][arg1:u32][arg2:u32]

Protobuf TraceEvent encoding (variable, 12-26 bytes):
  Example: timestamp=5000000, taskId=3, eventType=0x20, category=7, arg1=19912354, arg2=0

  Field 1 (timestamp=5000000):   tag=0x08, varint=0xC0843D    → 4 bytes
  Field 2 (taskId=3):            tag=0x10, varint=0x03         → 2 bytes
  Field 3 (eventType=0x20):      tag=0x18, varint=0x20         → 2 bytes
  Field 4 (category=7):          tag=0x20, varint=0x07         → 2 bytes
  Field 5 (arg1=19912354):       tag=0x28, varint=0xA2C5C009   → 5 bytes
  Field 6 (arg2=0):              (omitted, default value)      → 0 bytes
                                                        TOTAL: 15 bytes

  With large values (timestamp=4294967295, arg1=4294967295, arg2=4294967295):
  Field 1: 6 bytes, Field 2: 4 bytes, Field 3: 2, Field 4: 2, Field 5: 6, Field 6: 6
                                                        TOTAL: 26 bytes
```

**Verdict:** 16-byte binary is superior for dense event storage. Use protobuf `bytes` field to carry batches of raw events.

## Appendix B: ESP-NOW Beacon Time Sync Pseudocode

```cpp
// In espNowService.cpp, on beacon receive:

void EspNowService::onBeaconReceived(const MsgHeader& header) {
    uint32_t localNow = (uint32_t)esp_timer_get_time();
    uint32_t senderTimestamp = header.timestampUs;

    // Only sync to lower pod_id (deterministic master)
    uint8_t senderPodId = extractPodId(header.senderMac);
    if (senderPodId >= myPodId_) {
        return;  // We are the reference, or equal
    }

    // Compute offset (signed, handles wraparound)
    int64_t rawOffset = (int64_t)senderTimestamp - (int64_t)localNow;

    // Exponential moving average for smoothing
    constexpr float kAlpha = 0.25f;
    if (!hasOffset_) {
        clockOffset_ = rawOffset;
        hasOffset_ = true;
    } else {
        clockOffset_ = (int64_t)(kAlpha * rawOffset + (1.0f - kAlpha) * clockOffset_);
    }
}

// When trace dump is requested, include offset in session info:
// session_info.clock_offset_us = espNowService.getClockOffset();
```

## Appendix C: Frame Demuxer Architecture (Host Side)

```rust
// In domes-cli, unified frame handler:

fn process_frame(frame: Frame) {
    match frame.msg_type {
        // Trace protocol
        0x10..=0x19 => trace_handler.on_frame(frame),

        // Config protocol
        0x20..=0x3F => config_handler.on_frame(frame),

        // OTA protocol
        0x01..=0x05 => ota_handler.on_frame(frame),

        // Log forwarding (NEW)
        0x40 => {
            let log_entry = LogEntry::decode(&frame.payload)?;
            display_log(log_entry);
        }

        _ => warn!("Unknown frame type: 0x{:02X}", frame.msg_type),
    }
}
```
