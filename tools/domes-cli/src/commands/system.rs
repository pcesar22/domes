//! System mode commands

use crate::proto::config::SystemMode;
use crate::protocol::{
    parse_get_mode_response, parse_get_system_info_response, parse_set_mode_response,
    parse_set_pod_id_response, serialize_set_mode, serialize_set_pod_id, CliModeInfo,
    CliSystemInfo, ConfigMsgType,
};
use crate::transport::Transport;
use anyhow::{Context, Result};

/// Get the current system mode
pub fn system_get_mode(transport: &mut dyn Transport) -> Result<CliModeInfo> {
    let frame = transport
        .send_command(ConfigMsgType::GetModeReq as u8, &[])
        .context("Failed to send get mode command")?;

    if frame.msg_type != ConfigMsgType::GetModeRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::GetModeRsp as u8
        );
    }

    parse_get_mode_response(&frame.payload).context("Failed to parse get mode response")
}

/// Set the system mode
pub fn system_set_mode(
    transport: &mut dyn Transport,
    mode: SystemMode,
) -> Result<(SystemMode, bool)> {
    let payload = serialize_set_mode(mode);
    let frame = transport
        .send_command(ConfigMsgType::SetModeReq as u8, &payload)
        .context("Failed to send set mode command")?;

    if frame.msg_type != ConfigMsgType::SetModeRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SetModeRsp as u8
        );
    }

    parse_set_mode_response(&frame.payload).context("Failed to parse set mode response")
}

/// Get system information
pub fn system_info(transport: &mut dyn Transport) -> Result<CliSystemInfo> {
    let frame = transport
        .send_command(ConfigMsgType::GetSystemInfoReq as u8, &[])
        .context("Failed to send get system info command")?;

    if frame.msg_type != ConfigMsgType::GetSystemInfoRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::GetSystemInfoRsp as u8
        );
    }

    parse_get_system_info_response(&frame.payload)
        .context("Failed to parse get system info response")
}

/// Set the pod ID (persisted to NVS, takes effect on next reboot for BLE name)
pub fn system_set_pod_id(transport: &mut dyn Transport, pod_id: u32) -> Result<u32> {
    let payload = serialize_set_pod_id(pod_id);
    let frame = transport
        .send_command(ConfigMsgType::SetPodIdReq as u8, &payload)
        .context("Failed to send set pod id command")?;

    if frame.msg_type != ConfigMsgType::SetPodIdRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SetPodIdRsp as u8
        );
    }

    parse_set_pod_id_response(&frame.payload).context("Failed to parse set pod id response")
}
