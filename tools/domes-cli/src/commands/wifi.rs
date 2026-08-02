//! WiFi subsystem commands

use crate::proto::config::Feature;
use crate::protocol::CliFeatureState;
use crate::transport::Transport;
use anyhow::{anyhow, Result};

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
    wifi_state(&features)
}

fn wifi_state(features: &[CliFeatureState]) -> Result<bool> {
    features
        .iter()
        .find(|f| f.feature == Feature::Wifi)
        .map(|f| f.enabled)
        .ok_or_else(|| anyhow!("Device does not expose the WiFi feature flag"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn wifi_status_requires_the_feature_to_be_advertised() {
        let features = vec![CliFeatureState {
            feature: Feature::LedEffects,
            enabled: true,
        }];

        let error = wifi_state(&features).unwrap_err().to_string();
        assert!(error.contains("does not expose the WiFi feature flag"));
    }

    #[test]
    fn wifi_status_returns_the_advertised_state() {
        for enabled in [false, true] {
            let features = vec![CliFeatureState {
                feature: Feature::Wifi,
                enabled,
            }];
            assert_eq!(wifi_state(&features).unwrap(), enabled);
        }
    }
}
