//! Protocol sniffer command
//!
//! Captures and decodes DOMES protocol frames on any transport.
//! Prints human-readable decoded output, raw hex, or JSON lines.

use crate::proto::{config::MsgType as ConfigMsgType, trace::MsgType as TraceMsgType};
use crate::transport::frame::{Frame, FrameDecoder};
use crate::transport::serial::open_serial_port;
use anyhow::{Context, Result};
use prost::Message;
use std::io::Read;
use std::time::{Duration, Instant};

/// Protocol filter categories
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ProtocolFilter {
    Config,
    Trace,
    Ota,
}

impl ProtocolFilter {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "config" => Some(Self::Config),
            "trace" => Some(Self::Trace),
            "ota" => Some(Self::Ota),
            _ => None,
        }
    }

    /// Check if a message type belongs to this protocol filter
    pub fn matches(&self, msg_type: u8) -> bool {
        match self {
            Self::Config => config_msg_type(msg_type).is_some(),
            Self::Trace => trace_msg_type(msg_type).is_some(),
            Self::Ota => (0x01..=0x05).contains(&msg_type),
        }
    }
}

/// Output format for the sniffer
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum OutputFormat {
    /// Human-readable decoded output
    Pretty,
    /// Raw hex bytes
    Raw,
    /// JSON lines (one JSON object per frame)
    Json,
}

/// Sniffer options
pub struct SniffOptions {
    pub filters: Vec<ProtocolFilter>,
    pub format: OutputFormat,
    pub count: Option<u32>,
}

/// Decoded frame info for display
struct DecodedFrame {
    timestamp: Duration,
    msg_type: u8,
    msg_name: String,
    direction: &'static str,
    protocol: &'static str,
    payload_size: usize,
    decoded_fields: Vec<(String, String)>,
    raw_payload: Vec<u8>,
}

/// Run the sniffer on a serial port
pub fn sniff_serial(port_name: &str, opts: &SniffOptions) -> Result<()> {
    let port = open_serial_port(port_name, Duration::from_millis(100))?;

    eprintln!("Sniffing on {} (press Ctrl+C to stop)", port_name);
    eprintln!();

    let mut decoder = FrameDecoder::new();
    let start = Instant::now();
    let mut buf = [0u8; 256];
    let mut frame_count = 0u32;
    let mut reader = port;

    loop {
        match reader.read(&mut buf) {
            Ok(0) => {
                std::thread::sleep(Duration::from_millis(1));
                continue;
            }
            Ok(n) => {
                for &byte in &buf[..n] {
                    if let Some(result) = decoder.feed_byte(byte) {
                        match result {
                            Ok(frame) => {
                                let elapsed = start.elapsed();
                                if should_display(&frame, &opts.filters) {
                                    let decoded = decode_frame(elapsed, &frame);
                                    display_frame(&decoded, opts.format);
                                    frame_count += 1;

                                    if let Some(max) = opts.count {
                                        if frame_count >= max {
                                            eprintln!(
                                                "\nCaptured {} frame(s), stopping.",
                                                frame_count
                                            );
                                            return Ok(());
                                        }
                                    }
                                }
                            }
                            Err(e) => {
                                eprintln!("[FRAME ERROR] {}", e);
                            }
                        }
                        decoder.reset();
                    }
                }
            }
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut => {
                continue;
            }
            Err(e) => {
                return Err(e).context("Failed to read from serial port");
            }
        }
    }
}

/// Check if a frame should be displayed based on filters
fn should_display(frame: &Frame, filters: &[ProtocolFilter]) -> bool {
    if filters.is_empty() {
        return true;
    }
    filters.iter().any(|f| f.matches(frame.msg_type))
}

fn config_msg_type(msg_type: u8) -> Option<ConfigMsgType> {
    ConfigMsgType::try_from(i32::from(msg_type))
        .ok()
        .filter(|kind| *kind != ConfigMsgType::Unknown)
}

fn trace_msg_type(msg_type: u8) -> Option<TraceMsgType> {
    TraceMsgType::try_from(i32::from(msg_type))
        .ok()
        .filter(|kind| *kind != TraceMsgType::Unknown)
}

fn config_response_has_status(kind: ConfigMsgType) -> bool {
    kind != ConfigMsgType::ListFeaturesRsp && kind.as_str_name().ends_with("_RSP")
}

fn config_status_name(status: i32) -> String {
    crate::proto::config::Status::try_from(status)
        .map(|status| status.as_str_name().to_string())
        .unwrap_or_else(|_| format!("UNKNOWN({})", status))
}

/// Decode a frame into human-readable info
fn decode_frame(timestamp: Duration, frame: &Frame) -> DecodedFrame {
    let msg_type = frame.msg_type;
    let (msg_name, direction, protocol) = identify_message(msg_type);
    let decoded_fields = decode_payload(msg_type, &frame.payload);

    DecodedFrame {
        timestamp,
        msg_type,
        msg_name,
        direction,
        protocol,
        payload_size: frame.payload.len(),
        decoded_fields,
        raw_payload: frame.payload.clone(),
    }
}

/// Identify a message type by name, direction, and protocol
fn identify_message(msg_type: u8) -> (String, &'static str, &'static str) {
    // OTA is a bounded legacy protocol without protobuf message-type definitions.
    match msg_type {
        0x01 => return ("OTA_BEGIN".into(), "host->dev", "ota"),
        0x02 => return ("OTA_DATA".into(), "host->dev", "ota"),
        0x03 => return ("OTA_END".into(), "host->dev", "ota"),
        0x04 => return ("OTA_ACK".into(), "dev->host", "ota"),
        0x05 => return ("OTA_ABORT".into(), "either", "ota"),
        _ => {}
    }

    identify_protobuf_message(msg_type)
}

fn identify_protobuf_message(msg_type: u8) -> (String, &'static str, &'static str) {
    if let Some(kind) = trace_msg_type(msg_type) {
        let proto_name = kind.as_str_name();
        let short_name = proto_name.strip_prefix("MSG_TYPE_").unwrap_or(proto_name);
        let direction = match kind {
            TraceMsgType::Start
            | TraceMsgType::Stop
            | TraceMsgType::Dump
            | TraceMsgType::Clear
            | TraceMsgType::StatusReq => "host->dev",
            TraceMsgType::Data
            | TraceMsgType::End
            | TraceMsgType::StatusResp
            | TraceMsgType::StreamData
            | TraceMsgType::SessionInfo
            | TraceMsgType::Ack => "dev->host",
            TraceMsgType::Unknown => "unknown",
        };
        return (format!("TRACE_{short_name}"), direction, "trace");
    }

    if let Some(kind) = config_msg_type(msg_type) {
        let proto_name = kind.as_str_name();
        let name = proto_name
            .strip_prefix("MSG_TYPE_")
            .unwrap_or(proto_name)
            .to_string();
        let direction = if proto_name.ends_with("_RSP") || proto_name.ends_with("_NTF") {
            "dev->host"
        } else {
            "host->dev"
        };
        return (name, direction, "config");
    }

    (format!("UNKNOWN_0x{:02X}", msg_type), "unknown", "unknown")
}

/// Decode protobuf payload fields into key-value pairs
fn decode_payload(msg_type: u8, payload: &[u8]) -> Vec<(String, String)> {
    if let Some(kind) = config_msg_type(msg_type) {
        return decode_config_payload(kind, payload);
    }
    if let Some(kind) = trace_msg_type(msg_type) {
        return decode_trace_payload(kind, payload);
    }

    payload_size_field(payload)
}

fn decode_config_payload(kind: ConfigMsgType, payload: &[u8]) -> Vec<(String, String)> {
    let mut fields = Vec::new();

    // Skip status byte for response messages that include one
    let proto_payload = if config_response_has_status(kind) {
        if payload.is_empty() {
            return fields;
        }
        fields.push(("status".into(), config_status_name(i32::from(payload[0]))));
        if payload[0] != 0 {
            return fields;
        }
        &payload[1..]
    } else {
        payload
    };

    match kind {
        ConfigMsgType::ListFeaturesRsp => {
            if let Ok(resp) = crate::proto::config::ListFeaturesResponse::decode(proto_payload) {
                if resp.pod_id > 0 {
                    fields.push(("pod_id".into(), resp.pod_id.to_string()));
                }
                for fs in &resp.features {
                    let fname = feature_name(fs.feature);
                    let state = if fs.enabled { "enabled" } else { "disabled" };
                    fields.push((fname, state.into()));
                }
            }
        }

        ConfigMsgType::SetFeatureReq => {
            if let Ok(req) = crate::proto::config::SetFeatureRequest::decode(proto_payload) {
                fields.push(("feature".into(), feature_name(req.feature)));
                fields.push(("enabled".into(), req.enabled.to_string()));
            }
        }

        ConfigMsgType::SetFeatureRsp => {
            if let Ok(resp) = crate::proto::config::SetFeatureResponse::decode(proto_payload) {
                if let Some(fs) = &resp.feature {
                    fields.push(("feature".into(), feature_name(fs.feature)));
                    fields.push(("enabled".into(), fs.enabled.to_string()));
                }
            }
        }

        ConfigMsgType::GetFeatureReq => {
            if let Ok(req) = crate::proto::config::GetFeatureRequest::decode(proto_payload) {
                fields.push(("feature".into(), feature_name(req.feature)));
            }
        }

        ConfigMsgType::GetFeatureRsp => {
            if let Ok(resp) = crate::proto::config::GetFeatureResponse::decode(proto_payload) {
                if let Some(fs) = &resp.feature {
                    fields.push(("feature".into(), feature_name(fs.feature)));
                    fields.push(("enabled".into(), fs.enabled.to_string()));
                }
            }
        }

        ConfigMsgType::GetModeRsp => {
            if let Ok(resp) = crate::proto::config::GetModeResponse::decode(proto_payload) {
                fields.push(("mode".into(), mode_name(resp.mode)));
                fields.push(("time_in_mode_ms".into(), resp.time_in_mode_ms.to_string()));
            }
        }

        ConfigMsgType::SetModeReq => {
            if let Ok(req) = crate::proto::config::SetModeRequest::decode(proto_payload) {
                fields.push(("mode".into(), mode_name(req.mode)));
            }
        }

        ConfigMsgType::GetSystemInfoRsp => {
            if let Ok(resp) = crate::proto::config::GetSystemInfoResponse::decode(proto_payload) {
                fields.push(("firmware".into(), resp.firmware_version));
                fields.push(("pod_id".into(), resp.pod_id.to_string()));
                fields.push(("mode".into(), mode_name(resp.mode)));
                fields.push(("uptime_s".into(), resp.uptime_s.to_string()));
                fields.push(("free_heap".into(), resp.free_heap.to_string()));
                fields.push(("boot_count".into(), resp.boot_count.to_string()));
                fields.push((
                    "reset_reason".into(),
                    crate::proto::config::ResetReason::try_from(resp.reset_reason)
                        .unwrap_or(crate::proto::config::ResetReason::Unknown)
                        .cli_name()
                        .into(),
                ));
            }
        }

        ConfigMsgType::GetHealthRsp => {
            if let Ok(resp) = crate::proto::config::GetHealthResponse::decode(proto_payload) {
                fields.push(("free_heap".into(), resp.free_heap.to_string()));
                fields.push(("min_free_heap".into(), resp.min_free_heap.to_string()));
                fields.push(("uptime_s".into(), resp.uptime_seconds.to_string()));
                fields.push(("tasks".into(), resp.tasks.len().to_string()));
            }
        }

        ConfigMsgType::SetLedPatternReq => {
            if let Ok(req) = crate::proto::config::SetLedPatternRequest::decode(proto_payload) {
                if let Some(p) = &req.pattern {
                    let pattern_name = crate::proto::config::LedPatternType::try_from(p.r#type)
                        .map(|pattern| {
                            pattern
                                .as_str_name()
                                .strip_prefix("LED_PATTERN_")
                                .unwrap_or(pattern.as_str_name())
                                .to_ascii_lowercase()
                        })
                        .unwrap_or_else(|_| format!("unknown({})", p.r#type));
                    fields.push(("type".into(), pattern_name));
                    if let Some(c) = &p.color {
                        fields.push((
                            "color".into(),
                            format!("#{:02x}{:02x}{:02x}", c.r, c.g, c.b),
                        ));
                    }
                    fields.push(("period_ms".into(), p.period_ms.to_string()));
                    fields.push(("brightness".into(), p.brightness.to_string()));
                }
            }
        }

        ConfigMsgType::GetEspnowStatusRsp => {
            if let Ok(resp) = crate::proto::config::GetEspNowStatusResponse::decode(proto_payload) {
                fields.push(("state".into(), resp.discovery_state));
                fields.push(("peers".into(), resp.peer_count.to_string()));
                fields.push(("tx".into(), resp.tx_count.to_string()));
                fields.push(("rx".into(), resp.rx_count.to_string()));
            }
        }

        ConfigMsgType::GetCrashDumpRsp => {
            if let Ok(resp) = crate::proto::config::CrashDumpResponse::decode(proto_payload) {
                fields.push(("has_dump".into(), resp.has_dump.to_string()));
                if resp.has_dump {
                    fields.push(("reason".into(), resp.reason));
                    fields.push(("task".into(), resp.task_name));
                    fields.push(("uptime_s".into(), resp.uptime_s.to_string()));
                    fields.push(("free_heap".into(), resp.free_heap.to_string()));
                    fields.push(("backtrace_entries".into(), resp.backtrace.len().to_string()));
                    fields.push(("boot_count".into(), resp.timestamp.to_string()));
                }
            }
        }

        ConfigMsgType::ClearCrashDumpRsp => {
            if let Ok(resp) = crate::proto::config::ClearCrashDumpResponse::decode(proto_payload) {
                fields.push(("cleared".into(), resp.cleared.to_string()));
            }
        }

        ConfigMsgType::GetMemoryProfileRsp => {
            if let Ok(resp) = crate::proto::config::GetMemoryProfileResponse::decode(proto_payload)
            {
                fields.push(("free_heap".into(), resp.current_free_heap.to_string()));
                fields.push((
                    "min_free_heap".into(),
                    resp.current_min_free_heap.to_string(),
                ));
                fields.push((
                    "largest_block".into(),
                    resp.current_largest_block.to_string(),
                ));
                fields.push(("total_heap".into(), resp.total_heap.to_string()));
                fields.push(("samples".into(), resp.samples.len().to_string()));
            }
        }

        ConfigMsgType::SelfTestRsp => {
            if let Ok(resp) = crate::proto::config::SelfTestResponse::decode(proto_payload) {
                fields.push(("tests_run".into(), resp.tests_run.to_string()));
                fields.push(("tests_passed".into(), resp.tests_passed.to_string()));
                fields.push(("results".into(), resp.results.len().to_string()));
            }
        }

        ConfigMsgType::CheckUpdateRsp => {
            if let Ok(resp) = crate::proto::config::CheckUpdateResponse::decode(proto_payload) {
                fields.push(("update_available".into(), resp.update_available.to_string()));
                fields.push(("current_version".into(), resp.current_version));
                fields.push(("available_version".into(), resp.available_version));
                fields.push(("firmware_size".into(), resp.firmware_size.to_string()));
                fields.push(("auto_update".into(), resp.auto_update_enabled.to_string()));
            }
        }

        ConfigMsgType::SetAutoUpdateReq => {
            if let Ok(req) = crate::proto::config::SetAutoUpdateRequest::decode(proto_payload) {
                fields.push(("enabled".into(), req.enabled.to_string()));
            }
        }

        ConfigMsgType::SetAutoUpdateRsp => {
            if let Ok(resp) = crate::proto::config::SetAutoUpdateResponse::decode(proto_payload) {
                fields.push(("enabled".into(), resp.enabled.to_string()));
            }
        }

        ConfigMsgType::SimulateTouchReq => {
            if let Ok(req) = crate::proto::config::SimulateTouchRequest::decode(proto_payload) {
                fields.push(("pad_index".into(), req.pad_index.to_string()));
            }
        }

        ConfigMsgType::SimulateTouchRsp => {
            let _ = crate::proto::config::SimulateTouchResponse::decode(proto_payload);
        }

        ConfigMsgType::SetSimModeReq => {
            if let Ok(req) = crate::proto::config::SetSimModeRequest::decode(proto_payload) {
                fields.push(("enabled".into(), req.enabled.to_string()));
                fields.push(("delay_ms".into(), req.delay_ms.to_string()));
                fields.push(("pad_index".into(), req.pad_index.to_string()));
            }
        }

        ConfigMsgType::SetSimModeRsp => {
            if let Ok(resp) = crate::proto::config::SetSimModeResponse::decode(proto_payload) {
                fields.push(("enabled".into(), resp.enabled.to_string()));
                fields.push(("delay_ms".into(), resp.delay_ms.to_string()));
                fields.push(("pad_index".into(), resp.pad_index.to_string()));
            }
        }

        ConfigMsgType::TouchEventNtf => {
            if let Ok(event) = crate::proto::config::TouchEventNotification::decode(proto_payload) {
                fields.push(("pod_id".into(), event.pod_id.to_string()));
                fields.push(("pad_index".into(), event.pad_index.to_string()));
                fields.push(("timestamp_us".into(), event.timestamp_us.to_string()));
            }
        }

        _ => {
            fields.extend(payload_size_field(proto_payload));
        }
    }

    fields
}

fn decode_trace_payload(kind: TraceMsgType, payload: &[u8]) -> Vec<(String, String)> {
    let mut fields = Vec::new();

    match kind {
        TraceMsgType::Ack => {
            if let Ok(ack) = crate::proto::trace::AckResponse::decode(payload) {
                let status = crate::proto::trace::Status::try_from(ack.status)
                    .map(|status| status.to_string())
                    .unwrap_or_else(|_| format!("unknown({})", ack.status));
                fields.push(("status".into(), status));
            }
        }
        TraceMsgType::StatusResp => {
            if let Ok(resp) = crate::proto::trace::TraceStatusResponse::decode(payload) {
                fields.push(("initialized".into(), resp.initialized.to_string()));
                fields.push(("enabled".into(), resp.enabled.to_string()));
                fields.push(("streaming".into(), resp.streaming.to_string()));
                fields.push(("events".into(), resp.event_count.to_string()));
                fields.push(("dropped".into(), resp.dropped_count.to_string()));
                fields.push(("buffer_size".into(), resp.buffer_size.to_string()));
            }
        }
        TraceMsgType::SessionInfo => {
            if let Ok(info) = crate::proto::trace::TraceSessionInfo::decode(payload) {
                fields.push(("pod_id".into(), info.pod_id.to_string()));
                fields.push(("events".into(), info.event_count.to_string()));
                fields.push(("dropped".into(), info.dropped_count.to_string()));
                fields.push(("tasks".into(), info.tasks.len().to_string()));
            }
        }
        TraceMsgType::Data => {
            if let Ok(chunk) = crate::proto::trace::TraceDataChunk::decode(payload) {
                fields.push(("offset".into(), chunk.offset.to_string()));
                fields.push(("count".into(), chunk.count.to_string()));
                fields.push(("data_bytes".into(), chunk.events.len().to_string()));
            }
        }
        TraceMsgType::End => {
            if let Ok(end) = crate::proto::trace::TraceDumpComplete::decode(payload) {
                fields.push(("total_events".into(), end.total_events.to_string()));
                fields.push(("checksum".into(), format!("0x{:08X}", end.checksum)));
            }
        }
        _ => fields.extend(payload_size_field(payload)),
    }

    fields
}

fn payload_size_field(payload: &[u8]) -> Vec<(String, String)> {
    if payload.is_empty() {
        Vec::new()
    } else {
        vec![("payload_bytes".into(), payload.len().to_string())]
    }
}

/// Display a decoded frame
fn display_frame(frame: &DecodedFrame, format: OutputFormat) {
    match format {
        OutputFormat::Pretty => display_pretty(frame),
        OutputFormat::Raw => display_raw(frame),
        OutputFormat::Json => display_json(frame),
    }
}

fn display_pretty(frame: &DecodedFrame) {
    let ts = frame.timestamp.as_secs_f64();
    let dir_arrow = match frame.direction {
        "host->dev" => ">>>",
        "dev->host" => "<<<",
        _ => "???",
    };

    print!(
        "[{:>10.3}s] {} {:<24} [{:>5}] {:>3}B",
        ts, dir_arrow, frame.msg_name, frame.protocol, frame.payload_size,
    );

    if !frame.decoded_fields.is_empty() {
        print!("  |");
        for (k, v) in &frame.decoded_fields {
            print!(" {}={}", k, v);
        }
    }

    println!();
}

fn display_raw(frame: &DecodedFrame) {
    let ts = frame.timestamp.as_secs_f64();
    println!(
        "[{:>10.3}s] 0x{:02X} {} ({} bytes):",
        ts, frame.msg_type, frame.msg_name, frame.payload_size,
    );
    if !frame.raw_payload.is_empty() {
        // Hex dump in 16-byte rows
        for (i, chunk) in frame.raw_payload.chunks(16).enumerate() {
            let hex: Vec<String> = chunk.iter().map(|b| format!("{:02x}", b)).collect();
            let ascii: String = chunk
                .iter()
                .map(|&b| {
                    if (0x20..=0x7e).contains(&b) {
                        b as char
                    } else {
                        '.'
                    }
                })
                .collect();
            println!("  {:04x}  {:<48}  {}", i * 16, hex.join(" "), ascii);
        }
    }
    println!();
}

fn display_json(frame: &DecodedFrame) {
    let ts_us = frame.timestamp.as_micros();
    let mut fields_json = String::new();
    for (i, (k, v)) in frame.decoded_fields.iter().enumerate() {
        if i > 0 {
            fields_json.push(',');
        }
        // Escape strings for JSON
        let escaped_v = v.replace('\\', "\\\\").replace('"', "\\\"");
        fields_json.push_str(&format!("\"{}\":\"{}\"", k, escaped_v));
    }

    let raw_hex: String = frame
        .raw_payload
        .iter()
        .map(|b| format!("{:02x}", b))
        .collect();

    println!(
        "{{\"ts_us\":{},\"msg_type\":\"0x{:02X}\",\"msg_name\":\"{}\",\"direction\":\"{}\",\"protocol\":\"{}\",\"payload_size\":{},\"fields\":{{{}}},\"raw\":\"{}\"}}",
        ts_us,
        frame.msg_type,
        frame.msg_name,
        frame.direction,
        frame.protocol,
        frame.payload_size,
        fields_json,
        raw_hex,
    );
}

/// Get human-readable feature name from proto enum value
fn feature_name(feature: i32) -> String {
    crate::proto::config::Feature::try_from(feature)
        .map(|f| f.cli_name().to_string())
        .unwrap_or_else(|_| format!("feature({})", feature))
}

/// Get human-readable mode name from proto enum value
fn mode_name(mode: i32) -> String {
    crate::proto::config::SystemMode::try_from(mode)
        .map(|m| m.cli_name().to_string())
        .unwrap_or_else(|_| format!("mode({})", mode))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::transport::frame::encode_frame;

    #[test]
    fn test_protocol_filter_matches() {
        assert!(ProtocolFilter::Config.matches(0x20));
        assert!(ProtocolFilter::Config.matches(0x3F));
        assert!(ProtocolFilter::Config.matches(0x40));
        assert!(ProtocolFilter::Config.matches(0x49));
        assert!(ProtocolFilter::Config.matches(0x4F));
        assert!(ProtocolFilter::Config.matches(0x50));
        assert!(!ProtocolFilter::Config.matches(0x2C));
        assert!(!ProtocolFilter::Config.matches(0x4A));
        assert!(!ProtocolFilter::Config.matches(0x10));

        assert!(ProtocolFilter::Trace.matches(0x10));
        assert!(ProtocolFilter::Trace.matches(0x1B));
        assert!(!ProtocolFilter::Trace.matches(0x1F));
        assert!(!ProtocolFilter::Trace.matches(0x20));

        assert!(ProtocolFilter::Ota.matches(0x01));
        assert!(ProtocolFilter::Ota.matches(0x05));
        assert!(!ProtocolFilter::Ota.matches(0x10));
    }

    #[test]
    fn test_identify_message() {
        let (name, dir, proto) = identify_message(0x20);
        assert_eq!(name, "LIST_FEATURES_REQ");
        assert_eq!(dir, "host->dev");
        assert_eq!(proto, "config");

        let (name, dir, proto) = identify_message(0x21);
        assert_eq!(name, "LIST_FEATURES_RSP");
        assert_eq!(dir, "dev->host");
        assert_eq!(proto, "config");

        for (msg_type, expected_name, expected_direction) in [
            (0x40, "CLEAR_CRASH_DUMP_REQ", "host->dev"),
            (0x43, "GET_MEMORY_PROFILE_RSP", "dev->host"),
            (0x49, "SET_AUTO_UPDATE_RSP", "dev->host"),
            (0x4C, "SIMULATE_TOUCH_REQ", "host->dev"),
            (0x4F, "SET_SIM_MODE_RSP", "dev->host"),
            (0x50, "TOUCH_EVENT_NTF", "dev->host"),
        ] {
            let (name, direction, protocol) = identify_message(msg_type);
            assert_eq!(name, expected_name);
            assert_eq!(direction, expected_direction);
            assert_eq!(protocol, "config");
        }

        let (name, dir, proto) = identify_message(0x10);
        assert_eq!(name, "TRACE_START");
        assert_eq!(dir, "host->dev");
        assert_eq!(proto, "trace");

        let (name, _, proto) = identify_message(0xFF);
        assert_eq!(name, "UNKNOWN_0xFF");
        assert_eq!(proto, "unknown");
    }

    #[test]
    fn test_should_display_no_filter() {
        let frame = Frame {
            msg_type: 0x20,
            payload: vec![],
        };
        assert!(should_display(&frame, &[]));
    }

    #[test]
    fn test_should_display_with_filter() {
        let frame = Frame {
            msg_type: 0x20,
            payload: vec![],
        };
        assert!(should_display(&frame, &[ProtocolFilter::Config]));
        assert!(!should_display(&frame, &[ProtocolFilter::Trace]));
        assert!(should_display(
            &frame,
            &[ProtocolFilter::Trace, ProtocolFilter::Config]
        ));
    }

    #[test]
    fn test_decode_list_features_rsp() {
        use crate::proto::config::{FeatureState, ListFeaturesResponse};

        let resp = ListFeaturesResponse {
            features: vec![
                FeatureState {
                    feature: 1, // LED_EFFECTS
                    enabled: true,
                },
                FeatureState {
                    feature: 3, // WIFI
                    enabled: false,
                },
            ],
            pod_id: 1,
        };
        let payload = resp.encode_to_vec();

        let fields = decode_payload(0x21, &payload);
        assert!(fields.iter().any(|(k, v)| k == "pod_id" && v == "1"));
        assert!(fields
            .iter()
            .any(|(k, v)| k == "led-effects" && v == "enabled"));
        assert!(fields.iter().any(|(k, v)| k == "wifi" && v == "disabled"));
    }

    #[test]
    fn test_decode_set_feature_req() {
        use crate::proto::config::SetFeatureRequest;

        let req = SetFeatureRequest {
            feature: 1,
            enabled: true,
        };
        let payload = req.encode_to_vec();

        let fields = decode_payload(0x22, &payload);
        assert!(fields
            .iter()
            .any(|(k, v)| k == "feature" && v == "led-effects"));
        assert!(fields.iter().any(|(k, v)| k == "enabled" && v == "true"));
    }

    #[test]
    fn test_decode_get_feature_contract() {
        use crate::proto::config::{Feature, FeatureState, GetFeatureRequest, GetFeatureResponse};

        let request = GetFeatureRequest {
            feature: Feature::Wifi as i32,
        };
        let request_fields = decode_payload(
            ConfigMsgType::GetFeatureReq.as_u8(),
            &request.encode_to_vec(),
        );
        assert!(request_fields
            .iter()
            .any(|(key, value)| key == "feature" && value == "wifi"));

        let response = GetFeatureResponse {
            feature: Some(FeatureState {
                feature: Feature::Wifi as i32,
                enabled: true,
            }),
        };
        let mut payload = vec![crate::proto::config::Status::Ok as u8];
        payload.extend(response.encode_to_vec());
        let response_fields = decode_payload(ConfigMsgType::GetFeatureRsp.as_u8(), &payload);
        assert!(response_fields
            .iter()
            .any(|(key, value)| key == "feature" && value == "wifi"));
        assert!(response_fields
            .iter()
            .any(|(key, value)| key == "enabled" && value == "true"));
    }

    #[test]
    fn test_decode_latest_config_response_with_status_prefix() {
        use crate::proto::config::{SetSimModeResponse, Status};

        let response = SetSimModeResponse {
            enabled: true,
            delay_ms: 250,
            pad_index: 3,
        };
        let mut payload = vec![Status::Ok as u8];
        payload.extend(response.encode_to_vec());

        let fields = decode_payload(0x4F, &payload);
        assert!(fields
            .iter()
            .any(|(key, value)| key == "status" && value == "STATUS_OK"));
        assert!(fields
            .iter()
            .any(|(key, value)| key == "enabled" && value == "true"));
        assert!(fields
            .iter()
            .any(|(key, value)| key == "delay_ms" && value == "250"));
        assert!(fields
            .iter()
            .any(|(key, value)| key == "pad_index" && value == "3"));
    }

    #[test]
    fn test_decode_touch_event_notification_without_status_prefix() {
        use crate::proto::config::TouchEventNotification;

        let event = TouchEventNotification {
            pod_id: 2,
            pad_index: 3,
            timestamp_us: 1_234_567,
        };
        let fields = decode_payload(0x50, &event.encode_to_vec());

        assert!(fields
            .iter()
            .any(|(key, value)| key == "pod_id" && value == "2"));
        assert!(fields
            .iter()
            .any(|(key, value)| key == "pad_index" && value == "3"));
        assert!(fields
            .iter()
            .any(|(key, value)| key == "timestamp_us" && value == "1234567"));
        assert!(!fields.iter().any(|(key, _)| key == "status"));
    }

    #[test]
    fn test_decode_config_error_status_stops_before_protobuf() {
        use crate::proto::config::Status;

        let fields = decode_payload(0x4F, &[Status::Busy as u8]);
        assert_eq!(fields, vec![("status".into(), "STATUS_BUSY".into())]);
    }

    #[test]
    fn test_decode_trace_status_resp() {
        use crate::proto::trace::TraceStatusResponse;

        let resp = TraceStatusResponse {
            initialized: true,
            enabled: true,
            streaming: false,
            event_count: 42,
            dropped_count: 0,
            buffer_size: 32768,
            stream_category_mask: 0,
        };
        let payload = resp.encode_to_vec();

        let fields = decode_payload(0x17, &payload);
        assert!(fields
            .iter()
            .any(|(k, v)| k == "initialized" && v == "true"));
        assert!(fields.iter().any(|(k, v)| k == "events" && v == "42"));
    }

    #[test]
    fn test_decode_frame_round_trip() {
        // Encode a frame, feed it to decoder, and verify decode
        let payload = vec![0x08, 0x01, 0x10, 0x01]; // protobuf for SetFeatureRequest
        let encoded = encode_frame(0x22, &payload).unwrap();

        let mut decoder = FrameDecoder::new();
        let mut result = None;
        for byte in encoded {
            if let Some(r) = decoder.feed_byte(byte) {
                result = Some(r);
            }
        }

        let frame = result.unwrap().unwrap();
        assert_eq!(frame.msg_type, 0x22);

        let decoded = decode_frame(Duration::from_millis(100), &frame);
        assert_eq!(decoded.msg_name, "SET_FEATURE_REQ");
        assert_eq!(decoded.protocol, "config");
    }

    #[test]
    fn test_json_output_format() {
        let frame = DecodedFrame {
            timestamp: Duration::from_millis(1234),
            msg_type: 0x21,
            msg_name: "LIST_FEATURES_RSP".into(),
            direction: "dev->host",
            protocol: "config",
            payload_size: 10,
            decoded_fields: vec![("pod_id".into(), "1".into())],
            raw_payload: vec![0x08, 0x01],
        };

        // Capture JSON output
        let ts_us = frame.timestamp.as_micros();
        assert_eq!(ts_us, 1234000);
    }

    #[test]
    fn test_feature_name() {
        assert_eq!(feature_name(1), "led-effects");
        assert_eq!(feature_name(3), "wifi");
        assert_eq!(feature_name(99), "feature(99)");
    }

    #[test]
    fn test_mode_name() {
        assert_eq!(mode_name(0), "booting");
        assert_eq!(mode_name(1), "idle");
        assert_eq!(mode_name(99), "mode(99)");
    }
}
