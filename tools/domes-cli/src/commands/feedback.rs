use crate::proto::config::FeedbackProbe;
use crate::protocol::{
    parse_get_audio_volume_response, parse_set_audio_volume_response,
    parse_trigger_feedback_response, serialize_set_audio_volume, serialize_trigger_feedback,
    ConfigMsgType,
};
use crate::transport::Transport;
use anyhow::{Context, Result};

pub fn audio_volume_get(transport: &mut dyn Transport) -> Result<u8> {
    let frame = transport
        .send_command(ConfigMsgType::GetAudioVolumeReq as u8, &[])
        .context("Failed to send get audio software-gain command")?;
    if frame.msg_type != ConfigMsgType::GetAudioVolumeRsp as u8 {
        anyhow::bail!(
            "Unexpected get audio-volume response type: 0x{:02X}",
            frame.msg_type
        );
    }
    parse_get_audio_volume_response(&frame.payload)
        .context("Device rejected get audio software-gain command")
}

pub fn audio_volume_set(transport: &mut dyn Transport, volume: u8) -> Result<u8> {
    let frame = transport
        .send_command(
            ConfigMsgType::SetAudioVolumeReq as u8,
            &serialize_set_audio_volume(volume),
        )
        .context("Failed to send set audio software-gain command")?;
    if frame.msg_type != ConfigMsgType::SetAudioVolumeRsp as u8 {
        anyhow::bail!(
            "Unexpected set audio-volume response type: 0x{:02X}",
            frame.msg_type
        );
    }
    let applied = parse_set_audio_volume_response(&frame.payload)
        .context("Device rejected set audio software-gain command")?;
    if applied != volume {
        anyhow::bail!("Device applied software gain {applied}, requested {volume}");
    }
    Ok(applied)
}

pub fn feedback_play(transport: &mut dyn Transport, probe: FeedbackProbe) -> Result<()> {
    let frame = transport
        .send_command(
            ConfigMsgType::TriggerFeedbackReq as u8,
            &serialize_trigger_feedback(probe),
        )
        .context("Failed to send bounded feedback command")?;
    if frame.msg_type != ConfigMsgType::TriggerFeedbackRsp as u8 {
        anyhow::bail!(
            "Unexpected feedback response type: 0x{:02X}",
            frame.msg_type
        );
    }
    let (returned_probe, accepted) = parse_trigger_feedback_response(&frame.payload)
        .context("Device rejected bounded feedback command")?;
    if returned_probe != probe || !accepted {
        anyhow::bail!("Device did not accept the requested bounded feedback command");
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::proto::config::{SetAudioVolumeResponse, Status, TriggerFeedbackResponse};
    use crate::transport::Frame;
    use prost::Message;

    struct FakeTransport {
        response: Option<Frame>,
        request_type: Option<u8>,
        request_payload: Vec<u8>,
    }

    impl FakeTransport {
        fn new(response: Frame) -> Self {
            Self {
                response: Some(response),
                request_type: None,
                request_payload: Vec::new(),
            }
        }
    }

    impl Transport for FakeTransport {
        fn send_frame(&mut self, _: u8, _: &[u8]) -> Result<()> {
            unreachable!()
        }
        fn receive_frame(&mut self, _: u64) -> Result<Frame> {
            unreachable!()
        }
        fn send_command(&mut self, msg_type: u8, payload: &[u8]) -> Result<Frame> {
            self.request_type = Some(msg_type);
            self.request_payload = payload.to_vec();
            Ok(self.response.take().unwrap())
        }
    }

    fn status_payload<T: Message>(status: Status, message: &T) -> Vec<u8> {
        let mut payload = vec![status as u8];
        payload.extend(message.encode_to_vec());
        payload
    }

    #[test]
    fn set_volume_uses_generated_contract_and_applied_value() {
        let response = SetAudioVolumeResponse { volume: 64 };
        let mut transport = FakeTransport::new(Frame {
            msg_type: ConfigMsgType::SetAudioVolumeRsp as u8,
            payload: status_payload(Status::Ok, &response),
        });
        assert_eq!(audio_volume_set(&mut transport, 64).unwrap(), 64);
        assert_eq!(
            transport.request_type,
            Some(ConfigMsgType::SetAudioVolumeReq as u8)
        );
        assert_eq!(transport.request_payload, serialize_set_audio_volume(64));
    }

    #[test]
    fn rejected_probe_is_a_command_failure() {
        let response = TriggerFeedbackResponse {
            probe: FeedbackProbe::EmbeddedBeep as i32,
            accepted: false,
        };
        let mut transport = FakeTransport::new(Frame {
            msg_type: ConfigMsgType::TriggerFeedbackRsp as u8,
            payload: status_payload(Status::Rejected, &response),
        });
        let error = feedback_play(&mut transport, FeedbackProbe::EmbeddedBeep).unwrap_err();
        assert!(format!("{error:#}").contains("Rejected"));
    }
}
