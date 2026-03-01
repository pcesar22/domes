//! Touch injection commands

use crate::protocol::{
    parse_simulate_touch_response, serialize_simulate_touch, ConfigMsgType,
};
use crate::transport::Transport;
use anyhow::{Context, Result};

/// Inject a simulated touch on a specific pad
pub fn touch_simulate(transport: &mut dyn Transport, pad_index: u32) -> Result<()> {
    let payload = serialize_simulate_touch(pad_index);

    let frame = transport
        .send_command(ConfigMsgType::SimulateTouchReq as u8, &payload)
        .context("Failed to send simulate touch command")?;

    if frame.msg_type != ConfigMsgType::SimulateTouchRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SimulateTouchRsp as u8
        );
    }

    parse_simulate_touch_response(&frame.payload)
        .context("Failed to parse simulate touch response")?;

    Ok(())
}
