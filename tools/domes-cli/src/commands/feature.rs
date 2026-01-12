//! Feature toggle commands

use crate::protocol::{
    parse_feature_response, parse_list_features_response, serialize_set_feature, ConfigMsgType,
    Feature, FeatureState,
};
use crate::transport::SerialTransport;
use anyhow::{Context, Result};

/// List all features and their current state
pub fn feature_list(transport: &mut SerialTransport) -> Result<Vec<FeatureState>> {
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
pub fn feature_enable(transport: &mut SerialTransport, feature: Feature) -> Result<FeatureState> {
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

    parse_feature_response(&frame.payload).context("Failed to parse set feature response")
}

/// Disable a feature
pub fn feature_disable(transport: &mut SerialTransport, feature: Feature) -> Result<FeatureState> {
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

    parse_feature_response(&frame.payload).context("Failed to parse set feature response")
}
