//! LED pattern commands

use crate::protocol::{
    parse_led_pattern_response, serialize_set_led_pattern, CliLedPattern, ConfigMsgType,
};
use crate::transport::Transport;
use anyhow::{Context, Result};

/// Get current LED pattern
pub fn led_get(transport: &mut dyn Transport) -> Result<CliLedPattern> {
    let frame = transport
        .send_command(ConfigMsgType::GetLedPatternReq as u8, &[])
        .context("Failed to send get LED pattern command")?;

    if frame.msg_type != ConfigMsgType::GetLedPatternRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::GetLedPatternRsp as u8
        );
    }

    parse_led_pattern_response(&frame.payload).context("Failed to parse get LED pattern response")
}

/// Set LED pattern
pub fn led_set(transport: &mut dyn Transport, pattern: &CliLedPattern) -> Result<CliLedPattern> {
    let payload = serialize_set_led_pattern(pattern);
    let frame = transport
        .send_command(ConfigMsgType::SetLedPatternReq as u8, &payload)
        .context("Failed to send set LED pattern command")?;

    if frame.msg_type != ConfigMsgType::SetLedPatternRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SetLedPatternRsp as u8
        );
    }

    let applied = parse_led_pattern_response(&frame.payload)
        .context("Failed to parse set LED pattern response")?;
    validate_applied_pattern(pattern, &applied)?;
    Ok(applied)
}

/// Turn LEDs off
pub fn led_off(transport: &mut dyn Transport) -> Result<CliLedPattern> {
    led_set(transport, &CliLedPattern::off())
}

fn validate_applied_pattern(requested: &CliLedPattern, applied: &CliLedPattern) -> Result<()> {
    use crate::proto::config::LedPatternType;

    let matches = requested.pattern_type == applied.pattern_type
        && match requested.pattern_type {
            LedPatternType::LedPatternOff => true,
            LedPatternType::LedPatternSolid => {
                requested.color == applied.color && requested.brightness == applied.brightness
            }
            LedPatternType::LedPatternBreathing => {
                requested.color == applied.color
                    && requested.period_ms == applied.period_ms
                    && requested.brightness == applied.brightness
            }
            LedPatternType::LedPatternColorCycle => {
                requested.colors == applied.colors
                    && requested.period_ms == applied.period_ms
                    && requested.brightness == applied.brightness
            }
        };

    if !matches {
        anyhow::bail!(
            "Applied LED pattern did not match request: requested {:?}, got {:?}",
            requested,
            applied
        );
    }

    Ok(())
}
