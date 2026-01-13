//! Protocol definitions for DOMES CLI
//!
//! Matches the wire protocol defined in firmware/domes/main/config/configProtocol.hpp

use thiserror::Error;

/// Config protocol message types (0x20-0x2F range)
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

/// Runtime-toggleable features
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Feature {
    Unknown = 0,
    LedEffects = 1,
    BleAdvertising = 2,
    Wifi = 3,
    EspNow = 4,
    Touch = 5,
    Haptic = 6,
    Audio = 7,
}

impl Feature {
    /// Get human-readable name for the feature
    pub fn name(&self) -> &'static str {
        match self {
            Feature::Unknown => "unknown",
            Feature::LedEffects => "led-effects",
            Feature::BleAdvertising => "ble",
            Feature::Wifi => "wifi",
            Feature::EspNow => "esp-now",
            Feature::Touch => "touch",
            Feature::Haptic => "haptic",
            Feature::Audio => "audio",
        }
    }

    /// Get all valid features (excluding Unknown)
    pub fn all() -> &'static [Feature] {
        &[
            Feature::LedEffects,
            Feature::BleAdvertising,
            Feature::Wifi,
            Feature::EspNow,
            Feature::Touch,
            Feature::Haptic,
            Feature::Audio,
        ]
    }
}

impl TryFrom<u8> for Feature {
    type Error = ProtocolError;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0 => Ok(Self::Unknown),
            1 => Ok(Self::LedEffects),
            2 => Ok(Self::BleAdvertising),
            3 => Ok(Self::Wifi),
            4 => Ok(Self::EspNow),
            5 => Ok(Self::Touch),
            6 => Ok(Self::Haptic),
            7 => Ok(Self::Audio),
            _ => Err(ProtocolError::UnknownFeature(value)),
        }
    }
}

impl std::str::FromStr for Feature {
    type Err = ProtocolError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "led-effects" | "led" | "leds" => Ok(Feature::LedEffects),
            "ble" | "bluetooth" | "ble-advertising" => Ok(Feature::BleAdvertising),
            "wifi" | "wi-fi" => Ok(Feature::Wifi),
            "esp-now" | "espnow" => Ok(Feature::EspNow),
            "touch" => Ok(Feature::Touch),
            "haptic" | "haptics" => Ok(Feature::Haptic),
            "audio" | "sound" => Ok(Feature::Audio),
            _ => Err(ProtocolError::InvalidFeatureName(s.to_string())),
        }
    }
}

/// Config status codes
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ConfigStatus {
    Ok = 0x00,
    Error = 0x01,
    InvalidFeature = 0x02,
    Busy = 0x03,
}

impl TryFrom<u8> for ConfigStatus {
    type Error = ProtocolError;

    fn try_from(value: u8) -> Result<Self, <Self as TryFrom<u8>>::Error> {
        match value {
            0x00 => Ok(Self::Ok),
            0x01 => Ok(Self::Error),
            0x02 => Ok(Self::InvalidFeature),
            0x03 => Ok(Self::Busy),
            _ => Err(ProtocolError::UnknownStatus(value)),
        }
    }
}

/// Protocol errors
#[derive(Debug, Error)]
pub enum ProtocolError {
    #[error("Unknown message type: 0x{0:02X}")]
    UnknownMessageType(u8),

    #[error("Unknown feature ID: {0}")]
    UnknownFeature(u8),

    #[error("Unknown status code: 0x{0:02X}")]
    UnknownStatus(u8),

    #[error("Invalid feature name: {0}")]
    InvalidFeatureName(String),

    #[error("Payload too short: expected {expected}, got {actual}")]
    PayloadTooShort { expected: usize, actual: usize },

    #[error("Device returned error: {0:?}")]
    DeviceError(ConfigStatus),
}

/// Feature state (feature ID + enabled flag)
#[derive(Debug, Clone, Copy)]
pub struct FeatureState {
    pub feature: Feature,
    pub enabled: bool,
}

/// Serialize SetFeatureRequest payload
pub fn serialize_set_feature(feature: Feature, enabled: bool) -> [u8; 2] {
    [feature as u8, if enabled { 1 } else { 0 }]
}

/// Serialize GetFeatureRequest payload
pub fn serialize_get_feature(feature: Feature) -> [u8; 1] {
    [feature as u8]
}

/// Parse ListFeaturesResponse payload
pub fn parse_list_features_response(payload: &[u8]) -> Result<Vec<FeatureState>, ProtocolError> {
    if payload.len() < 2 {
        return Err(ProtocolError::PayloadTooShort {
            expected: 2,
            actual: payload.len(),
        });
    }

    let status = ConfigStatus::try_from(payload[0])?;
    if status != ConfigStatus::Ok {
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
        let feature = Feature::try_from(payload[offset])?;
        let enabled = payload[offset + 1] != 0;
        features.push(FeatureState { feature, enabled });
    }

    Ok(features)
}

/// Parse SetFeatureResponse or GetFeatureResponse payload
pub fn parse_feature_response(payload: &[u8]) -> Result<FeatureState, ProtocolError> {
    if payload.len() < 3 {
        return Err(ProtocolError::PayloadTooShort {
            expected: 3,
            actual: payload.len(),
        });
    }

    let status = ConfigStatus::try_from(payload[0])?;
    if status != ConfigStatus::Ok {
        return Err(ProtocolError::DeviceError(status));
    }

    let feature = Feature::try_from(payload[1])?;
    let enabled = payload[2] != 0;

    Ok(FeatureState { feature, enabled })
}
