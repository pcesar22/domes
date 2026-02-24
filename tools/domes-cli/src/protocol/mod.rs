//! Protocol definitions for DOMES CLI
//!
//! Wire format handling for communication with firmware.
//! Uses protobuf encoding via prost for message payloads.
//!
//! IMPORTANT: All types come from proto modules, generated from
//! firmware/common/proto/*.proto. DO NOT hand-roll protocol types here.

use crate::proto::config::{
    Color, EspNowBenchRequest, EspNowBenchResponse, Feature, GetEspNowStatusResponse,
    GetHealthResponse, GetLedPatternResponse, GetModeResponse, GetSystemInfoResponse, LedPattern,
    LedPatternType, ListFeaturesResponse, SetFeatureRequest, SetFeatureResponse,
    SetImuTriageRequest, SetImuTriageResponse, SetLedPatternRequest, SetLedPatternResponse,
    SetModeRequest, SetModeResponse, SetPodIdRequest, SetPodIdResponse, Status, SystemMode,
};
use prost::Message;
use thiserror::Error;

// Re-export config MsgType with clearer name for use in commands
pub use crate::proto::config::MsgType as ConfigMsgType;

impl TryFrom<u8> for ConfigMsgType {
    type Error = ProtocolError;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0x20 => Ok(Self::ListFeaturesReq),
            0x21 => Ok(Self::ListFeaturesRsp),
            0x22 => Ok(Self::SetFeatureReq),
            0x23 => Ok(Self::SetFeatureRsp),
            0x24 => Ok(Self::GetFeatureReq),
            0x25 => Ok(Self::GetFeatureRsp),
            0x26 => Ok(Self::SetLedPatternReq),
            0x27 => Ok(Self::SetLedPatternRsp),
            0x28 => Ok(Self::GetLedPatternReq),
            0x29 => Ok(Self::GetLedPatternRsp),
            0x2A => Ok(Self::SetImuTriageReq),
            0x2B => Ok(Self::SetImuTriageRsp),
            0x30 => Ok(Self::GetModeReq),
            0x31 => Ok(Self::GetModeRsp),
            0x32 => Ok(Self::SetModeReq),
            0x33 => Ok(Self::SetModeRsp),
            0x34 => Ok(Self::GetSystemInfoReq),
            0x35 => Ok(Self::GetSystemInfoRsp),
            0x36 => Ok(Self::SetPodIdReq),
            0x37 => Ok(Self::SetPodIdRsp),
            0x38 => Ok(Self::GetHealthReq),
            0x39 => Ok(Self::GetHealthRsp),
            0x3A => Ok(Self::GetEspnowStatusReq),
            0x3B => Ok(Self::GetEspnowStatusRsp),
            0x3C => Ok(Self::EspnowBenchReq),
            0x3D => Ok(Self::EspnowBenchRsp),
            _ => Err(ProtocolError::UnknownMessageType(value)),
        }
    }
}

/// Protocol errors
#[derive(Debug, Error)]
pub enum ProtocolError {
    #[error("Unknown message type: 0x{0:02X}")]
    UnknownMessageType(u8),

    #[error("Unknown feature ID: {0}")]
    UnknownFeature(i32),

    #[error("Unknown status code: {0}")]
    UnknownStatus(i32),

    #[error("Payload too short: expected {expected}, got {actual}")]
    PayloadTooShort { expected: usize, actual: usize },

    #[error("Device returned error: {0:?}")]
    DeviceError(Status),

    #[error("Protobuf decode error: {0}")]
    DecodeError(#[from] prost::DecodeError),
}

/// Feature state for CLI use
#[derive(Debug, Clone, Copy)]
pub struct CliFeatureState {
    pub feature: Feature,
    pub enabled: bool,
}

/// Serialize SetFeatureRequest using protobuf encoding
pub fn serialize_set_feature(feature: Feature, enabled: bool) -> Vec<u8> {
    let req = SetFeatureRequest {
        feature: feature as i32,
        enabled,
    };
    req.encode_to_vec()
}

/// Parse ListFeaturesResponse payload (protobuf encoded)
pub fn parse_list_features_response(payload: &[u8]) -> Result<Vec<CliFeatureState>, ProtocolError> {
    let resp = ListFeaturesResponse::decode(payload)?;

    let mut features = Vec::with_capacity(resp.features.len());
    for fs in resp.features {
        let feature = Feature::try_from(fs.feature)
            .map_err(|_| ProtocolError::UnknownFeature(fs.feature))?;
        features.push(CliFeatureState {
            feature,
            enabled: fs.enabled,
        });
    }

    Ok(features)
}

/// Parse SetFeatureResponse or GetFeatureResponse payload
/// Format: [status_byte][protobuf_SetFeatureResponse]
pub fn parse_feature_response(payload: &[u8]) -> Result<CliFeatureState, ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    // First byte is status
    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    // Rest is protobuf-encoded SetFeatureResponse
    let resp = SetFeatureResponse::decode(&payload[1..])?;

    let fs = resp.feature.ok_or(ProtocolError::PayloadTooShort {
        expected: 3,
        actual: payload.len(),
    })?;

    let feature =
        Feature::try_from(fs.feature).map_err(|_| ProtocolError::UnknownFeature(fs.feature))?;

    Ok(CliFeatureState {
        feature,
        enabled: fs.enabled,
    })
}

/// LED pattern state for CLI use
#[derive(Debug, Clone)]
pub struct CliLedPattern {
    pub pattern_type: LedPatternType,
    pub color: Option<(u8, u8, u8, u8)>, // RGBW
    pub colors: Vec<(u8, u8, u8, u8)>,   // Color list for cycles
    pub period_ms: u32,
    pub brightness: u8,
}

impl Default for CliLedPattern {
    fn default() -> Self {
        Self {
            pattern_type: LedPatternType::LedPatternOff,
            color: None,
            colors: Vec::new(),
            period_ms: 2000,
            brightness: 128,
        }
    }
}

impl CliLedPattern {
    /// Create a solid color pattern
    pub fn solid(r: u8, g: u8, b: u8) -> Self {
        Self {
            pattern_type: LedPatternType::LedPatternSolid,
            color: Some((r, g, b, 0)),
            ..Default::default()
        }
    }

    /// Create a breathing pattern
    pub fn breathing(r: u8, g: u8, b: u8, period_ms: u32) -> Self {
        Self {
            pattern_type: LedPatternType::LedPatternBreathing,
            color: Some((r, g, b, 0)),
            period_ms,
            ..Default::default()
        }
    }

    /// Create a color cycle pattern
    pub fn color_cycle(colors: Vec<(u8, u8, u8, u8)>, period_ms: u32) -> Self {
        Self {
            pattern_type: LedPatternType::LedPatternColorCycle,
            colors,
            period_ms,
            ..Default::default()
        }
    }

    /// Turn LEDs off
    pub fn off() -> Self {
        Self {
            pattern_type: LedPatternType::LedPatternOff,
            ..Default::default()
        }
    }
}

/// Serialize SetLedPatternRequest using protobuf encoding
pub fn serialize_set_led_pattern(pattern: &CliLedPattern) -> Vec<u8> {
    let req = SetLedPatternRequest {
        pattern: Some(LedPattern {
            r#type: pattern.pattern_type as i32,
            color: pattern.color.map(|(r, g, b, w)| Color {
                r: r as u32,
                g: g as u32,
                b: b as u32,
                w: w as u32,
            }),
            colors: pattern
                .colors
                .iter()
                .map(|(r, g, b, w)| Color {
                    r: *r as u32,
                    g: *g as u32,
                    b: *b as u32,
                    w: *w as u32,
                })
                .collect(),
            period_ms: pattern.period_ms,
            brightness: pattern.brightness as u32,
        }),
    };
    req.encode_to_vec()
}

/// Parse SetLedPatternResponse or GetLedPatternResponse payload
/// Format: [status_byte][protobuf_response]
pub fn parse_led_pattern_response(payload: &[u8]) -> Result<CliLedPattern, ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    // First byte is status
    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    // Try to decode as SetLedPatternResponse first
    let pattern = if let Ok(resp) = SetLedPatternResponse::decode(&payload[1..]) {
        resp.pattern
    } else if let Ok(resp) = GetLedPatternResponse::decode(&payload[1..]) {
        resp.pattern
    } else {
        return Err(ProtocolError::PayloadTooShort {
            expected: 2,
            actual: payload.len(),
        });
    };

    let pattern = pattern.ok_or(ProtocolError::PayloadTooShort {
        expected: 3,
        actual: payload.len(),
    })?;

    let pattern_type = LedPatternType::try_from(pattern.r#type)
        .unwrap_or(LedPatternType::LedPatternOff);

    let color = pattern.color.map(|c| {
        (
            c.r as u8,
            c.g as u8,
            c.b as u8,
            c.w as u8,
        )
    });

    let colors: Vec<_> = pattern
        .colors
        .iter()
        .map(|c| (c.r as u8, c.g as u8, c.b as u8, c.w as u8))
        .collect();

    Ok(CliLedPattern {
        pattern_type,
        color,
        colors,
        period_ms: pattern.period_ms,
        brightness: pattern.brightness as u8,
    })
}

/// Serialize SetImuTriageRequest using protobuf encoding
pub fn serialize_set_imu_triage(enabled: bool) -> Vec<u8> {
    let req = SetImuTriageRequest { enabled };
    req.encode_to_vec()
}

/// Parse SetImuTriageResponse payload
/// Format: [status_byte][protobuf_SetImuTriageResponse]
pub fn parse_imu_triage_response(payload: &[u8]) -> Result<bool, ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    // First byte is status
    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    // Rest is protobuf-encoded SetImuTriageResponse
    let resp = SetImuTriageResponse::decode(&payload[1..])?;

    Ok(resp.enabled)
}

/// System mode info for CLI use
#[derive(Debug, Clone)]
pub struct CliModeInfo {
    pub mode: SystemMode,
    pub time_in_mode_ms: u32,
}

/// System info for CLI use
#[derive(Debug, Clone)]
pub struct CliSystemInfo {
    pub firmware_version: String,
    pub uptime_s: u32,
    pub free_heap: u32,
    pub boot_count: u32,
    pub mode: SystemMode,
    pub feature_mask: u32,
    pub pod_id: u32,
}

/// Serialize SetModeRequest using protobuf encoding
pub fn serialize_set_mode(mode: SystemMode) -> Vec<u8> {
    let req = SetModeRequest {
        mode: mode as i32,
    };
    req.encode_to_vec()
}

/// Parse GetModeResponse payload
/// Format: [status_byte][protobuf_GetModeResponse]
pub fn parse_get_mode_response(payload: &[u8]) -> Result<CliModeInfo, ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let resp = GetModeResponse::decode(&payload[1..])?;
    let mode = SystemMode::try_from(resp.mode).unwrap_or(SystemMode::Booting);

    Ok(CliModeInfo {
        mode,
        time_in_mode_ms: resp.time_in_mode_ms,
    })
}

/// Parse SetModeResponse payload
/// Format: [status_byte][protobuf_SetModeResponse]
pub fn parse_set_mode_response(payload: &[u8]) -> Result<(SystemMode, bool), ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let resp = SetModeResponse::decode(&payload[1..])?;
    let mode = SystemMode::try_from(resp.mode).unwrap_or(SystemMode::Booting);

    Ok((mode, resp.transition_ok))
}

/// Parse GetSystemInfoResponse payload
/// Format: [status_byte][protobuf_GetSystemInfoResponse]
pub fn parse_get_system_info_response(payload: &[u8]) -> Result<CliSystemInfo, ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let resp = GetSystemInfoResponse::decode(&payload[1..])?;
    let mode = SystemMode::try_from(resp.mode).unwrap_or(SystemMode::Booting);

    Ok(CliSystemInfo {
        firmware_version: resp.firmware_version,
        uptime_s: resp.uptime_s,
        free_heap: resp.free_heap,
        boot_count: resp.boot_count,
        mode,
        feature_mask: resp.feature_mask,
        pod_id: resp.pod_id,
    })
}

/// Serialize SetPodIdRequest
pub fn serialize_set_pod_id(pod_id: u32) -> Vec<u8> {
    let req = SetPodIdRequest { pod_id };
    req.encode_to_vec()
}

/// Parse SetPodIdResponse payload
/// Format: [status_byte][protobuf_SetPodIdResponse]
pub fn parse_set_pod_id_response(payload: &[u8]) -> Result<u32, ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let resp = SetPodIdResponse::decode(&payload[1..])?;
    Ok(resp.pod_id)
}

// ============================================================================
// Observability types and parsers
// ============================================================================

/// Task health info for CLI use
#[derive(Debug, Clone)]
pub struct CliTaskHealth {
    pub name: String,
    pub stack_high_water: u32,
    pub priority: u32,
    pub core: u32,
}

/// System health info for CLI use
#[derive(Debug, Clone)]
pub struct CliHealthInfo {
    pub free_heap: u32,
    pub min_free_heap: u32,
    pub uptime_seconds: u32,
    pub wifi_rssi: i32,
    pub tasks: Vec<CliTaskHealth>,
}

/// ESP-NOW peer info for CLI use
#[derive(Debug, Clone)]
pub struct CliEspNowPeer {
    pub mac: [u8; 6],
    pub rssi: i32,
    pub last_seen_ms: u32,
}

/// ESP-NOW status for CLI use
#[derive(Debug, Clone)]
pub struct CliEspNowStatus {
    pub peer_count: u32,
    pub channel: u32,
    pub tx_count: u32,
    pub rx_count: u32,
    pub tx_fail_count: u32,
    pub last_rtt_us: u32,
    pub discovery_state: String,
    pub peers: Vec<CliEspNowPeer>,
}

/// Parse GetHealthResponse payload
/// Format: [status_byte][protobuf_GetHealthResponse]
pub fn parse_get_health_response(payload: &[u8]) -> Result<CliHealthInfo, ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let resp = GetHealthResponse::decode(&payload[1..])?;

    let tasks = resp
        .tasks
        .iter()
        .map(|t| CliTaskHealth {
            name: t.name.clone(),
            stack_high_water: t.stack_high_water,
            priority: t.priority,
            core: t.core,
        })
        .collect();

    Ok(CliHealthInfo {
        free_heap: resp.free_heap,
        min_free_heap: resp.min_free_heap,
        uptime_seconds: resp.uptime_seconds,
        wifi_rssi: resp.wifi_rssi,
        tasks,
    })
}

/// Parse GetEspNowStatusResponse payload
/// Format: [status_byte][protobuf_GetEspNowStatusResponse]
pub fn parse_get_espnow_status_response(
    payload: &[u8],
) -> Result<CliEspNowStatus, ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let resp = GetEspNowStatusResponse::decode(&payload[1..])?;

    let peers = resp
        .peers
        .iter()
        .map(|p| {
            let mut mac = [0u8; 6];
            let len = p.mac.len().min(6);
            mac[..len].copy_from_slice(&p.mac[..len]);
            CliEspNowPeer {
                mac,
                rssi: p.rssi,
                last_seen_ms: p.last_seen_ms,
            }
        })
        .collect();

    Ok(CliEspNowStatus {
        peer_count: resp.peer_count,
        channel: resp.channel,
        tx_count: resp.tx_count,
        rx_count: resp.rx_count,
        tx_fail_count: resp.tx_fail_count,
        last_rtt_us: resp.last_rtt_us,
        discovery_state: resp.discovery_state,
        peers,
    })
}

/// ESP-NOW benchmark results for CLI use
#[derive(Debug, Clone)]
pub struct CliBenchResult {
    pub rounds_completed: u32,
    pub rounds_failed: u32,
    pub min_rtt_us: u32,
    pub max_rtt_us: u32,
    pub mean_rtt_us: u32,
    pub p50_rtt_us: u32,
    pub p95_rtt_us: u32,
    pub p99_rtt_us: u32,
}

/// Serialize EspNowBenchRequest
pub fn serialize_espnow_bench(rounds: u32) -> Vec<u8> {
    let req = EspNowBenchRequest { rounds };
    req.encode_to_vec()
}

/// Parse EspNowBenchResponse payload
/// Format: [status_byte][protobuf_EspNowBenchResponse]
pub fn parse_espnow_bench_response(payload: &[u8]) -> Result<CliBenchResult, ProtocolError> {
    if payload.is_empty() {
        return Err(ProtocolError::PayloadTooShort {
            expected: 1,
            actual: 0,
        });
    }

    let status_val = payload[0] as i32;
    let status =
        Status::try_from(status_val).map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let resp = EspNowBenchResponse::decode(&payload[1..])?;

    Ok(CliBenchResult {
        rounds_completed: resp.rounds_completed,
        rounds_failed: resp.rounds_failed,
        min_rtt_us: resp.min_rtt_us,
        max_rtt_us: resp.max_rtt_us,
        mean_rtt_us: resp.mean_rtt_us,
        p50_rtt_us: resp.p50_rtt_us,
        p95_rtt_us: resp.p95_rtt_us,
        p99_rtt_us: resp.p99_rtt_us,
    })
}
