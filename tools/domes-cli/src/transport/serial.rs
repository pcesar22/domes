//! Serial port transport for DOMES CLI
//!
//! Handles USB CDC communication with the ESP32-S3 device.

use super::frame::{encode_frame, Frame, FrameDecoder, FrameError};
use anyhow::{Context, Result};
use serialport::SerialPort;
use std::io::{Read, Write};
use std::time::Duration;

/// Default serial port settings
const DEFAULT_BAUD_RATE: u32 = 115200;
const DEFAULT_TIMEOUT_MS: u64 = 1000;

/// Serial transport for communicating with DOMES device
pub struct SerialTransport {
    port: Box<dyn SerialPort>,
    decoder: FrameDecoder,
}

impl SerialTransport {
    /// Open a serial connection to the device
    pub fn open(port_name: &str) -> Result<Self> {
        let port = serialport::new(port_name, DEFAULT_BAUD_RATE)
            .timeout(Duration::from_millis(DEFAULT_TIMEOUT_MS))
            .open()
            .with_context(|| format!("Failed to open serial port: {}", port_name))?;

        Ok(Self {
            port,
            decoder: FrameDecoder::new(),
        })
    }

    /// Send a frame to the device
    pub fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()> {
        let frame = encode_frame(msg_type, payload)?;
        self.port
            .write_all(&frame)
            .context("Failed to write frame to serial port")?;
        self.port.flush().context("Failed to flush serial port")?;
        Ok(())
    }

    /// Receive a frame from the device with timeout
    pub fn receive_frame(&mut self, timeout_ms: u64) -> Result<Frame> {
        self.decoder.reset();

        let start = std::time::Instant::now();
        let timeout = Duration::from_millis(timeout_ms);

        let mut buf = [0u8; 1];

        loop {
            if start.elapsed() > timeout {
                anyhow::bail!("Timeout waiting for response");
            }

            match self.port.read(&mut buf) {
                Ok(1) => {
                    if let Some(result) = self.decoder.feed_byte(buf[0]) {
                        return result.map_err(|e| anyhow::anyhow!("Frame decode error: {}", e));
                    }
                }
                Ok(0) => {
                    // No data available, continue waiting
                    std::thread::sleep(Duration::from_millis(1));
                }
                Err(e) if e.kind() == std::io::ErrorKind::TimedOut => {
                    // Timeout on read, continue loop and check overall timeout
                    continue;
                }
                Err(e) => {
                    return Err(e).context("Failed to read from serial port");
                }
            }
        }
    }

    /// Send a command and wait for response
    pub fn send_command(&mut self, msg_type: u8, payload: &[u8]) -> Result<Frame> {
        self.send_frame(msg_type, payload)?;
        self.receive_frame(DEFAULT_TIMEOUT_MS)
    }

    /// List available serial ports
    pub fn list_ports() -> Result<Vec<String>> {
        let ports = serialport::available_ports().context("Failed to enumerate serial ports")?;

        Ok(ports.into_iter().map(|p| p.port_name).collect())
    }
}
