//! Protocol sniffer command
//!
//! Captures and decodes DOMES protocol frames on any transport.
//! Prints human-readable decoded output, raw hex, or JSON lines.

use crate::transport::frame::{Frame, FrameDecoder};
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
            Self::Config => (0x20..=0x3F).contains(&msg_type),
            Self::Trace => (0x10..=0x1F).contains(&msg_type),
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
    let port = serialport::new(port_name, 115200)
        .timeout(Duration::from_millis(100))
        .open()
        .with_context(|| format!("Failed to open serial port: {}", port_name))?;

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
    match msg_type {
        // OTA messages (0x01-0x05)
        0x01 => ("OTA_BEGIN".into(), "host->dev", "ota"),
        0x02 => ("OTA_DATA".into(), "host->dev", "ota"),
        0x03 => ("OTA_END".into(), "host->dev", "ota"),
        0x04 => ("OTA_ACK".into(), "dev->host", "ota"),
        0x05 => ("OTA_ABORT".into(), "either", "ota"),

        // Trace messages (0x10-0x1F)
        0x10 => ("TRACE_START".into(), "host->dev", "trace"),
        0x11 => ("TRACE_STOP".into(), "host->dev", "trace"),
        0x12 => ("TRACE_DUMP".into(), "host->dev", "trace"),
        0x13 => ("TRACE_DATA".into(), "dev->host", "trace"),
        0x14 => ("TRACE_END".into(), "dev->host", "trace"),
        0x15 => ("TRACE_CLEAR".into(), "host->dev", "trace"),
        0x16 => ("TRACE_STATUS_REQ".into(), "host->dev", "trace"),
        0x17 => ("TRACE_STATUS_RESP".into(), "dev->host", "trace"),
        0x18 => ("TRACE_STREAM_CFG".into(), "host->dev", "trace"),
        0x19 => ("TRACE_STREAM_DATA".into(), "dev->host", "trace"),
        0x1A => ("TRACE_SESSION_INFO".into(), "dev->host", "trace"),
        0x1B => ("TRACE_ACK".into(), "dev->host", "trace"),

        // Config messages (0x20-0x3F)
        0x20 => ("LIST_FEATURES_REQ".into(), "host->dev", "config"),
        0x21 => ("LIST_FEATURES_RSP".into(), "dev->host", "config"),
        0x22 => ("SET_FEATURE_REQ".into(), "host->dev", "config"),
        0x23 => ("SET_FEATURE_RSP".into(), "dev->host", "config"),
        0x24 => ("GET_FEATURE_REQ".into(), "host->dev", "config"),
        0x25 => ("GET_FEATURE_RSP".into(), "dev->host", "config"),
        0x26 => ("SET_LED_PATTERN_REQ".into(), "host->dev", "config"),
        0x27 => ("SET_LED_PATTERN_RSP".into(), "dev->host", "config"),
        0x28 => ("GET_LED_PATTERN_REQ".into(), "host->dev", "config"),
        0x29 => ("GET_LED_PATTERN_RSP".into(), "dev->host", "config"),
        0x2A => ("SET_IMU_TRIAGE_REQ".into(), "host->dev", "config"),
        0x2B => ("SET_IMU_TRIAGE_RSP".into(), "dev->host", "config"),
        0x30 => ("GET_MODE_REQ".into(), "host->dev", "config"),
        0x31 => ("GET_MODE_RSP".into(), "dev->host", "config"),
        0x32 => ("SET_MODE_REQ".into(), "host->dev", "config"),
        0x33 => ("SET_MODE_RSP".into(), "dev->host", "config"),
        0x34 => ("GET_SYSTEM_INFO_REQ".into(), "host->dev", "config"),
        0x35 => ("GET_SYSTEM_INFO_RSP".into(), "dev->host", "config"),
        0x36 => ("SET_POD_ID_REQ".into(), "host->dev", "config"),
        0x37 => ("SET_POD_ID_RSP".into(), "dev->host", "config"),
        0x38 => ("GET_HEALTH_REQ".into(), "host->dev", "config"),
        0x39 => ("GET_HEALTH_RSP".into(), "dev->host", "config"),
        0x3A => ("GET_ESPNOW_STATUS_REQ".into(), "host->dev", "config"),
        0x3B => ("GET_ESPNOW_STATUS_RSP".into(), "dev->host", "config"),
        0x3C => ("ESPNOW_BENCH_REQ".into(), "host->dev", "config"),
        0x3D => ("ESPNOW_BENCH_RSP".into(), "dev->host", "config"),

        _ => (format!("UNKNOWN_0x{:02X}", msg_type), "unknown", "unknown"),
    }
}

/// Decode protobuf payload fields into key-value pairs
fn decode_payload(msg_type: u8, payload: &[u8]) -> Vec<(String, String)> {
    let mut fields = Vec::new();

    // Skip status byte for response messages that include one
    let (status_byte, proto_payload) = match msg_type {
        // Responses that have a leading status byte
        0x23 | 0x25 | 0x27 | 0x29 | 0x2B | 0x31 | 0x33 | 0x35 | 0x37 | 0x39 | 0x3B | 0x3D => {
            if payload.is_empty() {
                return fields;
            }
            let status_name = match payload[0] {
                0 => "OK",
                1 => "ERROR",
                2 => "INVALID_FEATURE",
                3 => "BUSY",
                4 => "INVALID_PATTERN",
                _ => "UNKNOWN",
            };
            fields.push(("status".into(), status_name.into()));
            (Some(payload[0]), &payload[1..])
        }
        _ => (None, payload),
    };

    // Don't try to decode further if status was error
    if let Some(s) = status_byte {
        if s != 0 {
            return fields;
        }
    }

    match msg_type {
        // LIST_FEATURES_RSP
        0x21 => {
            if let Ok(resp) =
                crate::proto::config::ListFeaturesResponse::decode(proto_payload)
            {
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

        // SET_FEATURE_REQ
        0x22 => {
            if let Ok(req) =
                crate::proto::config::SetFeatureRequest::decode(proto_payload)
            {
                fields.push(("feature".into(), feature_name(req.feature)));
                fields.push((
                    "enabled".into(),
                    req.enabled.to_string(),
                ));
            }
        }

        // SET_FEATURE_RSP
        0x23 => {
            if let Ok(resp) =
                crate::proto::config::SetFeatureResponse::decode(proto_payload)
            {
                if let Some(fs) = &resp.feature {
                    fields.push(("feature".into(), feature_name(fs.feature)));
                    fields.push((
                        "enabled".into(),
                        fs.enabled.to_string(),
                    ));
                }
            }
        }

        // GET_MODE_RSP
        0x31 => {
            if let Ok(resp) =
                crate::proto::config::GetModeResponse::decode(proto_payload)
            {
                fields.push(("mode".into(), mode_name(resp.mode)));
                fields.push((
                    "time_in_mode_ms".into(),
                    resp.time_in_mode_ms.to_string(),
                ));
            }
        }

        // SET_MODE_REQ
        0x32 => {
            if let Ok(req) =
                crate::proto::config::SetModeRequest::decode(proto_payload)
            {
                fields.push(("mode".into(), mode_name(req.mode)));
            }
        }

        // GET_SYSTEM_INFO_RSP
        0x35 => {
            if let Ok(resp) =
                crate::proto::config::GetSystemInfoResponse::decode(proto_payload)
            {
                fields.push(("firmware".into(), resp.firmware_version));
                fields.push(("pod_id".into(), resp.pod_id.to_string()));
                fields.push(("mode".into(), mode_name(resp.mode)));
                fields.push(("uptime_s".into(), resp.uptime_s.to_string()));
                fields.push(("free_heap".into(), resp.free_heap.to_string()));
                fields.push(("boot_count".into(), resp.boot_count.to_string()));
            }
        }

        // GET_HEALTH_RSP
        0x39 => {
            if let Ok(resp) =
                crate::proto::config::GetHealthResponse::decode(proto_payload)
            {
                fields.push(("free_heap".into(), resp.free_heap.to_string()));
                fields.push((
                    "min_free_heap".into(),
                    resp.min_free_heap.to_string(),
                ));
                fields.push((
                    "uptime_s".into(),
                    resp.uptime_seconds.to_string(),
                ));
                fields.push((
                    "tasks".into(),
                    resp.tasks.len().to_string(),
                ));
            }
        }

        // TRACE_ACK
        0x1B => {
            if let Ok(ack) =
                crate::proto::trace::AckResponse::decode(proto_payload)
            {
                let status = crate::proto::trace::Status::try_from(ack.status)
                    .map(|s| format!("{}", s))
                    .unwrap_or_else(|_| format!("unknown({})", ack.status));
                fields.push(("status".into(), status));
            }
        }

        // TRACE_STATUS_RESP
        0x17 => {
            if let Ok(resp) =
                crate::proto::trace::TraceStatusResponse::decode(proto_payload)
            {
                fields.push(("initialized".into(), resp.initialized.to_string()));
                fields.push(("enabled".into(), resp.enabled.to_string()));
                fields.push(("streaming".into(), resp.streaming.to_string()));
                fields.push(("events".into(), resp.event_count.to_string()));
                fields.push(("dropped".into(), resp.dropped_count.to_string()));
                fields.push(("buffer_size".into(), resp.buffer_size.to_string()));
            }
        }

        // TRACE_SESSION_INFO
        0x1A => {
            if let Ok(info) =
                crate::proto::trace::TraceSessionInfo::decode(proto_payload)
            {
                fields.push(("pod_id".into(), info.pod_id.to_string()));
                fields.push(("events".into(), info.event_count.to_string()));
                fields.push(("dropped".into(), info.dropped_count.to_string()));
                fields.push(("tasks".into(), info.tasks.len().to_string()));
            }
        }

        // TRACE_DATA
        0x13 => {
            if let Ok(chunk) =
                crate::proto::trace::TraceDataChunk::decode(proto_payload)
            {
                fields.push(("offset".into(), chunk.offset.to_string()));
                fields.push(("count".into(), chunk.count.to_string()));
                fields.push((
                    "data_bytes".into(),
                    chunk.events.len().to_string(),
                ));
            }
        }

        // TRACE_END
        0x14 => {
            if let Ok(end) =
                crate::proto::trace::TraceDumpComplete::decode(proto_payload)
            {
                fields.push(("total_events".into(), end.total_events.to_string()));
                fields.push((
                    "checksum".into(),
                    format!("0x{:08X}", end.checksum),
                ));
            }
        }

        // SET_LED_PATTERN_REQ
        0x26 => {
            if let Ok(req) =
                crate::proto::config::SetLedPatternRequest::decode(proto_payload)
            {
                if let Some(p) = &req.pattern {
                    let ptype = match p.r#type {
                        0 => "off",
                        1 => "solid",
                        2 => "breathing",
                        3 => "color_cycle",
                        _ => "unknown",
                    };
                    fields.push(("type".into(), ptype.into()));
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

        // GET_ESPNOW_STATUS_RSP
        0x3B => {
            if let Ok(resp) =
                crate::proto::config::GetEspNowStatusResponse::decode(proto_payload)
            {
                fields.push(("state".into(), resp.discovery_state));
                fields.push(("peers".into(), resp.peer_count.to_string()));
                fields.push(("tx".into(), resp.tx_count.to_string()));
                fields.push(("rx".into(), resp.rx_count.to_string()));
            }
        }

        _ => {
            // No specific decoder — show payload size
            if !proto_payload.is_empty() {
                fields.push((
                    "payload_bytes".into(),
                    proto_payload.len().to_string(),
                ));
            }
        }
    }

    fields
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
                .map(|&b| if (0x20..=0x7e).contains(&b) { b as char } else { '.' })
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

    let raw_hex: String = frame.raw_payload.iter().map(|b| format!("{:02x}", b)).collect();

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
    match feature {
        0 => "unknown".into(),
        1 => "led-effects".into(),
        2 => "ble".into(),
        3 => "wifi".into(),
        4 => "esp-now".into(),
        5 => "touch".into(),
        6 => "haptic".into(),
        7 => "audio".into(),
        _ => format!("feature({})", feature),
    }
}

/// Get human-readable mode name from proto enum value
fn mode_name(mode: i32) -> String {
    match mode {
        0 => "booting".into(),
        1 => "idle".into(),
        2 => "triage".into(),
        3 => "connected".into(),
        4 => "game".into(),
        5 => "error".into(),
        _ => format!("mode({})", mode),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::transport::frame::encode_frame;

    #[test]
    fn test_protocol_filter_matches() {
        assert!(ProtocolFilter::Config.matches(0x20));
        assert!(ProtocolFilter::Config.matches(0x3F));
        assert!(!ProtocolFilter::Config.matches(0x10));

        assert!(ProtocolFilter::Trace.matches(0x10));
        assert!(ProtocolFilter::Trace.matches(0x1B));
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
        assert!(fields
            .iter()
            .any(|(k, v)| k == "wifi" && v == "disabled"));
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
        assert!(fields
            .iter()
            .any(|(k, v)| k == "enabled" && v == "true"));
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
        assert!(fields
            .iter()
            .any(|(k, v)| k == "events" && v == "42"));
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
