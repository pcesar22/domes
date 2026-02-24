//! System health commands

use crate::protocol::{parse_get_health_response, CliHealthInfo, ConfigMsgType};
use crate::transport::Transport;
use anyhow::{Context, Result};

/// Get system health diagnostics
pub fn system_health(transport: &mut dyn Transport) -> Result<CliHealthInfo> {
    let frame = transport
        .send_command(ConfigMsgType::GetHealthReq as u8, &[])
        .context("Failed to send get health command")?;

    if frame.msg_type != ConfigMsgType::GetHealthRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::GetHealthRsp as u8
        );
    }

    parse_get_health_response(&frame.payload).context("Failed to parse get health response")
}
