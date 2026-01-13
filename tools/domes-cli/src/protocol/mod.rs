//! Protocol definitions for DOMES CLI
//!
//! Wire format handling for communication with firmware.
//! Uses protobuf encoding via prost for message payloads.
//!
//! IMPORTANT: Types (Feature, Status) come from proto::config module,
//! generated from firmware/common/proto/config.proto.

use crate::proto::config::{
    Feature, GetRgbPatternResponse, ListFeaturesResponse, ListRgbPatternsResponse, RgbColor,
    RgbPattern, RgbPatternInfo, SetFeatureRequest, SetFeatureResponse, SetRgbPatternRequest,
    SetRgbPatternResponse, Status,
};
use prost::Message;
use thiserror::Error;

/// Config protocol message types (0x20-0x2F range)
/// These are frame-level identifiers, separate from protobuf encoding.
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ConfigMsgType {
    // Feature control (0x20-0x2F)
    ListFeaturesReq = 0x20,
    ListFeaturesRsp = 0x21,
    SetFeatureReq = 0x22,
    SetFeatureRsp = 0x23,
    GetFeatureReq = 0x24,
    GetFeatureRsp = 0x25,

    // RGB LED pattern control (0x30-0x3F)
    SetRgbPatternReq = 0x30,
    SetRgbPatternRsp = 0x31,
    GetRgbPatternReq = 0x32,
    GetRgbPatternRsp = 0x33,
    ListRgbPatternsReq = 0x34,
    ListRgbPatternsRsp = 0x35,
}

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
            0x30 => Ok(Self::SetRgbPatternReq),
            0x31 => Ok(Self::SetRgbPatternRsp),
            0x32 => Ok(Self::GetRgbPatternReq),
            0x33 => Ok(Self::GetRgbPatternRsp),
            0x34 => Ok(Self::ListRgbPatternsReq),
            0x35 => Ok(Self::ListRgbPatternsRsp),
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

    #[error("Unknown RGB pattern ID: {0}")]
    UnknownRgbPattern(i32),

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

// =============================================================================
// RGB Pattern Protocol
// =============================================================================

/// RGB pattern state for CLI use
#[derive(Debug, Clone)]
pub struct CliRgbPatternState {
    pub pattern: RgbPattern,
    pub primary_color: (u8, u8, u8),
    pub speed_ms: u32,
    pub brightness: u8,
}

/// RGB pattern info for listing
#[derive(Debug, Clone)]
pub struct CliRgbPatternInfo {
    pub pattern: RgbPattern,
    pub name: String,
    pub description: String,
}

/// Serialize SetRgbPatternRequest using protobuf encoding
pub fn serialize_set_rgb_pattern(
    pattern: RgbPattern,
    color: Option<(u8, u8, u8)>,
    speed_ms: Option<u32>,
    brightness: Option<u8>,
) -> Vec<u8> {
    let req = SetRgbPatternRequest {
        pattern: pattern as i32,
        primary_color: color.map(|(r, g, b)| RgbColor {
            r: r as u32,
            g: g as u32,
            b: b as u32,
        }),
        speed_ms: speed_ms.unwrap_or(50),
        brightness: brightness.unwrap_or(128) as u32,
    };
    req.encode_to_vec()
}

/// Parse SetRgbPatternResponse payload
pub fn parse_set_rgb_pattern_response(payload: &[u8]) -> Result<RgbPattern, ProtocolError> {
    let resp = SetRgbPatternResponse::decode(payload)?;
    RgbPattern::try_from(resp.active_pattern)
        .map_err(|_| ProtocolError::UnknownRgbPattern(resp.active_pattern))
}

/// Parse GetRgbPatternResponse payload
pub fn parse_get_rgb_pattern_response(payload: &[u8]) -> Result<CliRgbPatternState, ProtocolError> {
    let resp = GetRgbPatternResponse::decode(payload)?;

    let pattern = RgbPattern::try_from(resp.active_pattern)
        .map_err(|_| ProtocolError::UnknownRgbPattern(resp.active_pattern))?;

    let primary_color = resp
        .primary_color
        .map(|c| (c.r as u8, c.g as u8, c.b as u8))
        .unwrap_or((255, 0, 0));

    Ok(CliRgbPatternState {
        pattern,
        primary_color,
        speed_ms: resp.speed_ms,
        brightness: resp.brightness as u8,
    })
}

/// Parse ListRgbPatternsResponse payload
pub fn parse_list_rgb_patterns_response(
    payload: &[u8],
) -> Result<Vec<CliRgbPatternInfo>, ProtocolError> {
    let resp = ListRgbPatternsResponse::decode(payload)?;

    let mut patterns = Vec::with_capacity(resp.patterns.len());
    for info in resp.patterns {
        let pattern = RgbPattern::try_from(info.pattern)
            .map_err(|_| ProtocolError::UnknownRgbPattern(info.pattern))?;
        patterns.push(CliRgbPatternInfo {
            pattern,
            name: info.name,
            description: info.description,
        });
    }

    Ok(patterns)
}
