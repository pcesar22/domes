//! IMU commands

use crate::protocol::{parse_imu_triage_response, serialize_set_imu_triage, ConfigMsgType};
use crate::transport::Transport;
use anyhow::{Context, Result};

/// Set IMU triage mode
pub fn imu_triage_set(transport: &mut dyn Transport, enabled: bool) -> Result<bool> {
    let payload = serialize_set_imu_triage(enabled);
    let frame = transport
        .send_command(ConfigMsgType::SetImuTriageReq as u8, &payload)
        .context("Failed to send set IMU triage command")?;

    if frame.msg_type != ConfigMsgType::SetImuTriageRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SetImuTriageRsp as u8
        );
    }

    parse_imu_triage_response(&frame.payload).context("Failed to parse IMU triage response")
}
