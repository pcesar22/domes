//! WiFi subsystem commands

use crate::proto::config::Feature;
use crate::transport::Transport;
use anyhow::Result;

/// Enable WiFi subsystem
pub fn wifi_enable(transport: &mut dyn Transport) -> Result<bool> {
    let state = super::feature_enable(transport, Feature::Wifi)?;
    Ok(state.enabled)
}

/// Disable WiFi subsystem
pub fn wifi_disable(transport: &mut dyn Transport) -> Result<bool> {
    let state = super::feature_disable(transport, Feature::Wifi)?;
    Ok(!state.enabled)
}

/// Get WiFi subsystem status
pub fn wifi_status(transport: &mut dyn Transport) -> Result<bool> {
    let features = super::feature_list(transport)?;
    let wifi_state = features
        .iter()
        .find(|f| f.feature == Feature::Wifi)
        .map(|f| f.enabled)
        .unwrap_or(false);
    Ok(wifi_state)
}
