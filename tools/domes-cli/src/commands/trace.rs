//! Trace/perfetto commands
//!
//! Uses protobuf-encoded messages for all control/metadata (prost).
//! TraceEvent data is 16-byte binary carried in protobuf 'bytes' fields.

use crate::proto::trace::{
    AckResponse, MsgType as TraceMsgType, Status as TraceStatus, TraceDataChunk,
    TraceDumpComplete, TraceSessionInfo, TraceStatusResponse,
};
use crate::transport::Transport;
use anyhow::{Context, Result};
use prost::Message;
use std::collections::HashMap;
use std::fs::File;
use std::io::Write;
use std::path::Path;

/// Compact trace event (16 bytes, binary)
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct TraceEvent {
    timestamp: u32,
    task_id: u16,
    event_type: u8,
    flags: u8,
    arg1: u32,
    arg2: u32,
}

/// Trace status information
#[derive(Debug)]
pub struct TraceStatusInfo {
    pub initialized: bool,
    pub enabled: bool,
    pub streaming: bool,
    pub event_count: u32,
    pub dropped_count: u32,
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
        TraceStatus::Ok => Ok(()),
        TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
        TraceStatus::AlreadyOn => anyhow::bail!("Tracing is already enabled"),
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
        TraceStatus::Ok => Ok(()),
        TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
        TraceStatus::AlreadyOff => anyhow::bail!("Tracing is already disabled"),
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
    let mut events: Vec<TraceEvent> = Vec::with_capacity(session_info.event_count as usize);
    let mut total_received = 0u32;

    loop {
        let frame = transport
            .receive_frame(5000) // 5 second timeout for trace data
            .context("Failed to receive trace data")?;

        if frame.msg_type == TraceMsgType::Data.as_u8() {
            // Parse data chunk (protobuf)
            let chunk = TraceDataChunk::decode(frame.payload.as_slice())
                .context("Failed to decode TraceDataChunk")?;

            // Extract binary events from bytes field
            let event_bytes = &chunk.events;
            let event_size = std::mem::size_of::<TraceEvent>();
            let event_count = event_bytes.len() / event_size;

            for i in 0..event_count {
                let offset = i * event_size;
                if offset + event_size <= event_bytes.len() {
                    let event = unsafe {
                        std::ptr::read_unaligned(
                            event_bytes[offset..].as_ptr() as *const TraceEvent,
                        )
                    };
                    events.push(event);
                    total_received += 1;
                }
            }
        } else if frame.msg_type == TraceMsgType::End.as_u8() {
            // Parse dump complete (protobuf)
            let _end = TraceDumpComplete::decode(frame.payload.as_slice())
                .context("Failed to decode TraceDumpComplete")?;
            break;
        } else {
            anyhow::bail!(
                "Unexpected message type during dump: 0x{:02X}",
                frame.msg_type
            );
        }
    }

    // Convert to Chrome JSON trace format for Perfetto
    let json = convert_to_perfetto_json(
        &events,
        &task_names,
        &span_names,
        session_info.pod_id,
    )?;

    // Write to file
    let mut file =
        File::create(output_path).context("Failed to create output file")?;
    file.write_all(json.as_bytes())
        .context("Failed to write trace file")?;

    Ok(DumpResult {
        event_count: total_received,
        dropped_count: session_info.dropped_count,
        duration_us: session_info
            .end_timestamp_us
            .saturating_sub(session_info.start_timestamp_us),
        pod_id: session_info.pod_id,
        output_path: output_path.to_path_buf(),
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
    use std::fmt::Write;

    let mut json = String::from("[");
    let mut first = true;

    for event in events {
        if !first {
            json.push(',');
        }
        first = false;

        // Copy packed struct fields to local variables to avoid unaligned access
        let timestamp = { event.timestamp };
        let task_id = { event.task_id };
        let event_type = { event.event_type };
        let flags = { event.flags };
        let arg1 = { event.arg1 };
        let arg2 = { event.arg2 };

        let task_name = task_names
            .get(&(task_id as u32))
            .map(|s| s.as_str())
            .unwrap_or("unknown");
        let category = category_name((flags >> 4) & 0x0F);

        // Chrome trace event format
        let phase = match event_type {
            0x20 => "B", // SPAN_BEGIN -> Begin
            0x21 => "E", // SPAN_END -> End
            0x22 => "i", // INSTANT -> Instant
            0x23 => "C", // COUNTER -> Counter
            0x24 => "X", // COMPLETE -> Complete (duration in arg2)
            0x01 => "B", // TASK_SWITCH_IN -> Begin
            0x02 => "E", // TASK_SWITCH_OUT -> End
            0x05 => "B", // ISR_ENTER -> Begin
            0x06 => "E", // ISR_EXIT -> End
            0x09 => "B", // MUTEX_LOCK -> Begin (lock held)
            0x0A => "E", // MUTEX_UNLOCK -> End (lock released)
            0x0B => "i", // MUTEX_CONTENTION -> Instant (with wait time)
            0x0C => "i", // SEM_TAKE -> Instant
            0x0D => "i", // SEM_GIVE -> Instant
            _ => "i",    // Default to instant
        };

        // Resolve span name from hash
        let name = match event_type {
            0x01 | 0x02 => format!("task:{}", task_name),
            0x05 | 0x06 => format!("isr:{}", arg1),
            0x09 | 0x0A => {
                // Mutex lock/unlock: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("mutex:{}", arg1))
            }
            0x0B => {
                // Mutex contention: resolve name, arg2 = wait time us
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("mutex:{}", arg1))
            }
            0x0C | 0x0D => {
                // Semaphore take/give: resolve name from hash
                span_names
                    .get(&arg1)
                    .cloned()
                    .unwrap_or_else(|| format!("sem:{}", arg1))
            }
            0x23 => {
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

        write!(
            &mut json,
            r#"{{"name":"{}","cat":"{}","ph":"{}","ts":{},"pid":{},"tid":{}"#,
            name, category, phase, timestamp, pod_id, task_id
        )?;

        // Add duration for complete events
        if event_type == 0x24 {
            write!(&mut json, r#","dur":{}"#, arg2)?;
        }

        // Add counter value
        if event_type == 0x23 {
            write!(&mut json, r#","args":{{"value":{}}}"#, arg2)?;
        }

        // Add mutex contention wait time
        if event_type == 0x0B {
            write!(&mut json, r#","args":{{"wait_us":{}}}"#, arg2)?;
        }

        json.push('}');
    }

    json.push(']');
    Ok(json)
}

fn category_name(cat: u8) -> &'static str {
    match cat {
        0 => "kernel",
        1 => "transport",
        2 => "ota",
        3 => "wifi",
        4 => "led",
        5 => "audio",
        6 => "touch",
        7 => "game",
        8 => "user",
        9 => "haptic",
        10 => "ble",
        11 => "nvs",
        12 => "espnow",
        13 => "sync",
        _ => "unknown",
    }
}
