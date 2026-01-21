//! Trace/perfetto commands

use crate::proto::trace::{MsgType as TraceMsgType, Status as TraceStatus};
use crate::transport::Transport;
use anyhow::{Context, Result};
use std::fs::File;
use std::io::Write;
use std::path::Path;

/// Binary structures matching firmware trace protocol (traceProtocol.hpp)
/// These are packed binary, not protobuf.

/// Trace metadata sent at start of dump
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct TraceMetadata {
    event_count: u32,
    dropped_count: u32,
    start_timestamp: u32,
    end_timestamp: u32,
    task_count: u8,
}

/// Task entry in trace metadata
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct TraceTaskEntry {
    task_id: u16,
    name: [u8; 16],
}

/// Header for trace data chunks
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct TraceDataHeader {
    offset: u32,
    count: u16,
}

/// Compact trace event (16 bytes)
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

/// End of dump message payload
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct TraceDumpEnd {
    total_events: u32,
    checksum: u32,
}

/// Status response payload
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
struct TraceStatusResponse {
    initialized: u8,
    enabled: u8,
    event_count: u32,
    dropped_count: u32,
    buffer_size: u32,
}

/// Trace status information
#[derive(Debug)]
pub struct TraceStatusInfo {
    pub initialized: bool,
    pub enabled: bool,
    pub event_count: u32,
    pub dropped_count: u32,
    pub buffer_size: u32,
}

/// Start tracing
pub fn trace_start(transport: &mut dyn Transport) -> Result<()> {
    let frame = transport
        .send_command(TraceMsgType::Start.as_u8(), &[])
        .context("Failed to send trace start command")?;

    if frame.msg_type != TraceMsgType::Ack.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Ack.as_u8()
        );
    }

    if frame.payload.is_empty() {
        anyhow::bail!("Empty ACK payload");
    }

    let status = TraceStatus::try_from(frame.payload[0] as i32)
        .unwrap_or(TraceStatus::Error);

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
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Ack.as_u8()
        );
    }

    if frame.payload.is_empty() {
        anyhow::bail!("Empty ACK payload");
    }

    let status = TraceStatus::try_from(frame.payload[0] as i32)
        .unwrap_or(TraceStatus::Error);

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
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Ack.as_u8()
        );
    }

    if frame.payload.is_empty() {
        anyhow::bail!("Empty ACK payload");
    }

    let status = TraceStatus::try_from(frame.payload[0] as i32)
        .unwrap_or(TraceStatus::Error);

    match status {
        TraceStatus::Ok => Ok(()),
        TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
        _ => anyhow::bail!("Trace clear failed: {}", status),
    }
}

/// Get trace status
pub fn trace_status(transport: &mut dyn Transport) -> Result<TraceStatusInfo> {
    let frame = transport
        .send_command(TraceMsgType::Status.as_u8(), &[])
        .context("Failed to send trace status command")?;

    // Check for ACK with error first
    if frame.msg_type == TraceMsgType::Ack.as_u8() {
        if frame.payload.is_empty() {
            anyhow::bail!("Empty ACK payload");
        }
        let status = TraceStatus::try_from(frame.payload[0] as i32)
            .unwrap_or(TraceStatus::Error);
        if status == TraceStatus::NotInit {
            anyhow::bail!("Trace system not initialized");
        }
        anyhow::bail!("Trace status failed: {}", status);
    }

    if frame.msg_type != TraceMsgType::Status.as_u8() {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            TraceMsgType::Status.as_u8()
        );
    }

    if frame.payload.len() < std::mem::size_of::<TraceStatusResponse>() {
        anyhow::bail!(
            "Status payload too short: {} bytes, expected {}",
            frame.payload.len(),
            std::mem::size_of::<TraceStatusResponse>()
        );
    }

    let status = unsafe {
        std::ptr::read_unaligned(frame.payload.as_ptr() as *const TraceStatusResponse)
    };

    Ok(TraceStatusInfo {
        initialized: status.initialized != 0,
        enabled: status.enabled != 0,
        event_count: status.event_count,
        dropped_count: status.dropped_count,
        buffer_size: status.buffer_size,
    })
}

/// Dump traces to a JSON file compatible with Perfetto
pub fn trace_dump(transport: &mut dyn Transport, output_path: &Path) -> Result<DumpResult> {
    let frame = transport
        .send_command(TraceMsgType::Dump.as_u8(), &[])
        .context("Failed to send trace dump command")?;

    // Check for ACK with error (e.g., buffer empty)
    if frame.msg_type == TraceMsgType::Ack.as_u8() {
        if frame.payload.is_empty() {
            anyhow::bail!("Empty ACK payload");
        }
        let status = TraceStatus::try_from(frame.payload[0] as i32)
            .unwrap_or(TraceStatus::Error);
        match status {
            TraceStatus::NotInit => anyhow::bail!("Trace system not initialized"),
            TraceStatus::BufferEmpty => anyhow::bail!("Trace buffer is empty"),
            _ => anyhow::bail!("Trace dump failed: {}", status),
        }
    }

    // First response should be DATA with metadata
    if frame.msg_type != TraceMsgType::Data.as_u8() {
        anyhow::bail!(
            "Expected DATA message, got: 0x{:02X}",
            frame.msg_type
        );
    }

    // Parse metadata
    if frame.payload.len() < std::mem::size_of::<TraceMetadata>() {
        anyhow::bail!("Metadata too short");
    }

    let metadata = unsafe {
        std::ptr::read_unaligned(frame.payload.as_ptr() as *const TraceMetadata)
    };

    // Parse task entries
    let mut tasks: Vec<(u16, String)> = Vec::new();
    let task_data_offset = std::mem::size_of::<TraceMetadata>();
    let task_entry_size = std::mem::size_of::<TraceTaskEntry>();

    for i in 0..metadata.task_count as usize {
        let offset = task_data_offset + i * task_entry_size;
        if offset + task_entry_size > frame.payload.len() {
            break;
        }
        let entry = unsafe {
            std::ptr::read_unaligned(
                frame.payload[offset..].as_ptr() as *const TraceTaskEntry
            )
        };
        let name = std::str::from_utf8(&entry.name)
            .unwrap_or("???")
            .trim_end_matches('\0')
            .to_string();
        tasks.push((entry.task_id, name));
    }

    // Collect all events
    let mut events: Vec<TraceEvent> = Vec::with_capacity(metadata.event_count as usize);
    let mut total_received = 0u32;

    loop {
        let frame = transport
            .receive_frame(5000)  // 5 second timeout for trace data
            .context("Failed to receive trace data")?;

        match frame.msg_type {
            t if t == TraceMsgType::Data.as_u8() => {
                // Parse data header
                if frame.payload.len() < std::mem::size_of::<TraceDataHeader>() {
                    continue;
                }
                let header = unsafe {
                    std::ptr::read_unaligned(
                        frame.payload.as_ptr() as *const TraceDataHeader
                    )
                };

                // Parse events
                let event_data_offset = std::mem::size_of::<TraceDataHeader>();
                let event_size = std::mem::size_of::<TraceEvent>();

                for i in 0..header.count as usize {
                    let offset = event_data_offset + i * event_size;
                    if offset + event_size > frame.payload.len() {
                        break;
                    }
                    let event = unsafe {
                        std::ptr::read_unaligned(
                            frame.payload[offset..].as_ptr() as *const TraceEvent
                        )
                    };
                    events.push(event);
                    total_received += 1;
                }
            }
            t if t == TraceMsgType::End.as_u8() => {
                // Parse end message
                if frame.payload.len() >= std::mem::size_of::<TraceDumpEnd>() {
                    let _end = unsafe {
                        std::ptr::read_unaligned(
                            frame.payload.as_ptr() as *const TraceDumpEnd
                        )
                    };
                }
                break;
            }
            _ => {
                anyhow::bail!("Unexpected message type during dump: 0x{:02X}", frame.msg_type);
            }
        }
    }

    // Convert to Chrome JSON trace format for Perfetto
    let json = convert_to_perfetto_json(&events, &tasks)?;

    // Write to file
    let mut file = File::create(output_path)
        .context("Failed to create output file")?;
    file.write_all(json.as_bytes())
        .context("Failed to write trace file")?;

    Ok(DumpResult {
        event_count: total_received,
        dropped_count: metadata.dropped_count,
        duration_us: metadata.end_timestamp.saturating_sub(metadata.start_timestamp),
        output_path: output_path.to_path_buf(),
    })
}

/// Result of a trace dump operation
pub struct DumpResult {
    pub event_count: u32,
    pub dropped_count: u32,
    pub duration_us: u32,
    pub output_path: std::path::PathBuf,
}

/// Convert trace events to Perfetto-compatible Chrome JSON format
fn convert_to_perfetto_json(
    events: &[TraceEvent],
    tasks: &[(u16, String)],
) -> Result<String> {
    use std::fmt::Write;

    let mut json = String::from("[");
    let mut first = true;

    // Create task name lookup
    let task_names: std::collections::HashMap<u16, &str> = tasks
        .iter()
        .map(|(id, name)| (*id, name.as_str()))
        .collect();

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
            .get(&task_id)
            .copied()
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
            _ => "i",    // Default to instant
        };

        let name = match event_type {
            0x01 | 0x02 => format!("task:{}", task_name),
            0x05 | 0x06 => format!("isr:{}", arg1),
            _ => format!("span:{}", arg1),
        };

        write!(
            &mut json,
            r#"{{"name":"{}","cat":"{}","ph":"{}","ts":{},"pid":0,"tid":{}"#,
            name,
            category,
            phase,
            timestamp,
            task_id
        )?;

        // Add duration for complete events
        if event_type == 0x24 {
            write!(&mut json, r#","dur":{}"#, arg2)?;
        }

        // Add counter value
        if event_type == 0x23 {
            write!(&mut json, r#","args":{{"value":{}}}"#, arg2)?;
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
        _ => "unknown",
    }
}
