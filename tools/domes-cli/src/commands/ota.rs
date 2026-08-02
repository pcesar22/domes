//! OTA update commands
//!
//! Sends firmware updates to DOMES devices over serial or BLE.
//! Also includes GitHub OTA check and auto-update configuration commands.

use crate::protocol::{
    parse_check_update_response, parse_set_auto_update_response, serialize_set_auto_update,
    CliUpdateInfo, ConfigMsgType,
};
use crate::transport::Transport;
use anyhow::{Context, Result};
use sha2::{Digest, Sha256};
use std::fs::File;
use std::io::{Read, Write as IoWrite};
use std::path::Path;

/// OTA message types
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum OtaMsgType {
    Begin = 0x01,
    Data = 0x02,
    End = 0x03,
    Ack = 0x04,
    Abort = 0x05,
}

impl OtaMsgType {
    fn from_u8(value: u8) -> Option<Self> {
        match value {
            0x01 => Some(OtaMsgType::Begin),
            0x02 => Some(OtaMsgType::Data),
            0x03 => Some(OtaMsgType::End),
            0x04 => Some(OtaMsgType::Ack),
            0x05 => Some(OtaMsgType::Abort),
            _ => None,
        }
    }
}

/// OTA status codes
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum OtaStatus {
    Ok = 0,
    Busy = 1,
    FlashError = 2,
    VerifyFailed = 3,
    SizeMismatch = 4,
    OffsetMismatch = 5,
    VersionError = 6,
    PartitionError = 7,
    Aborted = 8,
}

impl OtaStatus {
    fn from_u8(value: u8) -> Result<Self> {
        match value {
            0 => Ok(OtaStatus::Ok),
            1 => Ok(OtaStatus::Busy),
            2 => Ok(OtaStatus::FlashError),
            3 => Ok(OtaStatus::VerifyFailed),
            4 => Ok(OtaStatus::SizeMismatch),
            5 => Ok(OtaStatus::OffsetMismatch),
            6 => Ok(OtaStatus::VersionError),
            7 => Ok(OtaStatus::PartitionError),
            8 => Ok(OtaStatus::Aborted),
            _ => anyhow::bail!("Unknown OTA status code: {}", value),
        }
    }

    fn description(self) -> &'static str {
        match self {
            OtaStatus::Ok => "OK",
            OtaStatus::Busy => "Busy",
            OtaStatus::FlashError => "Flash error",
            OtaStatus::VerifyFailed => "Verification failed",
            OtaStatus::SizeMismatch => "Size mismatch",
            OtaStatus::OffsetMismatch => "Offset mismatch",
            OtaStatus::VersionError => "Version error",
            OtaStatus::PartitionError => "Partition error",
            OtaStatus::Aborted => "Aborted",
        }
    }
}

// Note: OTA chunk size is now determined by the transport's max_ota_chunk_size() method
// to handle different MTU limits (BLE uses smaller chunks than serial/TCP)

/// SHA256 size
const SHA256_SIZE: usize = 32;

/// Version string max length
const VERSION_MAX_LEN: usize = 32;

/// Maximum data bytes in one OTA_DATA payload.
const OTA_CHUNK_MAX_LEN: usize = 1016;

/// Timeout for OTA operations (ms)
const OTA_TIMEOUT_MS: u64 = 5000;

/// Timeout for OTA_END (device reboots) (ms)
const OTA_END_TIMEOUT_MS: u64 = 30000;

/// Send firmware OTA update to device
pub fn ota_flash(transport: &mut dyn Transport, firmware_path: &Path, version: &str) -> Result<()> {
    // Read firmware file
    println!("Reading firmware from '{}'...", firmware_path.display());
    let firmware = read_firmware_file(firmware_path)?;
    flash_firmware(transport, &firmware, version)
}

fn flash_firmware(transport: &mut dyn Transport, firmware: &[u8], version: &str) -> Result<()> {
    validate_firmware_version(version)?;
    let firmware_size = firmware_size_for_wire(firmware.len())?;
    println!("Firmware size: {} bytes", firmware.len());

    // Compute SHA256
    println!("Computing SHA256...");
    let sha256 = compute_sha256(firmware);
    print!("SHA256: ");
    for byte in &sha256 {
        print!("{:02x}", byte);
    }
    println!();

    // Send OTA_BEGIN
    println!("Sending OTA_BEGIN (version: {})...", version);
    let begin_payload = serialize_ota_begin(firmware_size, &sha256, version)?;

    let (status, next_offset) =
        match send_and_wait_ack(transport, OtaMsgType::Begin, &begin_payload, OTA_TIMEOUT_MS) {
            Ok(response) => response,
            Err(error) => {
                // Once OTA_BEGIN is written, the device may own an OTA session even
                // when its ACK is lost or malformed. Release that session without
                // replacing the original transfer error.
                best_effort_ota_abort(transport);
                return Err(error);
            }
        };

    if status != OtaStatus::Ok {
        anyhow::bail!("Device rejected OTA_BEGIN: {}", status.description());
    }
    println!("Device accepted OTA_BEGIN.");

    let transfer_result = (|| -> Result<()> {
        validate_next_offset("OTA_BEGIN", next_offset, 0)?;

        // Use the transport-specific chunk size because BLE has a lower MTU.
        let ota_chunk_size = transport.max_ota_chunk_size();
        if ota_chunk_size == 0 {
            anyhow::bail!("Transport reported an OTA chunk size of zero");
        }
        println!(
            "Sending firmware data (chunk size: {} bytes)...",
            ota_chunk_size
        );
        let mut offset: usize = 0;
        let total = firmware.len();

        while offset < total {
            let chunk_size = std::cmp::min(ota_chunk_size, total - offset);
            let expected_next = offset
                .checked_add(chunk_size)
                .context("OTA offset overflow")?;
            let offset_u32 = u32::try_from(offset).context("OTA offset exceeds 32-bit limit")?;
            let expected_next_u32 =
                u32::try_from(expected_next).context("OTA offset exceeds 32-bit limit")?;
            let chunk = &firmware[offset..expected_next];

            let data_payload = serialize_ota_data(offset_u32, chunk)?;
            let (status, acknowledged_offset) =
                send_and_wait_ack(transport, OtaMsgType::Data, &data_payload, OTA_TIMEOUT_MS)?;

            if status != OtaStatus::Ok {
                anyhow::bail!(
                    "Device rejected chunk at offset {}: {}",
                    offset,
                    status.description()
                );
            }
            validate_next_offset("OTA_DATA", acknowledged_offset, expected_next_u32)?;

            offset = usize::try_from(acknowledged_offset)
                .context("Acknowledged OTA offset does not fit this host")?;
            print_progress(offset, total);
        }
        println!();

        // Send OTA_END
        println!("Sending OTA_END...");
        let (status, acknowledged_offset) =
            send_and_wait_ack(transport, OtaMsgType::End, &[], OTA_END_TIMEOUT_MS)?;

        if status != OtaStatus::Ok {
            anyhow::bail!("Device rejected OTA_END: {}", status.description());
        }
        validate_next_offset("OTA_END", acknowledged_offset, firmware_size)?;

        println!("\nOTA complete! Device will reboot.");
        Ok(())
    })();

    if transfer_result.is_err() {
        best_effort_ota_abort(transport);
    }

    transfer_result
}

fn firmware_size_for_wire(size: usize) -> Result<u32> {
    u32::try_from(size).context("Firmware image exceeds the OTA protocol's 32-bit size limit")
}

/// Read firmware file into memory
fn read_firmware_file(path: &Path) -> Result<Vec<u8>> {
    let mut file = File::open(path).context("Cannot open firmware file")?;
    let mut data = Vec::new();
    file.read_to_end(&mut data)
        .context("Failed to read firmware file")?;

    if data.is_empty() {
        anyhow::bail!("Firmware file is empty");
    }

    Ok(data)
}

/// Compute SHA256 hash
fn compute_sha256(data: &[u8]) -> [u8; SHA256_SIZE] {
    let mut hasher = Sha256::new();
    hasher.update(data);
    let result = hasher.finalize();
    let mut hash = [0u8; SHA256_SIZE];
    hash.copy_from_slice(&result);
    hash
}

/// Serialize OTA_BEGIN payload
/// Format: [u32 firmwareSize][32 bytes sha256][32 bytes version]
fn serialize_ota_begin(firmware_size: u32, sha256: &[u8; 32], version: &str) -> Result<Vec<u8>> {
    validate_firmware_version(version)?;
    if version.len() >= VERSION_MAX_LEN {
        anyhow::bail!(
            "OTA version is {} bytes; maximum is {} bytes",
            version.len(),
            VERSION_MAX_LEN - 1
        );
    }
    if version.as_bytes().contains(&0) {
        anyhow::bail!("OTA version must not contain a NUL byte");
    }

    let mut payload = Vec::with_capacity(4 + 32 + 32);

    // Firmware size (little-endian)
    payload.extend_from_slice(&firmware_size.to_le_bytes());

    // SHA256
    payload.extend_from_slice(sha256);

    // Version (null-terminated, padded to 32 bytes)
    let mut version_bytes = [0u8; VERSION_MAX_LEN];
    let version_slice = version.as_bytes();
    version_bytes[..version_slice.len()].copy_from_slice(version_slice);
    payload.extend_from_slice(&version_bytes);

    Ok(payload)
}

/// Serialize OTA_DATA payload
/// Format: [u32 offset][u16 length][data...]
fn serialize_ota_data(offset: u32, data: &[u8]) -> Result<Vec<u8>> {
    if data.is_empty() {
        anyhow::bail!("OTA chunk must contain at least one byte");
    }
    if data.len() > OTA_CHUNK_MAX_LEN {
        anyhow::bail!(
            "OTA chunk is {} bytes; protocol maximum is {} bytes",
            data.len(),
            OTA_CHUNK_MAX_LEN
        );
    }
    let mut payload = Vec::with_capacity(4 + 2 + data.len());

    // Offset (little-endian)
    payload.extend_from_slice(&offset.to_le_bytes());

    // Length (little-endian)
    let data_len = u16::try_from(data.len()).context("OTA chunk exceeds 16-bit length limit")?;
    payload.extend_from_slice(&data_len.to_le_bytes());

    // Data
    payload.extend_from_slice(data);

    Ok(payload)
}

/// Deserialize OTA_ACK payload
/// Format: [u8 status][u32 nextOffset]
fn deserialize_ota_ack(payload: &[u8]) -> Result<(OtaStatus, u32)> {
    if payload.len() != 5 {
        anyhow::bail!(
            "OTA_ACK payload has {} bytes, expected exactly 5",
            payload.len()
        );
    }

    let status = OtaStatus::from_u8(payload[0])?;
    let next_offset = u32::from_le_bytes([payload[1], payload[2], payload[3], payload[4]]);

    Ok((status, next_offset))
}

/// Deserialize OTA_ABORT payload
fn deserialize_ota_abort(payload: &[u8]) -> Result<OtaStatus> {
    if payload.len() != 1 {
        anyhow::bail!(
            "OTA_ABORT payload has {} bytes, expected exactly 1",
            payload.len()
        );
    }
    OtaStatus::from_u8(payload[0])
}

fn validate_firmware_version(version: &str) -> Result<()> {
    if version.is_empty() {
        anyhow::bail!("OTA version is required");
    }
    if version.len() >= VERSION_MAX_LEN {
        anyhow::bail!(
            "OTA version is {} bytes; maximum is {} bytes",
            version.len(),
            VERSION_MAX_LEN - 1
        );
    }
    if !version.is_ascii() || version.as_bytes().contains(&0) {
        anyhow::bail!("OTA version must contain only non-NUL ASCII characters");
    }

    let without_prefix = version
        .strip_prefix('v')
        .or_else(|| version.strip_prefix('V'))
        .unwrap_or(version);
    let (core, suffix) = without_prefix
        .split_once('-')
        .map_or((without_prefix, None), |(core, suffix)| {
            (core, Some(suffix))
        });
    let components: Vec<&str> = core.split('.').collect();
    if components.len() != 3
        || components.iter().any(|component| {
            component.is_empty()
                || !component.bytes().all(|byte| byte.is_ascii_digit())
                || component.parse::<u32>().is_err()
        })
    {
        anyhow::bail!("OTA version is not parser-valid: {version}");
    }

    let Some(suffix) = suffix else {
        return Ok(());
    };
    if suffix == "dirty" {
        return Ok(());
    }

    let suffix_parts: Vec<&str> = suffix.split('-').collect();
    if !(suffix_parts.len() == 2 || (suffix_parts.len() == 3 && suffix_parts[2] == "dirty"))
        || suffix_parts[0].is_empty()
        || !suffix_parts[0].bytes().all(|byte| byte.is_ascii_digit())
        || suffix_parts[0].parse::<u32>().is_err()
    {
        anyhow::bail!("OTA version is not parser-valid: {version}");
    }
    let hash = suffix_parts[1].strip_prefix('g').unwrap_or("");
    if hash.is_empty() || hash.len() > 40 || !hash.bytes().all(|byte| byte.is_ascii_hexdigit()) {
        anyhow::bail!("OTA version is not parser-valid: {version}");
    }

    Ok(())
}

fn validate_next_offset(operation: &str, actual: u32, expected: u32) -> Result<()> {
    if actual != expected {
        anyhow::bail!(
            "{} acknowledged next offset {}, expected {}",
            operation,
            actual,
            expected
        );
    }

    Ok(())
}

fn send_ota_abort(transport: &mut dyn Transport) -> Result<()> {
    transport
        .send_frame(OtaMsgType::Abort as u8, &[OtaStatus::Aborted as u8])
        .context("Failed to send OTA_ABORT")
}

fn best_effort_ota_abort(transport: &mut dyn Transport) {
    if let Err(abort_error) = send_ota_abort(transport) {
        eprintln!("Warning: failed to abort OTA session: {abort_error:#}");
    }
}

/// Send a frame and wait for ACK
fn send_and_wait_ack(
    transport: &mut dyn Transport,
    msg_type: OtaMsgType,
    payload: &[u8],
    timeout_ms: u64,
) -> Result<(OtaStatus, u32)> {
    // Send the frame
    transport
        .send_frame(msg_type as u8, payload)
        .context("Failed to send OTA frame")?;

    // Wait for response
    let frame = transport
        .receive_frame(timeout_ms)
        .context("Timeout waiting for OTA response")?;

    match OtaMsgType::from_u8(frame.msg_type) {
        Some(OtaMsgType::Ack) => deserialize_ota_ack(&frame.payload),
        Some(OtaMsgType::Abort) => {
            let reason = deserialize_ota_abort(&frame.payload)?;
            anyhow::bail!("Device aborted OTA: {}", reason.description())
        }
        _ => {
            anyhow::bail!("Unexpected response type: 0x{:02X}", frame.msg_type)
        }
    }
}

/// Timeout for update check (device queries GitHub API over HTTPS)
const OTA_CHECK_TIMEOUT_MS: u64 = 15000;

/// Check for firmware updates via GitHub releases
pub fn ota_check(transport: &mut dyn Transport) -> Result<CliUpdateInfo> {
    let frame = transport
        .send_command_with_timeout(
            ConfigMsgType::CheckUpdateReq as u8,
            &[],
            OTA_CHECK_TIMEOUT_MS,
        )
        .context("Failed to send check update command")?;

    if frame.msg_type != ConfigMsgType::CheckUpdateRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::CheckUpdateRsp as u8
        );
    }

    parse_check_update_response(&frame.payload).context("Failed to parse check update response")
}

/// Set auto-update enabled/disabled
pub fn ota_auto_update(transport: &mut dyn Transport, enabled: bool) -> Result<bool> {
    let payload = serialize_set_auto_update(enabled);
    let frame = transport
        .send_command(ConfigMsgType::SetAutoUpdateReq as u8, &payload)
        .context("Failed to send set auto-update command")?;

    if frame.msg_type != ConfigMsgType::SetAutoUpdateRsp as u8 {
        anyhow::bail!(
            "Unexpected response type: 0x{:02X}, expected 0x{:02X}",
            frame.msg_type,
            ConfigMsgType::SetAutoUpdateRsp as u8
        );
    }

    parse_set_auto_update_response(&frame.payload)
        .context("Failed to parse set auto-update response")
}

/// Print progress bar
fn print_progress(current: usize, total: usize) {
    const BAR_WIDTH: usize = 40;
    let progress = current as f64 / total as f64;
    let pos = (BAR_WIDTH as f64 * progress) as usize;

    print!("\r[");
    for i in 0..BAR_WIDTH {
        if i < pos {
            print!("=");
        } else if i == pos {
            print!(">");
        } else {
            print!(" ");
        }
    }
    print!("] {} / {} bytes ({:.1}%)", current, total, progress * 100.0);
    std::io::stdout().flush().ok();
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::transport::Frame;
    use std::collections::VecDeque;

    struct MockTransport {
        responses: VecDeque<Frame>,
        sent: Vec<(u8, Vec<u8>)>,
        chunk_size: usize,
    }

    impl MockTransport {
        fn new(responses: Vec<Frame>, chunk_size: usize) -> Self {
            Self {
                responses: responses.into(),
                sent: Vec::new(),
                chunk_size,
            }
        }
    }

    impl Transport for MockTransport {
        fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()> {
            self.sent.push((msg_type, payload.to_vec()));
            Ok(())
        }

        fn receive_frame(&mut self, _timeout_ms: u64) -> Result<Frame> {
            self.responses
                .pop_front()
                .context("No mock OTA response queued")
        }

        fn send_command(&mut self, _msg_type: u8, _payload: &[u8]) -> Result<Frame> {
            anyhow::bail!("Mock OTA transport does not support config commands")
        }

        fn max_ota_chunk_size(&self) -> usize {
            self.chunk_size
        }
    }

    fn ack(status: u8, next_offset: u32) -> Frame {
        let mut payload = vec![status];
        payload.extend_from_slice(&next_offset.to_le_bytes());
        Frame {
            msg_type: OtaMsgType::Ack as u8,
            payload,
        }
    }

    #[test]
    fn ota_flash_advances_using_acknowledged_offsets() {
        let responses = vec![
            ack(OtaStatus::Ok as u8, 0),
            ack(OtaStatus::Ok as u8, 2),
            ack(OtaStatus::Ok as u8, 4),
            ack(OtaStatus::Ok as u8, 4),
        ];
        let mut transport = MockTransport::new(responses, 2);

        flash_firmware(&mut transport, &[1, 2, 3, 4], "v1.2.3").unwrap();

        let message_types: Vec<u8> = transport.sent.iter().map(|(kind, _)| *kind).collect();
        assert_eq!(
            message_types,
            vec![
                OtaMsgType::Begin as u8,
                OtaMsgType::Data as u8,
                OtaMsgType::Data as u8,
                OtaMsgType::End as u8,
            ]
        );
    }

    #[test]
    fn ota_version_must_fit_null_terminated_wire_field() {
        let hash = [0u8; SHA256_SIZE];
        let maximum = format!("v0.0.0-0-g{}", "a".repeat(21));
        assert_eq!(maximum.len(), VERSION_MAX_LEN - 1);
        let payload = serialize_ota_begin(1, &hash, &maximum).unwrap();
        assert_eq!(payload.len(), 4 + SHA256_SIZE + VERSION_MAX_LEN);

        let too_long = format!("v0.0.0-0-g{}", "a".repeat(22));
        assert_eq!(too_long.len(), VERSION_MAX_LEN);
        let error = serialize_ota_begin(1, &hash, &too_long)
            .unwrap_err()
            .to_string();
        assert!(error.contains("maximum is 31 bytes"));
    }

    #[test]
    fn invalid_ota_version_is_rejected_before_transport_write() {
        let mut transport = MockTransport::new(Vec::new(), 2);
        let version = "v".repeat(VERSION_MAX_LEN);

        let error = flash_firmware(&mut transport, &[1, 2], &version)
            .unwrap_err()
            .to_string();

        assert!(error.contains("maximum is 31 bytes"));
        assert!(transport.sent.is_empty());
    }

    #[test]
    fn ota_version_must_match_firmware_parser_grammar() {
        for version in ["v1.2.3", "1.2.3", "v0.0.0-1-g0123456789ab", "v1.2.3-dirty"] {
            validate_firmware_version(version).unwrap();
        }

        for version in [
            "",
            "unknown",
            "v1.2",
            "v1.2.3-rollback",
            "v1.2.3-1-gxyz",
            "v4294967296.0.0",
        ] {
            assert!(validate_firmware_version(version).is_err(), "{version}");
        }
    }

    #[test]
    fn ota_flash_rejects_wrong_next_offset_and_aborts_session() {
        let responses = vec![ack(OtaStatus::Ok as u8, 0), ack(OtaStatus::Ok as u8, 1)];
        let mut transport = MockTransport::new(responses, 2);

        let error = flash_firmware(&mut transport, &[1, 2, 3, 4], "v1.2.3")
            .unwrap_err()
            .to_string();

        assert!(error.contains("acknowledged next offset 1, expected 2"));
        assert_eq!(
            transport.sent.last().map(|(kind, _)| *kind),
            Some(OtaMsgType::Abort as u8)
        );
        assert_eq!(
            transport.sent.last().map(|(_, payload)| payload.as_slice()),
            Some(&[OtaStatus::Aborted as u8][..])
        );
    }

    #[test]
    fn ota_flash_aborts_when_begin_ack_is_lost() {
        let mut transport = MockTransport::new(Vec::new(), 2);

        let error = flash_firmware(&mut transport, &[1, 2], "v1.2.3")
            .unwrap_err()
            .to_string();

        assert!(error.contains("Timeout waiting for OTA response"));
        assert_eq!(
            transport
                .sent
                .iter()
                .map(|(kind, _)| *kind)
                .collect::<Vec<_>>(),
            vec![OtaMsgType::Begin as u8, OtaMsgType::Abort as u8]
        );
    }

    #[test]
    fn ota_flash_aborts_when_begin_ack_is_malformed() {
        let responses = vec![Frame {
            msg_type: OtaMsgType::Ack as u8,
            payload: vec![OtaStatus::Ok as u8],
        }];
        let mut transport = MockTransport::new(responses, 2);

        let error = flash_firmware(&mut transport, &[1, 2], "v1.2.3")
            .unwrap_err()
            .to_string();

        assert!(error.contains("OTA_ACK payload has 1 bytes, expected exactly 5"));
        assert_eq!(
            transport
                .sent
                .iter()
                .map(|(kind, _)| *kind)
                .collect::<Vec<_>>(),
            vec![OtaMsgType::Begin as u8, OtaMsgType::Abort as u8]
        );
    }

    #[test]
    fn ota_flash_does_not_abort_an_explicitly_rejected_begin() {
        let responses = vec![ack(OtaStatus::Busy as u8, 0)];
        let mut transport = MockTransport::new(responses, 2);

        let error = flash_firmware(&mut transport, &[1, 2], "v1.2.3")
            .unwrap_err()
            .to_string();

        assert!(error.contains("Device rejected OTA_BEGIN: Busy"));
        assert_eq!(
            transport
                .sent
                .iter()
                .map(|(kind, _)| *kind)
                .collect::<Vec<_>>(),
            vec![OtaMsgType::Begin as u8]
        );
    }

    #[test]
    fn ota_ack_rejects_unknown_status_code() {
        let error = deserialize_ota_ack(&[0xFF, 0, 0, 0, 0])
            .unwrap_err()
            .to_string();

        assert!(error.contains("Unknown OTA status code: 255"));
    }

    #[test]
    fn ota_ack_and_abort_require_exact_payload_sizes() {
        assert!(deserialize_ota_ack(&[0, 0, 0, 0, 0, 0]).is_err());
        assert!(deserialize_ota_abort(&[]).is_err());
        assert!(deserialize_ota_abort(&[OtaStatus::Aborted as u8, 0]).is_err());
    }

    #[test]
    fn ota_data_rejects_chunks_above_protocol_maximum() {
        let data = vec![0; OTA_CHUNK_MAX_LEN + 1];
        let error = serialize_ota_data(0, &data).unwrap_err().to_string();

        assert!(error.contains("protocol maximum is 1016 bytes"));
    }

    #[test]
    fn ota_data_rejects_zero_length_chunk() {
        let error = serialize_ota_data(0, &[]).unwrap_err().to_string();

        assert!(error.contains("at least one byte"));
    }

    #[test]
    fn ota_firmware_size_rejects_values_above_wire_limit() {
        if usize::BITS > u32::BITS {
            let too_large = usize::try_from(u64::from(u32::MAX) + 1).unwrap();
            let error = firmware_size_for_wire(too_large).unwrap_err().to_string();
            assert!(error.contains("32-bit size limit"));
        }
    }
}
