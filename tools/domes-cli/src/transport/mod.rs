//! Transport layer for DOMES CLI
//!
//! Provides frame encoding/decoding and communication over serial, TCP, or BLE.

pub mod ble;
pub mod frame;
pub mod serial;
pub mod tcp;

pub use ble::{BleTarget, BleTransport};
pub use frame::Frame;
pub use serial::SerialTransport;
pub use tcp::TcpTransport;

use anyhow::Result;

/// Default OTA chunk size for serial/TCP (matches firmware kOtaChunkSize)
pub const OTA_CHUNK_SIZE_DEFAULT: usize = 1016;

/// BLE OTA chunk size - smaller to fit within BLE MTU limits
/// BLE MTU is typically 512 bytes max, with ATT overhead of 3 bytes = 509 bytes usable
/// Frame overhead is 9 bytes, so max payload is ~500 bytes
/// Using 400 bytes to leave margin for safety
pub const OTA_CHUNK_SIZE_BLE: usize = 400;

/// Transport trait for abstracting serial vs TCP vs BLE communication
pub trait Transport {
    /// Send a frame to the device
    fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()>;

    /// Receive a frame from the device with timeout
    fn receive_frame(&mut self, timeout_ms: u64) -> Result<Frame>;

    /// Send a command and wait for response
    fn send_command(&mut self, msg_type: u8, payload: &[u8]) -> Result<Frame>;

    /// Get the maximum OTA chunk size for this transport
    /// BLE has lower limits due to MTU constraints
    fn max_ota_chunk_size(&self) -> usize {
        OTA_CHUNK_SIZE_DEFAULT
    }
}

impl Transport for SerialTransport {
    fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()> {
        self.send_frame(msg_type, payload)
    }

    fn receive_frame(&mut self, timeout_ms: u64) -> Result<Frame> {
        self.receive_frame(timeout_ms)
    }

    fn send_command(&mut self, msg_type: u8, payload: &[u8]) -> Result<Frame> {
        self.send_command(msg_type, payload)
    }
}

impl Transport for TcpTransport {
    fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()> {
        self.send_frame(msg_type, payload)
    }

    fn receive_frame(&mut self, timeout_ms: u64) -> Result<Frame> {
        self.receive_frame(timeout_ms)
    }

    fn send_command(&mut self, msg_type: u8, payload: &[u8]) -> Result<Frame> {
        self.send_command(msg_type, payload)
    }
}

impl Transport for BleTransport {
    fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()> {
        self.send_frame(msg_type, payload)
    }

    fn receive_frame(&mut self, timeout_ms: u64) -> Result<Frame> {
        self.receive_frame(timeout_ms)
    }

    fn send_command(&mut self, msg_type: u8, payload: &[u8]) -> Result<Frame> {
        self.send_command(msg_type, payload)
    }

    fn max_ota_chunk_size(&self) -> usize {
        OTA_CHUNK_SIZE_BLE
    }
}
