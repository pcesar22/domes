//! OTA update commands
//!
//! Sends firmware updates to DOMES devices over serial or WiFi.

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
    fn from_u8(value: u8) -> Self {
        match value {
            0 => OtaStatus::Ok,
            1 => OtaStatus::Busy,
            2 => OtaStatus::FlashError,
            3 => OtaStatus::VerifyFailed,
            4 => OtaStatus::SizeMismatch,
            5 => OtaStatus::OffsetMismatch,
            6 => OtaStatus::VersionError,
            7 => OtaStatus::PartitionError,
            _ => OtaStatus::Aborted,
        }
    }

    fn to_string(&self) -> &'static str {
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

/// OTA chunk size (matches firmware kOtaChunkSize)
const OTA_CHUNK_SIZE: usize = 1016;

/// SHA256 size
const SHA256_SIZE: usize = 32;

/// Version string max length
const VERSION_MAX_LEN: usize = 32;

/// Timeout for OTA operations (ms)
const OTA_TIMEOUT_MS: u64 = 5000;

/// Timeout for OTA_END (device reboots) (ms)
const OTA_END_TIMEOUT_MS: u64 = 30000;

/// Send firmware OTA update to device
pub fn ota_flash(
    transport: &mut dyn Transport,
    firmware_path: &Path,
    version: Option<&str>,
) -> Result<()> {
    // Read firmware file
    println!("Reading firmware from '{}'...", firmware_path.display());
    let firmware = read_firmware_file(firmware_path)?;
    println!("Firmware size: {} bytes", firmware.len());

    // Compute SHA256
    println!("Computing SHA256...");
    let sha256 = compute_sha256(&firmware);
    print!("SHA256: ");
    for byte in &sha256 {
        print!("{:02x}", byte);
    }
    println!();

    let version_str = version.unwrap_or("unknown");

    // Send OTA_BEGIN
    println!("Sending OTA_BEGIN (version: {})...", version_str);
    let begin_payload = serialize_ota_begin(firmware.len() as u32, &sha256, version_str);

    let (status, _next_offset) =
        send_and_wait_ack(transport, OtaMsgType::Begin, &begin_payload, OTA_TIMEOUT_MS)?;

    if status != OtaStatus::Ok {
        anyhow::bail!("Device rejected OTA_BEGIN: {}", status.to_string());
    }
    println!("Device accepted OTA_BEGIN.");

    // Send firmware chunks
    println!("Sending firmware data...");
    let mut offset: usize = 0;
    let total = firmware.len();

    while offset < total {
        let chunk_size = std::cmp::min(OTA_CHUNK_SIZE, total - offset);
        let chunk = &firmware[offset..offset + chunk_size];

        let data_payload = serialize_ota_data(offset as u32, chunk);

        let (status, _next_offset) =
            send_and_wait_ack(transport, OtaMsgType::Data, &data_payload, OTA_TIMEOUT_MS)?;

        if status != OtaStatus::Ok {
            anyhow::bail!(
                "Device rejected chunk at offset {}: {}",
                offset,
                status.to_string()
            );
        }

        offset += chunk_size;
        print_progress(offset, total);
    }
    println!();

    // Send OTA_END
    println!("Sending OTA_END...");
    let (status, _) = send_and_wait_ack(transport, OtaMsgType::End, &[], OTA_END_TIMEOUT_MS)?;

    if status != OtaStatus::Ok {
        anyhow::bail!("Device rejected OTA_END: {}", status.to_string());
    }

    println!("\nOTA complete! Device will reboot.");
    Ok(())
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
fn serialize_ota_begin(firmware_size: u32, sha256: &[u8; 32], version: &str) -> Vec<u8> {
    let mut payload = Vec::with_capacity(4 + 32 + 32);

    // Firmware size (little-endian)
    payload.extend_from_slice(&firmware_size.to_le_bytes());

    // SHA256
    payload.extend_from_slice(sha256);

    // Version (null-terminated, padded to 32 bytes)
    let mut version_bytes = [0u8; VERSION_MAX_LEN];
    let version_slice = version.as_bytes();
    let copy_len = std::cmp::min(version_slice.len(), VERSION_MAX_LEN - 1);
    version_bytes[..copy_len].copy_from_slice(&version_slice[..copy_len]);
    payload.extend_from_slice(&version_bytes);

    payload
}

/// Serialize OTA_DATA payload
/// Format: [u32 offset][u16 length][data...]
fn serialize_ota_data(offset: u32, data: &[u8]) -> Vec<u8> {
    let mut payload = Vec::with_capacity(4 + 2 + data.len());

    // Offset (little-endian)
    payload.extend_from_slice(&offset.to_le_bytes());

    // Length (little-endian)
    payload.extend_from_slice(&(data.len() as u16).to_le_bytes());

    // Data
    payload.extend_from_slice(data);

    payload
}

/// Deserialize OTA_ACK payload
/// Format: [u8 status][u32 nextOffset]
fn deserialize_ota_ack(payload: &[u8]) -> Result<(OtaStatus, u32)> {
    if payload.len() < 5 {
        anyhow::bail!(
            "OTA_ACK payload too short: {} bytes, expected 5",
            payload.len()
        );
    }

    let status = OtaStatus::from_u8(payload[0]);
    let next_offset = u32::from_le_bytes([payload[1], payload[2], payload[3], payload[4]]);

    Ok((status, next_offset))
}

/// Deserialize OTA_ABORT payload
fn deserialize_ota_abort(payload: &[u8]) -> Result<OtaStatus> {
    if payload.is_empty() {
        anyhow::bail!("OTA_ABORT payload empty");
    }
    Ok(OtaStatus::from_u8(payload[0]))
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
            anyhow::bail!("Device aborted OTA: {}", reason.to_string())
        }
        _ => {
            anyhow::bail!("Unexpected response type: 0x{:02X}", frame.msg_type)
        }
    }
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
    print!(
        "] {} / {} bytes ({:.1}%)",
        current,
        total,
        progress * 100.0
    );
    std::io::stdout().flush().ok();
}
