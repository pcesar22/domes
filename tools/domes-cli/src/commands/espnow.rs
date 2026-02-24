//! ESP-NOW status and benchmark commands

use crate::protocol::{
    parse_espnow_bench_response, parse_get_espnow_status_response, serialize_espnow_bench,
    CliBenchResult, CliEspNowStatus, ConfigMsgType,
};
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

/// Run ESP-NOW latency benchmark
pub fn espnow_bench(transport: &mut dyn Transport, rounds: u32) -> Result<CliBenchResult> {
    let payload = serialize_espnow_bench(rounds);

    // Send request
    transport
        .send_frame(ConfigMsgType::EspnowBenchReq as u8, &payload)
        .context("Failed to send espnow bench command")?;

    // Wait for response with long timeout (benchmark can take 60+ seconds)
    let timeout_ms = 120_000u64;
    let frame = transport
        .receive_frame(timeout_ms)
        .context("Benchmark timed out waiting for response (may need more time)")?;

    if frame.msg_type != ConfigMsgType::EspnowBenchRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::EspnowBenchRsp as u8
        );
    }

    parse_espnow_bench_response(&frame.payload)
        .context("Failed to parse espnow bench response")
}
