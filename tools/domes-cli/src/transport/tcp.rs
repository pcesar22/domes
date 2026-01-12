//! TCP socket transport for DOMES CLI
//!
//! Handles WiFi communication with the ESP32-S3 device over TCP.

use super::frame::{encode_frame, Frame, FrameDecoder, FrameError};
use anyhow::{Context, Result};
use std::io::{Read, Write};
use std::net::{TcpStream, ToSocketAddrs};
use std::time::Duration;

/// Default TCP connection settings
const DEFAULT_TIMEOUT_MS: u64 = 2000;

/// TCP transport for communicating with DOMES device over WiFi
pub struct TcpTransport {
    stream: TcpStream,
    decoder: FrameDecoder,
}

impl TcpTransport {
    /// Connect to the device at the given address
    ///
    /// Address format: "ip:port" (e.g., "192.168.1.100:5000")
    pub fn connect<A: ToSocketAddrs>(addr: A) -> Result<Self> {
        let stream = TcpStream::connect(&addr)
            .with_context(|| format!("Failed to connect to {:?}", addr.to_socket_addrs().ok().and_then(|mut a| a.next())))?;

        // Set timeouts
        stream
            .set_read_timeout(Some(Duration::from_millis(DEFAULT_TIMEOUT_MS)))
            .context("Failed to set read timeout")?;
        stream
            .set_write_timeout(Some(Duration::from_millis(DEFAULT_TIMEOUT_MS)))
            .context("Failed to set write timeout")?;

        // Disable Nagle's algorithm for low latency
        stream
            .set_nodelay(true)
            .context("Failed to set TCP_NODELAY")?;

        Ok(Self {
            stream,
            decoder: FrameDecoder::new(),
        })
    }

    /// Get the peer address
    pub fn peer_addr(&self) -> Result<String> {
        Ok(self.stream.peer_addr()?.to_string())
    }

    /// Send a frame to the device
    pub fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()> {
        let frame = encode_frame(msg_type, payload)?;
        self.stream
            .write_all(&frame)
            .context("Failed to write frame to TCP socket")?;
        self.stream.flush().context("Failed to flush TCP socket")?;
        Ok(())
    }

    /// Receive a frame from the device with timeout
    pub fn receive_frame(&mut self, timeout_ms: u64) -> Result<Frame> {
        self.decoder.reset();

        // Set the read timeout for this receive
        self.stream
            .set_read_timeout(Some(Duration::from_millis(timeout_ms)))
            .context("Failed to set read timeout")?;

        let start = std::time::Instant::now();
        let timeout = Duration::from_millis(timeout_ms);

        let mut buf = [0u8; 1];

        loop {
            if start.elapsed() > timeout {
                anyhow::bail!("Timeout waiting for response");
            }

            match self.stream.read(&mut buf) {
                Ok(1) => {
                    if let Some(result) = self.decoder.feed_byte(buf[0]) {
                        return result.map_err(|e| anyhow::anyhow!("Frame decode error: {}", e));
                    }
                }
                Ok(0) => {
                    // Connection closed
                    anyhow::bail!("Connection closed by peer");
                }
                Err(e) if e.kind() == std::io::ErrorKind::TimedOut => {
                    // Continue loop and check overall timeout
                    continue;
                }
                Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                    // Non-blocking read would block, continue
                    std::thread::sleep(Duration::from_millis(1));
                    continue;
                }
                Err(e) => {
                    return Err(e).context("Failed to read from TCP socket");
                }
            }
        }
    }

    /// Send a command and wait for response
    pub fn send_command(&mut self, msg_type: u8, payload: &[u8]) -> Result<Frame> {
        self.send_frame(msg_type, payload)?;
        self.receive_frame(DEFAULT_TIMEOUT_MS)
    }
}
