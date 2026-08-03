//! Feature toggle commands

use crate::proto::config::Feature;
use crate::protocol::{
    parse_get_feature_response, parse_list_features_response, parse_set_feature_response,
    serialize_get_feature, serialize_set_feature, CliFeatureState, ConfigMsgType,
};
use crate::transport::Transport;
use anyhow::{Context, Result};

/// List all features and their current state
pub fn feature_list(transport: &mut dyn Transport) -> Result<Vec<CliFeatureState>> {
    let frame = transport
        .send_command(ConfigMsgType::ListFeaturesReq as u8, &[])
        .context("Failed to send list features command")?;

    if frame.msg_type != ConfigMsgType::ListFeaturesRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::ListFeaturesRsp as u8
        );
    }

    parse_list_features_response(&frame.payload).context("Failed to parse list features response")
}

/// Enable a feature
pub fn feature_enable(transport: &mut dyn Transport, feature: Feature) -> Result<CliFeatureState> {
    require_mutation_capability(transport, feature)?;
    let payload = serialize_set_feature(feature, true);
    let frame = transport
        .send_command(ConfigMsgType::SetFeatureReq as u8, &payload)
        .context("Failed to send set feature command")?;

    if frame.msg_type != ConfigMsgType::SetFeatureRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SetFeatureRsp as u8
        );
    }

    let state = parse_set_feature_response(&frame.payload)
        .context("Failed to parse set feature response")?;
    validate_feature_response(feature, Some(true), state)
}

/// Disable a feature
pub fn feature_disable(transport: &mut dyn Transport, feature: Feature) -> Result<CliFeatureState> {
    require_mutation_capability(transport, feature)?;
    let payload = serialize_set_feature(feature, false);
    let frame = transport
        .send_command(ConfigMsgType::SetFeatureReq as u8, &payload)
        .context("Failed to send set feature command")?;

    if frame.msg_type != ConfigMsgType::SetFeatureRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SetFeatureRsp as u8
        );
    }

    let state = parse_set_feature_response(&frame.payload)
        .context("Failed to parse set feature response")?;
    validate_feature_response(feature, Some(false), state)
}

/// Get one feature's current state.
pub fn feature_status(transport: &mut dyn Transport, feature: Feature) -> Result<CliFeatureState> {
    let payload = serialize_get_feature(feature);
    let frame = transport
        .send_command(ConfigMsgType::GetFeatureReq as u8, &payload)
        .context("Failed to send get feature command")?;

    if frame.msg_type != ConfigMsgType::GetFeatureRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::GetFeatureRsp as u8
        );
    }

    let state = parse_get_feature_response(&frame.payload)
        .context("Failed to parse get feature response")?;
    validate_feature_response(feature, None, state)
}

fn require_mutation_capability(transport: &mut dyn Transport, feature: Feature) -> Result<()> {
    if feature == Feature::Wifi {
        super::wifi::require_wifi_capability(transport)?;
    }
    Ok(())
}

fn validate_feature_response(
    requested_feature: Feature,
    expected_enabled: Option<bool>,
    state: CliFeatureState,
) -> Result<CliFeatureState> {
    if state.feature != requested_feature {
        anyhow::bail!(
            "Feature response identified '{}', expected '{}'",
            state.feature,
            requested_feature
        );
    }
    if let Some(expected_enabled) = expected_enabled {
        if state.enabled != expected_enabled {
            anyhow::bail!(
                "Feature '{}' response reported {}, expected {}",
                requested_feature,
                if state.enabled { "enabled" } else { "disabled" },
                if expected_enabled {
                    "enabled"
                } else {
                    "disabled"
                }
            );
        }
    }

    Ok(state)
}
