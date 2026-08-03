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

pub(crate) fn require_wifi_capability(transport: &mut dyn Transport) -> Result<()> {
    let features = super::feature_list(transport)?;
    wifi_state(&features).map(|_| ())
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
    use crate::proto::config::{FeatureState, ListFeaturesResponse};
    use crate::protocol::ConfigMsgType;
    use crate::transport::Frame;
    use anyhow::bail;
    use prost::Message;
    use std::collections::VecDeque;

    struct MockTransport {
        responses: VecDeque<Frame>,
        commands: Vec<u8>,
    }

    impl MockTransport {
        fn without_wifi() -> Self {
            let response = ListFeaturesResponse {
                features: vec![FeatureState {
                    feature: Feature::LedEffects as i32,
                    enabled: true,
                }],
                pod_id: 1,
            };
            Self {
                responses: [Frame {
                    msg_type: ConfigMsgType::ListFeaturesRsp as u8,
                    payload: response.encode_to_vec(),
                }]
                .into(),
                commands: Vec::new(),
            }
        }
    }

    impl Transport for MockTransport {
        fn send_frame(&mut self, _msg_type: u8, _payload: &[u8]) -> Result<()> {
            bail!("unexpected send_frame")
        }

        fn receive_frame(&mut self, _timeout_ms: u64) -> Result<Frame> {
            bail!("unexpected receive_frame")
        }

        fn send_command(&mut self, msg_type: u8, _payload: &[u8]) -> Result<Frame> {
            self.commands.push(msg_type);
            self.responses
                .pop_front()
                .ok_or_else(|| anyhow!("no mock response queued"))
        }
    }

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

    #[test]
    fn wifi_mutations_reject_unadvertised_capability_before_set_command() {
        for command in [wifi_enable, wifi_disable] {
            let mut transport = MockTransport::without_wifi();

            let error = command(&mut transport).unwrap_err().to_string();

            assert!(error.contains("does not expose the WiFi feature flag"));
            assert_eq!(
                transport.commands,
                vec![ConfigMsgType::ListFeaturesReq as u8]
            );
        }
    }

    #[test]
    fn generic_wifi_mutations_use_the_same_capability_preflight() {
        let mut enable_transport = MockTransport::without_wifi();
        let error = super::super::feature_enable(&mut enable_transport, Feature::Wifi)
            .unwrap_err()
            .to_string();
        assert!(error.contains("does not expose the WiFi feature flag"));
        assert_eq!(
            enable_transport.commands,
            vec![ConfigMsgType::ListFeaturesReq as u8]
        );

        let mut disable_transport = MockTransport::without_wifi();
        let error = super::super::feature_disable(&mut disable_transport, Feature::Wifi)
            .unwrap_err()
            .to_string();
        assert!(error.contains("does not expose the WiFi feature flag"));
        assert_eq!(
            disable_transport.commands,
            vec![ConfigMsgType::ListFeaturesReq as u8]
        );
    }
}
