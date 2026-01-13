//! Transport layer for DOMES CLI
//!
//! Provides frame encoding/decoding and communication over serial or TCP.

pub mod frame;
pub mod serial;
pub mod tcp;

pub use frame::Frame;
pub use serial::SerialTransport;
pub use tcp::TcpTransport;

use anyhow::Result;

/// Transport trait for abstracting serial vs TCP communication
pub trait Transport {
    /// Send a frame to the device
    fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()>;

    /// Receive a frame from the device with timeout
    fn receive_frame(&mut self, timeout_ms: u64) -> Result<Frame>;

    /// Send a command and wait for response
    fn send_command(&mut self, msg_type: u8, payload: &[u8]) -> Result<Frame>;
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
