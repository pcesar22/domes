//! RGB LED pattern commands

use crate::proto::config::RgbPattern;
use crate::protocol::{
    parse_get_rgb_pattern_response, parse_list_rgb_patterns_response,
    parse_set_rgb_pattern_response, serialize_set_rgb_pattern, CliRgbPatternInfo,
    CliRgbPatternState, ConfigMsgType,
};
use crate::transport::Transport;
use anyhow::{Context, Result};

/// List all available RGB patterns
pub fn rgb_list(transport: &mut dyn Transport) -> Result<Vec<CliRgbPatternInfo>> {
    let frame = transport
        .send_command(ConfigMsgType::ListRgbPatternsReq as u8, &[])
        .context("Failed to send list RGB patterns command")?;

    if frame.msg_type != ConfigMsgType::ListRgbPatternsRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::ListRgbPatternsRsp as u8
        );
    }

    parse_list_rgb_patterns_response(&frame.payload)
        .context("Failed to parse list RGB patterns response")
}

/// Get the current RGB pattern state
pub fn rgb_get(transport: &mut dyn Transport) -> Result<CliRgbPatternState> {
    let frame = transport
        .send_command(ConfigMsgType::GetRgbPatternReq as u8, &[])
        .context("Failed to send get RGB pattern command")?;

    if frame.msg_type != ConfigMsgType::GetRgbPatternRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::GetRgbPatternRsp as u8
        );
    }

    parse_get_rgb_pattern_response(&frame.payload).context("Failed to parse get RGB pattern response")
}

/// Set an RGB pattern
pub fn rgb_set(
    transport: &mut dyn Transport,
    pattern: RgbPattern,
    color: Option<(u8, u8, u8)>,
    speed_ms: Option<u32>,
    brightness: Option<u8>,
) -> Result<RgbPattern> {
    let payload = serialize_set_rgb_pattern(pattern, color, speed_ms, brightness);
    let frame = transport
        .send_command(ConfigMsgType::SetRgbPatternReq as u8, &payload)
        .context("Failed to send set RGB pattern command")?;

    if frame.msg_type != ConfigMsgType::SetRgbPatternRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SetRgbPatternRsp as u8
        );
    }

    parse_set_rgb_pattern_response(&frame.payload).context("Failed to parse set RGB pattern response")
}

/// Turn off all LEDs (shorthand for setting pattern to Off)
pub fn rgb_off(transport: &mut dyn Transport) -> Result<RgbPattern> {
    rgb_set(transport, RgbPattern::Off, None, None, None)
}
