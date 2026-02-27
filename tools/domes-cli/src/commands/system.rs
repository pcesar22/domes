//! System mode and diagnostics commands

use crate::proto::config::SystemMode;
use crate::protocol::{
    parse_clear_crash_dump_response, parse_crash_dump_response, parse_get_mode_response,
    parse_get_system_info_response, parse_memory_profile_response, parse_self_test_response,
    parse_set_mode_response, parse_set_pod_id_response, serialize_set_mode, serialize_set_pod_id,
    CliCrashDump, CliMemoryProfile, CliModeInfo, CliSelfTestInfo, CliSystemInfo, ConfigMsgType,
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

/// Get crash dump from device
pub fn system_crash_dump(transport: &mut dyn Transport) -> Result<CliCrashDump> {
    let frame = transport
        .send_command(ConfigMsgType::GetCrashDumpReq as u8, &[])
        .context("Failed to send get crash dump command")?;

    if frame.msg_type != ConfigMsgType::GetCrashDumpRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::GetCrashDumpRsp as u8
        );
    }

    parse_crash_dump_response(&frame.payload).context("Failed to parse crash dump response")
}

/// Clear crash dump from device
pub fn system_clear_crash_dump(transport: &mut dyn Transport) -> Result<bool> {
    let frame = transport
        .send_command(ConfigMsgType::ClearCrashDumpReq as u8, &[])
        .context("Failed to send clear crash dump command")?;

    if frame.msg_type != ConfigMsgType::ClearCrashDumpRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::ClearCrashDumpRsp as u8
        );
    }

    parse_clear_crash_dump_response(&frame.payload)
        .context("Failed to parse clear crash dump response")
}

/// Get memory profile from device
pub fn system_memory_profile(transport: &mut dyn Transport) -> Result<CliMemoryProfile> {
    let frame = transport
        .send_command(ConfigMsgType::GetMemoryProfileReq as u8, &[])
        .context("Failed to send get memory profile command")?;

    if frame.msg_type != ConfigMsgType::GetMemoryProfileRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::GetMemoryProfileRsp as u8
        );
    }

    parse_memory_profile_response(&frame.payload)
        .context("Failed to parse memory profile response")
}

/// Timeout for self-test command (tests WiFi scan, BLE, NVS, etc.)
const SELF_TEST_TIMEOUT_MS: u64 = 15000;

/// Run on-device self-test suite
pub fn system_self_test(transport: &mut dyn Transport) -> Result<CliSelfTestInfo> {
    let frame = transport
        .send_command_with_timeout(ConfigMsgType::SelfTestReq as u8, &[], SELF_TEST_TIMEOUT_MS)
        .context("Failed to send self-test command")?;

    if frame.msg_type != ConfigMsgType::SelfTestRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SelfTestRsp as u8
        );
    }

    parse_self_test_response(&frame.payload).context("Failed to parse self-test response")
}
