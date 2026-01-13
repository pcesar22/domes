//! Protocol definitions for DOMES CLI
//!
//! Wire format handling for communication with firmware.
//! Uses protobuf encoding via prost for message payloads.
//!
//! IMPORTANT: Types (Feature, Status) come from proto::config module,
//! generated from firmware/common/proto/config.proto.

use crate::proto::config::{
    Feature, ListFeaturesResponse, SetFeatureRequest, SetFeatureResponse, Status,
};
use prost::Message;
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
