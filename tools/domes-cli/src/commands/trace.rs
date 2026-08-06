//! Trace/perfetto commands
//!
//! Uses protobuf-encoded messages for all control/metadata (prost).
//! TraceEvent data is 16-byte binary carried in protobuf 'bytes' fields.

use crate::proto::trace::{
    AckResponse, Category, EventType, MsgType as TraceMsgType, Status as TraceStatus, StreamBatch,
    TraceDataChunk, TraceDumpComplete, TraceSessionInfo, TraceStatusResponse,
};
use crate::transport::frame::Frame;
use crate::transport::Transport;
use anyhow::{Context, Result};
use prost::Message;
use sha2::{Digest, Sha256};
use std::collections::HashMap;
use std::fs::File;
use std::io::Write;
use std::path::Path;

/// Compact trace event (16 bytes, binary)
#[derive(Debug, Clone, Copy)]
struct TraceEvent {
    timestamp: u32,
    task_id: u16,
    event_type: u8,
    flags: u8,
    arg1: u32,
    arg2: u32,
}

const TRACE_EVENT_SIZE: usize = 16;

fn decode_trace_events(event_bytes: &[u8]) -> Result<Vec<TraceEvent>> {
    if !event_bytes.len().is_multiple_of(TRACE_EVENT_SIZE) {
        anyhow::bail!(
            "Trace event payload has {} bytes; expected a multiple of {}",
            event_bytes.len(),
            TRACE_EVENT_SIZE
        );
    }

    Ok(event_bytes
        .chunks_exact(TRACE_EVENT_SIZE)
        .map(|bytes| TraceEvent {
            timestamp: u32::from_le_bytes(bytes[0..4].try_into().expect("fixed trace timestamp")),
            task_id: u16::from_le_bytes(bytes[4..6].try_into().expect("fixed trace task ID")),
            event_type: bytes[6],
            flags: bytes[7],
            arg1: u32::from_le_bytes(bytes[8..12].try_into().expect("fixed trace argument")),
            arg2: u32::from_le_bytes(bytes[12..16].try_into().expect("fixed trace argument")),
        })
        .collect())
}

#[derive(Debug, Default)]
struct TraceDumpIntegrity {
    next_offset: u32,
    checksum: u32,
}

impl TraceDumpIntegrity {
    fn accept_chunk(&mut self, chunk: &TraceDataChunk) -> Result<Vec<TraceEvent>> {
        if chunk.offset != self.next_offset {
            anyhow::bail!(
                "Trace chunk offset mismatch: expected {}, got {}",
                self.next_offset,
                chunk.offset
            );
        }

        let events = decode_trace_events(&chunk.events)?;
        let event_count =
            u32::try_from(events.len()).context("Trace chunk event count overflow")?;
        if chunk.count != event_count {
            anyhow::bail!(
                "Trace chunk count mismatch at offset {}: declared {}, decoded {}",
                chunk.offset,
                chunk.count,
                event_count
            );
        }

        self.next_offset = self
            .next_offset
            .checked_add(event_count)
            .context("Trace event offset overflow")?;
        for byte in &chunk.events {
            self.checksum = self.checksum.wrapping_add(u32::from(*byte));
        }

        Ok(events)
    }

    fn finish(&self, complete: &TraceDumpComplete, session_event_count: u32) -> Result<()> {
        if complete.total_events != self.next_offset {
            anyhow::bail!(
                "Trace dump total mismatch: end marker declares {}, received {}",
                complete.total_events,
                self.next_offset
            );
        }
        if session_event_count != self.next_offset {
            anyhow::bail!(
                "Trace dump session mismatch: session declared {}, received {}",
                session_event_count,
                self.next_offset
            );
        }
        if complete.checksum != self.checksum {
            anyhow::bail!(
                "Trace dump checksum mismatch: expected 0x{:08X}, calculated 0x{:08X}",
                complete.checksum,
                self.checksum
            );
        }

        Ok(())
    }
}

#[derive(Debug, Default)]
struct StreamSequenceTracker {
    next_sequence: u32,
}

impl StreamSequenceTracker {
    fn accept(&mut self, sequence: u32) -> Result<()> {
        if sequence != self.next_sequence {
            anyhow::bail!(
                "Trace stream sequence gap: expected {}, got {}",
                self.next_sequence,
                sequence
            );
        }

        self.next_sequence = sequence.wrapping_add(1);
        Ok(())
    }
}

/// Trace status information
#[derive(Debug)]
pub struct TraceStatusInfo {
    pub initialized: bool,
    pub enabled: bool,
    pub streaming: bool,
    pub event_count: u32,
    pub dropped_count: u32,
    pub discontinuity_count: u32,
    pub buffer_size: u32,
}

/// Helper to decode a protobuf AckResponse and check status
fn decode_ack(payload: &[u8]) -> Result<TraceStatus> {
    let ack = AckResponse::decode(payload).context("Failed to decode AckResponse")?;
    TraceStatus::try_from(ack.status).map_err(|_| anyhow::anyhow!("Unknown status: {}", ack.status))
}

/// Start tracing
pub fn trace_start(transport: &mut dyn Transport) -> Result<()> {
    let frame = transport
        .send_command(TraceMsgType::Start.as_u8(), &[])
        .context("Failed to send trace start command")?;

    if frame.msg_type != TraceMsgType::Ack.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected ACK 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Ack.as_u8()
        );
    }

    let status = decode_ack(&frame.payload)?;
    match status {
        TraceStatus::Ok | TraceStatus::AlreadyOn => Ok(()),
        TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
        _ => anyhow::bail!("Trace start failed: {}", status),
    }
}

/// Stop tracing
pub fn trace_stop(transport: &mut dyn Transport) -> Result<()> {
    let frame = transport
        .send_command(TraceMsgType::Stop.as_u8(), &[])
        .context("Failed to send trace stop command")?;

    if frame.msg_type != TraceMsgType::Ack.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected ACK 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Ack.as_u8()
        );
    }

    let status = decode_ack(&frame.payload)?;
    match status {
        TraceStatus::Ok | TraceStatus::AlreadyOff => Ok(()),
        TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
        _ => anyhow::bail!("Trace stop failed: {}", status),
    }
}

/// Clear trace buffer
pub fn trace_clear(transport: &mut dyn Transport) -> Result<()> {
    let frame = transport
        .send_command(TraceMsgType::Clear.as_u8(), &[])
        .context("Failed to send trace clear command")?;

    if frame.msg_type != TraceMsgType::Ack.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected ACK 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Ack.as_u8()
        );
    }

    let status = decode_ack(&frame.payload)?;
    match status {
        TraceStatus::Ok => Ok(()),
        TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
        _ => anyhow::bail!("Trace clear failed: {}", status),
    }
}

/// Get trace status
pub fn trace_status(transport: &mut dyn Transport) -> Result<TraceStatusInfo> {
    let frame = transport
        .send_command(TraceMsgType::StatusReq.as_u8(), &[])
        .context("Failed to send trace status command")?;

    // Check for ACK with error first
    if frame.msg_type == TraceMsgType::Ack.as_u8() {
        let status = decode_ack(&frame.payload)?;
        if status == TraceStatus::NotInit {
            anyhow::bail!("Trace system not initialized");
        }
        anyhow::bail!("Trace status failed: {}", status);
    }

    if frame.msg_type != TraceMsgType::StatusResp.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected STATUS_RESP 0x{:02X}",
            frame.msg_type,
            TraceMsgType::StatusResp.as_u8()
        );
    }

    let resp = TraceStatusResponse::decode(frame.payload.as_slice())
        .context("Failed to decode TraceStatusResponse")?;

    Ok(TraceStatusInfo {
        initialized: resp.initialized,
        enabled: resp.enabled,
        streaming: resp.streaming,
        event_count: resp.event_count,
        dropped_count: resp.dropped_count,
        discontinuity_count: resp.discontinuity_count,
        buffer_size: resp.buffer_size,
    })
}

/// Result of a trace dump operation
pub struct DumpResult {
    pub event_count: u32,
    pub dropped_count: u32,
    pub duration_us: u32,
    pub pod_id: u32,
    pub output_path: std::path::PathBuf,
    pub raw_path: std::path::PathBuf,
    pub session_path: std::path::PathBuf,
    pub raw_sha256: String,
}

fn trace_duration_us(start_timestamp_us: u32, end_timestamp_us: u32) -> u32 {
    end_timestamp_us.wrapping_sub(start_timestamp_us)
}

/// Dump traces to a JSON file compatible with Perfetto
pub fn trace_dump(
    transport: &mut dyn Transport,
    output_path: &Path,
    names_path: Option<&Path>,
) -> Result<DumpResult> {
    // Load span names if provided (or auto-discover)
    let span_names = load_span_names(names_path)?;

    let frame = transport
        .send_command(TraceMsgType::Dump.as_u8(), &[])
        .context("Failed to send trace dump command")?;

    // Check for ACK with error (e.g., buffer empty)
    if frame.msg_type == TraceMsgType::Ack.as_u8() {
        let status = decode_ack(&frame.payload)?;
        match status {
            TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
            TraceStatus::BufferEmpty => anyhow::bail!("Trace buffer is empty"),
            _ => anyhow::bail!("Trace dump failed: {}", status),
        }
    }

    // First response should be SESSION_INFO with metadata
    if frame.msg_type != TraceMsgType::SessionInfo.as_u8() {
        anyhow::bail!(
            "Expected SESSION_INFO (0x{:02X}), got: 0x{:02X}",
            TraceMsgType::SessionInfo.as_u8(),
            frame.msg_type
        );
    }

    // Parse session info (protobuf)
    let session_info = TraceSessionInfo::decode(frame.payload.as_slice())
        .context("Failed to decode TraceSessionInfo")?;
    // Build task name lookup
    let task_names: HashMap<u32, String> = session_info
        .tasks
        .iter()
        .map(|t| (t.task_id, t.name.clone()))
        .collect();

    // Collect all events
    let mut raw_events = Vec::with_capacity(session_info.event_count as usize * TRACE_EVENT_SIZE);
    let mut chunks = Vec::new();
    let dump_complete;

    loop {
        let frame = transport
            .receive_frame(5000) // 5 second timeout for trace data
            .context("Failed to receive trace data")?;

        if frame.msg_type == TraceMsgType::Data.as_u8() {
            // Parse data chunk (protobuf)
            let chunk = TraceDataChunk::decode(frame.payload.as_slice())
                .context("Failed to decode TraceDataChunk")?;
            raw_events.extend_from_slice(&chunk.events);
            chunks.push(chunk);
        } else if frame.msg_type == TraceMsgType::End.as_u8() {
            // Parse dump complete (protobuf)
            let complete = TraceDumpComplete::decode(frame.payload.as_slice())
                .context("Failed to decode TraceDumpComplete")?;
            dump_complete = complete;
            break;
        } else {
            anyhow::bail!(
                "Unexpected message type during dump: 0x{:02X}",
                frame.msg_type
            );
        }
    }

    let raw_path = std::path::PathBuf::from(format!("{}.raw", output_path.display()));
    let raw_sha256 = format!("{:x}", Sha256::digest(&raw_events));
    File::create(&raw_path)
        .context("Failed to create raw trace file")?
        .write_all(&raw_events)
        .context("Failed to write raw trace file")?;
    let sha_path = std::path::PathBuf::from(format!("{}.sha256", raw_path.display()));
    File::create(&sha_path)
        .context("Failed to create raw trace hash file")?
        .write_all(format!("{}  {}\n", raw_sha256, raw_path.display()).as_bytes())
        .context("Failed to write raw trace hash file")?;
    let session_path = std::path::PathBuf::from(format!("{}.session.json", raw_path.display()));
    let session_evidence = serde_json::json!({
        "format_version": session_info.trace_event_format_version,
        "pod_id": session_info.pod_id,
        "event_count": session_info.event_count,
        "dropped_count": session_info.dropped_count,
        "discontinuity_count": session_info.discontinuity_count,
        "start_timestamp_us": session_info.start_timestamp_us,
        "end_timestamp_us": session_info.end_timestamp_us,
        "raw_sha256": raw_sha256,
        "tasks": session_info.tasks.iter().map(|task| serde_json::json!({
            "task_id": task.task_id,
            "name": task.name.as_str(),
            "priority": task.priority,
            "core_affinity_mask": task.core_affinity_mask,
        })).collect::<Vec<_>>(),
        "objects": session_info.objects.iter().map(|object| serde_json::json!({
            "object_id": object.object_id,
            "kind": object.kind,
            "name": object.name.as_str(),
        })).collect::<Vec<_>>(),
    });
    File::create(&session_path)
        .context("Failed to create raw trace session file")?
        .write_all(serde_json::to_string_pretty(&session_evidence)?.as_bytes())
        .context("Failed to write raw trace session file")?;

    // Interpret only after the exact received event bytes, hash, and session
    // mappings are durable. Invalid evidence remains available for diagnosis.
    if session_info.trace_event_format_version > 1 {
        anyhow::bail!(
            "Unsupported trace event format version: {}",
            session_info.trace_event_format_version
        );
    }
    if session_info.dropped_count != 0 || session_info.discontinuity_count != 0 {
        anyhow::bail!(
            "Trace evidence is incomplete: dropped={}, discontinuities={}",
            session_info.dropped_count,
            session_info.discontinuity_count
        );
    }
    let mut events: Vec<TraceEvent> = Vec::with_capacity(session_info.event_count as usize);
    let mut integrity = TraceDumpIntegrity::default();
    for chunk in &chunks {
        events.extend(
            integrity
                .accept_chunk(chunk)
                .context("Invalid trace data chunk")?,
        );
    }
    integrity
        .finish(&dump_complete, session_info.event_count)
        .context("Trace dump integrity check failed")?;

    let json = convert_to_perfetto_json(&events, &task_names, &span_names, session_info.pod_id)?;

    // Write to file
    let mut file = File::create(output_path).context("Failed to create output file")?;
    file.write_all(json.as_bytes())
        .context("Failed to write trace file")?;

    Ok(DumpResult {
        event_count: integrity.next_offset,
        dropped_count: session_info.dropped_count,
        duration_us: trace_duration_us(
            session_info.start_timestamp_us,
            session_info.end_timestamp_us,
        ),
        pod_id: session_info.pod_id,
        output_path: output_path.to_path_buf(),
        raw_path,
        session_path,
        raw_sha256,
    })
}

/// Load span name mappings from a JSON file
///
/// Format: { "hash_decimal": "Module.SpanName", ... }
/// Auto-discovers tools/trace/trace_names.json if no path given.
fn load_span_names(names_path: Option<&Path>) -> Result<HashMap<u32, String>> {
    let path = match names_path {
        Some(p) => {
            if !p.exists() {
                anyhow::bail!("Span names file not found: {}", p.display());
            }
            p.to_path_buf()
        }
        None => {
            // Auto-discover trace_names.json relative to the binary
            let candidates = [
                Path::new("tools/trace/trace_names.json").to_path_buf(),
                Path::new("trace_names.json").to_path_buf(),
            ];
            match candidates.iter().find(|p| p.exists()) {
                Some(p) => p.clone(),
                None => return Ok(HashMap::new()), // No names file, use defaults
            }
        }
    };

    let content = std::fs::read_to_string(&path)
        .with_context(|| format!("Failed to read span names from {}", path.display()))?;

    let raw: HashMap<String, String> =
        serde_json::from_str(&content).context("Failed to parse span names JSON")?;

    let mut names = HashMap::new();
    for (key, value) in raw {
        if let Ok(hash) = key.parse::<u32>() {
            names.insert(hash, value);
        }
    }

    Ok(names)
}

/// Convert trace events to Perfetto-compatible Chrome JSON format
fn convert_to_perfetto_json(
    events: &[TraceEvent],
    task_names: &HashMap<u32, String>,
    span_names: &HashMap<u32, String>,
    pod_id: u32,
) -> Result<String> {
    let mut trace_events = Vec::with_capacity(task_names.len() + events.len());

    let mut tasks: Vec<_> = task_names.iter().collect();
    tasks.sort_unstable_by_key(|(task_id, _)| **task_id);
    for (task_id, task_name) in tasks {
        trace_events.push(serde_json::json!({
            "name": "thread_name",
            "cat": "__metadata",
            "ph": "M",
            "pid": pod_id,
            "tid": task_id,
            "args": {"name": task_name},
        }));
    }

    for event in events {
        // Copy packed struct fields to local variables to avoid unaligned access
        let timestamp = { event.timestamp };
        let task_id = { event.task_id };
        let event_type = EventType::try_from(i32::from(event.event_type)).ok();
        let flags = { event.flags };
        let arg1 = { event.arg1 };
        let arg2 = { event.arg2 };

        let category = category_name((flags >> 4) & 0x0F);

        // Chrome trace event format
        let phase = match event_type {
            Some(EventType::SpanBegin) => "B",
            Some(EventType::SpanEnd) => "E",
            Some(EventType::Counter) => "C",
            Some(EventType::Complete) => "X",
            Some(EventType::MutexLock) => "B",
            Some(EventType::MutexUnlock) => "E",
            _ => "i",
        };

        // Resolve span name from hash
        let name = match event_type {
            Some(EventType::MutexLock | EventType::MutexUnlock) => {
                // Mutex lock/unlock: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("mutex:{}", arg1))
            }
            Some(EventType::MutexContention) => {
                // Mutex contention: resolve name, arg2 = wait time us
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("mutex:{}", arg1))
            }
            Some(EventType::SemTake | EventType::SemGive) => {
                // Semaphore take/give: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("sem:{}", arg1))
            }
            Some(EventType::Counter) => {
                // Counter: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("counter:{}", arg1))
            }
            _ => {
                // Span/instant: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("span:{}", arg1))
            }
        };

        let mut trace_event = serde_json::json!({
            "name": name,
            "cat": category,
            "ph": phase,
            "ts": timestamp,
            "pid": pod_id,
            "tid": task_id,
        });
        let object = trace_event
            .as_object_mut()
            .context("Trace event must serialize as a JSON object")?;

        // Add duration for complete events
        if event_type == Some(EventType::Complete) {
            object.insert("dur".into(), arg2.into());
        }

        // Add counter value
        if event_type == Some(EventType::Counter) {
            object.insert("args".into(), serde_json::json!({"value": arg2}));
        }

        // Add mutex contention wait time
        if event_type == Some(EventType::MutexContention) {
            object.insert("args".into(), serde_json::json!({"wait_us": arg2}));
        }

        trace_events.push(trace_event);
    }

    serde_json::to_string(&trace_events).context("Failed to serialize Perfetto trace JSON")
}

fn decode_stream_batch(
    frame: &Frame,
    sequence_tracker: &mut StreamSequenceTracker,
) -> Result<(u32, Vec<TraceEvent>)> {
    if frame.msg_type != TraceMsgType::StreamData.as_u8() {
        anyhow::bail!(
            "Unexpected trace stream message type: 0x{:02X}, expected STREAM_DATA 0x{:02X}",
            frame.msg_type,
            TraceMsgType::StreamData.as_u8()
        );
    }

    let batch = StreamBatch::decode(frame.payload.as_slice())
        .context("Failed to decode trace StreamBatch")?;
    sequence_tracker
        .accept(batch.sequence)
        .context("Trace stream integrity check failed")?;
    let events = decode_trace_events(&batch.events).context("Invalid trace stream event batch")?;
    Ok((batch.dropped, events))
}

/// Stream trace events in real-time from a TCP connection
///
/// Connects to the trace stream port (5001) on the device and prints
/// events as they arrive. Runs until interrupted (Ctrl+C) or error.
pub fn trace_stream(addr: &str) -> Result<()> {
    use std::net::TcpStream;

    // Connect to trace stream port (5001)
    let stream_addr = if addr.contains(':') {
        // Replace port with 5001
        let parts: Vec<&str> = addr.splitn(2, ':').collect();
        format!("{}:5001", parts[0])
    } else {
        format!("{}:5001", addr)
    };

    eprintln!("Connecting to trace stream at {}...", stream_addr);
    let mut stream = TcpStream::connect(&stream_addr)
        .with_context(|| format!("Failed to connect to trace stream at {}", stream_addr))?;

    stream
        .set_read_timeout(Some(std::time::Duration::from_secs(5)))
        .context("Failed to configure trace stream read timeout")?;

    eprintln!("Connected. Streaming trace events (Ctrl+C to stop)...");
    eprintln!(
        "{:<12} {:<6} {:<12} {:<12} {:>10} {:>10}",
        "TIMESTAMP", "TASK", "TYPE", "CATEGORY", "ARG1", "ARG2"
    );
    eprintln!(
        "{:-<12} {:-<6} {:-<12} {:-<12} {:->10} {:->10}",
        "", "", "", "", "", ""
    );

    // Load span names for resolving
    let span_names = load_span_names(None).unwrap_or_default();

    let mut frame_decoder = crate::transport::frame::FrameDecoder::new();
    let mut sequence_tracker = StreamSequenceTracker::default();
    let mut buf = [0u8; 1024];

    loop {
        use std::io::Read;
        let n = match stream.read(&mut buf) {
            Ok(0) => {
                eprintln!("\nConnection closed by device");
                break;
            }
            Ok(n) => n,
            Err(e)
                if e.kind() == std::io::ErrorKind::TimedOut
                    || e.kind() == std::io::ErrorKind::WouldBlock =>
            {
                continue;
            }
            Err(e) => {
                return Err(anyhow::anyhow!("Read error: {}", e));
            }
        };

        // Feed bytes to frame decoder
        for &byte in &buf[..n] {
            if let Some(frame) = frame_decoder.feed_byte(byte) {
                frame_decoder.reset();
                let frame = frame.context("Invalid trace stream frame")?;
                let (dropped, events) = decode_stream_batch(&frame, &mut sequence_tracker)?;

                if dropped > 0 {
                    eprintln!("  [dropped {} events]", dropped);
                }

                for event in events {
                    let timestamp = { event.timestamp };
                    let task_id = { event.task_id };
                    let event_type = EventType::try_from(i32::from(event.event_type)).ok();
                    let flags = { event.flags };
                    let arg1 = { event.arg1 };
                    let arg2 = { event.arg2 };

                    let type_name = match event_type {
                        Some(EventType::SpanBegin) => "BEGIN",
                        Some(EventType::SpanEnd) => "END",
                        Some(EventType::Instant) => "INSTANT",
                        Some(EventType::Counter) => "COUNTER",
                        Some(EventType::Complete) => "COMPLETE",
                        _ => "UNKNOWN",
                    };

                    let cat = category_name((flags >> 4) & 0x0F);

                    let name = span_names.get(&arg1).map(|s| s.as_str()).unwrap_or("");

                    if event_type == Some(EventType::Counter) {
                        // Counter
                        println!(
                            "{:<12} {:<6} {:<12} {:<12} {} = {}",
                            timestamp, task_id, type_name, cat, name, arg2
                        );
                    } else {
                        println!(
                            "{:<12} {:<6} {:<12} {:<12} {:>10} {:>10}  {}",
                            timestamp, task_id, type_name, cat, arg1, arg2, name
                        );
                    }
                }
            }
        }
    }

    Ok(())
}

fn category_name(cat: u8) -> &'static str {
    Category::try_from(i32::from(cat))
        .map(|category| category.cli_name())
        .unwrap_or("unknown")
}

#[cfg(test)]
mod tests {
    use super::*;

    fn event_bytes(value: u8, count: usize) -> Vec<u8> {
        vec![value; TRACE_EVENT_SIZE * count]
    }

    #[test]
    fn trace_event_decode_is_explicitly_little_endian() {
        let bytes = [
            0x78, 0x56, 0x34, 0x12, 0xCD, 0xAB, 0x1E, 0x09, 0x04, 0x03, 0x02, 0x01, 0xD4, 0xC3,
            0xB2, 0xA1,
        ];
        let events = decode_trace_events(&bytes).unwrap();
        assert_eq!(events.len(), 1);
        assert_eq!(events[0].timestamp, 0x1234_5678);
        assert_eq!(events[0].task_id, 0xABCD);
        assert_eq!(events[0].event_type, 0x1E);
        assert_eq!(events[0].flags, 0x09);
        assert_eq!(events[0].arg1, 0x0102_0304);
        assert_eq!(events[0].arg2, 0xA1B2_C3D4);
    }

    #[test]
    fn dump_integrity_accepts_contiguous_chunks() {
        let mut integrity = TraceDumpIntegrity::default();
        let first = TraceDataChunk {
            offset: 0,
            count: 1,
            events: event_bytes(1, 1),
        };
        let second = TraceDataChunk {
            offset: 1,
            count: 2,
            events: event_bytes(2, 2),
        };

        assert_eq!(integrity.accept_chunk(&first).unwrap().len(), 1);
        assert_eq!(integrity.accept_chunk(&second).unwrap().len(), 2);

        let complete = TraceDumpComplete {
            total_events: 3,
            checksum: (TRACE_EVENT_SIZE as u32) + (TRACE_EVENT_SIZE as u32 * 2 * 2),
        };
        integrity.finish(&complete, 3).unwrap();
    }

    #[test]
    fn dump_integrity_rejects_offset_gap() {
        let mut integrity = TraceDumpIntegrity::default();
        let chunk = TraceDataChunk {
            offset: 1,
            count: 1,
            events: event_bytes(1, 1),
        };

        let error = integrity.accept_chunk(&chunk).unwrap_err().to_string();
        assert!(error.contains("offset mismatch"));
    }

    #[test]
    fn dump_integrity_rejects_count_and_alignment_mismatches() {
        let mut integrity = TraceDumpIntegrity::default();
        let wrong_count = TraceDataChunk {
            offset: 0,
            count: 2,
            events: event_bytes(1, 1),
        };
        assert!(integrity
            .accept_chunk(&wrong_count)
            .unwrap_err()
            .to_string()
            .contains("count mismatch"));

        let mut integrity = TraceDumpIntegrity::default();
        let misaligned = TraceDataChunk {
            offset: 0,
            count: 1,
            events: vec![0; TRACE_EVENT_SIZE - 1],
        };
        assert!(integrity
            .accept_chunk(&misaligned)
            .unwrap_err()
            .to_string()
            .contains("multiple"));
    }

    #[test]
    fn dump_integrity_rejects_bad_completion_metadata() {
        let mut integrity = TraceDumpIntegrity::default();
        let chunk = TraceDataChunk {
            offset: 0,
            count: 1,
            events: event_bytes(1, 1),
        };
        integrity.accept_chunk(&chunk).unwrap();

        let wrong_total = TraceDumpComplete {
            total_events: 2,
            checksum: TRACE_EVENT_SIZE as u32,
        };
        assert!(integrity
            .finish(&wrong_total, 1)
            .unwrap_err()
            .to_string()
            .contains("total mismatch"));

        let wrong_checksum = TraceDumpComplete {
            total_events: 1,
            checksum: 0,
        };
        assert!(integrity
            .finish(&wrong_checksum, 1)
            .unwrap_err()
            .to_string()
            .contains("checksum mismatch"));
    }

    #[test]
    fn stream_sequence_tracker_detects_gaps_and_wraps() {
        let mut tracker = StreamSequenceTracker::default();
        tracker.accept(0).unwrap();
        tracker.accept(1).unwrap();
        assert!(tracker.accept(3).unwrap_err().to_string().contains("gap"));

        let mut wrapping = StreamSequenceTracker {
            next_sequence: u32::MAX,
        };
        wrapping.accept(u32::MAX).unwrap();
        wrapping.accept(0).unwrap();
    }

    #[test]
    fn stream_sequence_tracker_rejects_nonzero_initial_sequence() {
        let error = StreamSequenceTracker::default()
            .accept(1)
            .unwrap_err()
            .to_string();
        assert!(error.contains("expected 0, got 1"));
    }

    #[test]
    fn stream_batch_rejects_unexpected_frame_type() {
        let frame = Frame {
            msg_type: TraceMsgType::Ack.as_u8(),
            payload: Vec::new(),
        };
        let error = decode_stream_batch(&frame, &mut StreamSequenceTracker::default())
            .unwrap_err()
            .to_string();
        assert!(error.contains("Unexpected trace stream message type"));
    }

    #[test]
    fn perfetto_json_escapes_task_and_span_names() {
        let event = TraceEvent {
            timestamp: 123,
            task_id: 7,
            event_type: EventType::Instant as u8,
            flags: (Category::Game as u8) << 4,
            arg1: 42,
            arg2: 0,
        };
        let task_name = "task\"\\name\n";
        let span_name = "span\"\\name\n";
        let task_names = HashMap::from([(7, task_name.to_string())]);
        let span_names = HashMap::from([(42, span_name.to_string())]);

        let json = convert_to_perfetto_json(&[event], &task_names, &span_names, 9).unwrap();
        let value: serde_json::Value = serde_json::from_str(&json).unwrap();

        assert_eq!(value[0]["args"]["name"], task_name);
        assert_eq!(value[1]["name"], span_name);
    }

    #[test]
    fn trace_duration_handles_timestamp_wrap() {
        assert_eq!(trace_duration_us(100, 175), 75);
        assert_eq!(trace_duration_us(u32::MAX - 10, 9), 20);
    }
}
