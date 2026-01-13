//! Protocol definitions for DOMES CLI
//!
//! Wire format handling for communication with firmware.
//!
//! IMPORTANT: Enum types (Feature, Status) come from proto::config module,
//! which is generated from firmware/common/proto/config.proto.
//! DO NOT define enums here - use the generated types.
//!
//! TODO: Switch to full protobuf encoding (prost::Message) when firmware
//! is updated to use nanopb for serialization.

use crate::proto::config::{Feature, Status};
use thiserror::Error;

/// Config protocol message types (0x20-0x2F range)
/// These are frame-level identifiers, separate from protobuf encoding.
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ConfigMsgType {
    ListFeaturesReq = 0x20,
    ListFeaturesRsp = 0x21,
    SetFeatureReq = 0x22,
    SetFeatureRsp = 0x23,
    GetFeatureReq = 0x24,
    GetFeatureRsp = 0x25,
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
}

/// Feature state (feature ID + enabled flag)
/// Uses the proto-generated Feature enum.
#[derive(Debug, Clone, Copy)]
pub struct FeatureState {
    pub feature: Feature,
    pub enabled: bool,
}

/// Serialize SetFeatureRequest payload (legacy binary format)
/// TODO: Replace with prost::Message::encode when firmware uses nanopb
pub fn serialize_set_feature(feature: Feature, enabled: bool) -> [u8; 2] {
    [feature as i32 as u8, if enabled { 1 } else { 0 }]
}

/// Parse ListFeaturesResponse payload (legacy binary format)
/// TODO: Replace with prost::Message::decode when firmware uses nanopb
pub fn parse_list_features_response(payload: &[u8]) -> Result<Vec<FeatureState>, ProtocolError> {
    if payload.len() < 2 {
        return Err(ProtocolError::PayloadTooShort {
            expected: 2,
            actual: payload.len(),
        });
    }

    let status_val = payload[0] as i32;
    let status = Status::try_from(status_val)
        .map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let count = payload[1] as usize;
    let expected_len = 2 + count * 2;

    if payload.len() < expected_len {
        return Err(ProtocolError::PayloadTooShort {
            expected: expected_len,
            actual: payload.len(),
        });
    }

    let mut features = Vec::with_capacity(count);
    for i in 0..count {
        let offset = 2 + i * 2;
        let feature_val = payload[offset] as i32;
        let feature = Feature::try_from(feature_val)
            .map_err(|_| ProtocolError::UnknownFeature(feature_val))?;
        let enabled = payload[offset + 1] != 0;
        features.push(FeatureState { feature, enabled });
    }

    Ok(features)
}

/// Parse SetFeatureResponse or GetFeatureResponse payload (legacy binary format)
/// TODO: Replace with prost::Message::decode when firmware uses nanopb
pub fn parse_feature_response(payload: &[u8]) -> Result<FeatureState, ProtocolError> {
    if payload.len() < 3 {
        return Err(ProtocolError::PayloadTooShort {
            expected: 3,
            actual: payload.len(),
        });
    }

    let status_val = payload[0] as i32;
    let status = Status::try_from(status_val)
        .map_err(|_| ProtocolError::UnknownStatus(status_val))?;

    if status != Status::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let feature_val = payload[1] as i32;
    let feature = Feature::try_from(feature_val)
        .map_err(|_| ProtocolError::UnknownFeature(feature_val))?;
    let enabled = payload[2] != 0;

    Ok(FeatureState { feature, enabled })
}
