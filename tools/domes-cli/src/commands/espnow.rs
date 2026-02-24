//! ESP-NOW status commands

use crate::protocol::{parse_get_espnow_status_response, CliEspNowStatus, ConfigMsgType};
use crate::transport::Transport;
use anyhow::{Context, Result};

/// Get ESP-NOW subsystem status
pub fn espnow_status(transport: &mut dyn Transport) -> Result<CliEspNowStatus> {
    let frame = transport
        .send_command(ConfigMsgType::GetEspnowStatusReq as u8, &[])
        .context("Failed to send get espnow status command")?;

    if frame.msg_type != ConfigMsgType::GetEspnowStatusRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::GetEspnowStatusRsp as u8
        );
    }

    parse_get_espnow_status_response(&frame.payload)
        .context("Failed to parse get espnow status response")
}
