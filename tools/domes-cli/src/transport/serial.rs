//! Serial port transport for DOMES CLI
//!
//! Handles UART communication through the NFF board's CP2102N bridge.

use super::frame::{encode_frame, Frame, FrameDecoder};
use anyhow::{Context, Result};
use serialport::SerialPort;
use std::io::{Read, Write};
use std::time::Duration;

#[cfg(unix)]
use std::os::fd::AsRawFd;

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
        Ok(Self {
            port: open_serial_port(port_name, Duration::from_millis(DEFAULT_TIMEOUT_MS))?,
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
                Ok(_) => {
                    // Unexpected: more bytes than buffer size (shouldn't happen with 1-byte buffer)
                    continue;
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

/// Open the board UART without pulsing the ESP32-S3 auto-reset lines.
pub(crate) fn open_serial_port(port_name: &str, timeout: Duration) -> Result<Box<dyn SerialPort>> {
    let builder = serialport::new(port_name, DEFAULT_BAUD_RATE)
        .timeout(timeout)
        // The ESP32-S3 DevKit auto-reset circuit is connected to the
        // CP2102N modem-control lines. Linux asserts DTR and RTS together
        // on open; preserve that interlocked state until we can release
        // RTS before DTR below.
        .preserve_dtr_on_open();

    #[cfg(unix)]
    let mut port: Box<dyn SerialPort> = {
        let native = builder
            .open_native()
            .with_context(|| format!("Failed to open serial port: {}", port_name))?;
        disable_hangup_on_close(&native)?;
        Box::new(native)
    };

    #[cfg(not(unix))]
    let mut port = builder
        .open()
        .with_context(|| format!("Failed to open serial port: {}", port_name))?;

    port.write_request_to_send(false)
        .context("Failed to deassert serial RTS")?;
    port.write_data_terminal_ready(false)
        .context("Failed to deassert serial DTR")?;

    Ok(port)
}

#[cfg(unix)]
fn disable_hangup_on_close(port: &serialport::TTYPort) -> Result<()> {
    let fd = port.as_raw_fd();
    let mut attributes = std::mem::MaybeUninit::<libc::termios>::uninit();

    // SAFETY: fd is owned by a live TTYPort and attributes points to writable
    // storage for tcgetattr to initialize.
    if unsafe { libc::tcgetattr(fd, attributes.as_mut_ptr()) } != 0 {
        return Err(std::io::Error::last_os_error()).context("Failed to read serial attributes");
    }

    // SAFETY: tcgetattr succeeded, so attributes is fully initialized.
    let mut attributes = unsafe { attributes.assume_init() };
    attributes.c_cflag &= !libc::HUPCL;

    // SAFETY: fd remains valid and attributes is a valid termios structure.
    if unsafe { libc::tcsetattr(fd, libc::TCSANOW, &attributes) } != 0 {
        return Err(std::io::Error::last_os_error())
            .context("Failed to disable serial hangup-on-close");
    }

    Ok(())
}
