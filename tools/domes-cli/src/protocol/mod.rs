//! Protocol definitions for DOMES CLI
//!
//! Wire format handling for communication with firmware.
//! Uses protobuf encoding via prost for message payloads.
//!
//! IMPORTANT: All types come from proto modules, generated from
//! firmware/common/proto/*.proto. DO NOT hand-roll protocol types here.

use crate::proto::config::{
    Color, Feature, GetLedPatternResponse, LedPattern, LedPatternType, ListFeaturesResponse,
    SetFeatureRequest, SetFeatureResponse, SetLedPatternRequest, SetLedPatternResponse, Status,
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
