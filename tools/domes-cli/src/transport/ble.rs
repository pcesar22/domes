//! BLE transport for DOMES CLI
//!
//! Handles Bluetooth Low Energy communication with the ESP32-S3 device.
//! Uses btleplug for BLE Central role (connecting to the device as peripheral).

use super::frame::{encode_frame, Frame, FrameDecoder};
use anyhow::{bail, Context, Result};
use btleplug::api::{
    Central, Characteristic, Manager as _, Peripheral as _, ScanFilter, WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral};
use crossbeam_channel::{Receiver, Sender};
use futures::stream::StreamExt;
use std::time::{Duration, Instant};
use tokio::runtime::Runtime;
use uuid::Uuid;

/// OTA Service UUID: 12345678-1234-5678-1234-56789abcdef0
const OTA_SERVICE_UUID: Uuid = Uuid::from_u128(0x12345678_1234_5678_1234_56789abcdef0);

/// OTA Data Characteristic UUID: 12345678-1234-5678-1234-56789abcdef1 (Write)
const OTA_DATA_CHAR_UUID: Uuid = Uuid::from_u128(0x12345678_1234_5678_1234_56789abcdef1);

/// OTA Status Characteristic UUID: 12345678-1234-5678-1234-56789abcdef2 (Notify)
const OTA_STATUS_CHAR_UUID: Uuid = Uuid::from_u128(0x12345678_1234_5678_1234_56789abcdef2);

/// Default BLE operation timeout
const DEFAULT_TIMEOUT_MS: u64 = 5000;

/// Default scan timeout for device discovery
const DEFAULT_SCAN_TIMEOUT_SECS: u64 = 10;

/// Target device identifier for BLE connection
#[derive(Clone, Debug)]
pub enum BleTarget {
    /// Connect to device by advertised name (e.g., "DOMES-Pod")
    Name(String),
    /// Connect to device by Bluetooth address (e.g., "AA:BB:CC:DD:EE:FF")
    Address(String),
}

impl BleTarget {
    /// Parse a target string - if it contains colons, treat as address, otherwise name
    pub fn parse(target: &str) -> Self {
        if target.contains(':') && target.len() == 17 {
            BleTarget::Address(target.to_string())
        } else {
            BleTarget::Name(target.to_string())
        }
    }
}

/// BLE transport for communicating with DOMES device
pub struct BleTransport {
    runtime: Runtime,
    adapter: Adapter,
    peripheral: Peripheral,
    data_char: Characteristic,
    status_char: Characteristic,
    rx_receiver: Receiver<Vec<u8>>,
    decoder: FrameDecoder,
    target: BleTarget,
    device_name: String,
    auto_reconnect: bool,
}

impl BleTransport {
    /// Connect to a DOMES device via BLE
    ///
    /// # Arguments
    /// * `target` - Device name or address to connect to
    /// * `scan_timeout` - How long to scan for the device
    /// * `auto_reconnect` - Whether to auto-reconnect on disconnect
    pub fn connect(
        target: BleTarget,
        scan_timeout: Duration,
        auto_reconnect: bool,
    ) -> Result<Self> {
        let runtime = Runtime::new().context("Failed to create tokio runtime")?;

        let (adapter, peripheral, device_name) = runtime.block_on(async {
            // Get BLE manager and adapter
            let manager = Manager::new()
                .await
                .context("Failed to create BLE manager")?;

            let adapters = manager
                .adapters()
                .await
                .context("Failed to get BLE adapters")?;

            let adapter = adapters
                .into_iter()
                .next()
                .ok_or_else(|| anyhow::anyhow!("No Bluetooth adapter found"))?;

            // Start scanning
            adapter
                .start_scan(ScanFilter::default())
                .await
                .context("Failed to start BLE scan")?;

            // Find the target device
            let (peripheral, device_name) =
                find_device(&adapter, &target, scan_timeout).await?;

            // Stop scanning
            let _ = adapter.stop_scan().await;

            // Connect to the device
            peripheral
                .connect()
                .await
                .context("Failed to connect to BLE device")?;

            // Discover services
            peripheral
                .discover_services()
                .await
                .context("Failed to discover BLE services")?;

            Ok::<_, anyhow::Error>((adapter, peripheral, device_name))
        })?;

        // Find the OTA characteristics
        let (data_char, status_char) = find_ota_characteristics(&peripheral)?;

        // Subscribe to notifications on status characteristic
        runtime.block_on(async {
            peripheral
                .subscribe(&status_char)
                .await
                .context("Failed to subscribe to status notifications")
        })?;

        // Set up notification listener
        let rx_receiver = setup_notification_listener(&runtime, &peripheral)?;

        Ok(Self {
            runtime,
            adapter,
            peripheral,
            data_char,
            status_char,
            rx_receiver,
            decoder: FrameDecoder::new(),
            target,
            device_name,
            auto_reconnect,
        })
    }

    /// Scan for nearby DOMES devices
    ///
    /// Returns a list of (name, address) tuples for devices advertising the OTA service
    pub fn scan_devices(timeout: Duration) -> Result<Vec<(String, String)>> {
        let runtime = Runtime::new().context("Failed to create tokio runtime")?;

        runtime.block_on(async {
            let manager = Manager::new()
                .await
                .context("Failed to create BLE manager")?;

            let adapters = manager
                .adapters()
                .await
                .context("Failed to get BLE adapters")?;

            let adapter = adapters
                .into_iter()
                .next()
                .ok_or_else(|| anyhow::anyhow!("No Bluetooth adapter found"))?;

            // Start scanning
            adapter
                .start_scan(ScanFilter::default())
                .await
                .context("Failed to start BLE scan")?;

            let start = Instant::now();
            let mut devices = Vec::new();
            let mut seen_addresses = std::collections::HashSet::new();

            while start.elapsed() < timeout {
                let peripherals = adapter
                    .peripherals()
                    .await
                    .context("Failed to get peripherals")?;

                for p in peripherals {
                    let addr = p.address().to_string();
                    if seen_addresses.contains(&addr) {
                        continue;
                    }

                    if let Ok(Some(props)) = p.properties().await {
                        // Check if this device advertises the OTA service or has DOMES in name
                        let name = props.local_name.unwrap_or_default();
                        let is_domes = name.contains("DOMES")
                            || props.services.contains(&OTA_SERVICE_UUID);

                        if is_domes {
                            seen_addresses.insert(addr.clone());
                            devices.push((name, addr));
                        }
                    }
                }

                tokio::time::sleep(Duration::from_millis(200)).await;
            }

            let _ = adapter.stop_scan().await;

            Ok(devices)
        })
    }

    /// Get the connected device name
    pub fn device_name(&self) -> &str {
        &self.device_name
    }

    /// Get the connected device address
    pub fn device_address(&self) -> String {
        self.peripheral.address().to_string()
    }

    /// Check if still connected
    pub fn is_connected(&self) -> bool {
        self.runtime
            .block_on(self.peripheral.is_connected())
            .unwrap_or(false)
    }

    /// Get negotiated MTU (if available)
    pub fn mtu(&self) -> Option<u16> {
        // btleplug doesn't expose MTU directly, return None
        // The actual MTU negotiation happens during connection
        None
    }

    /// Send a frame to the device
    pub fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()> {
        self.ensure_connected()?;

        let frame = encode_frame(msg_type, payload)?;

        self.runtime.block_on(async {
            self.peripheral
                .write(&self.data_char, &frame, WriteType::WithoutResponse)
                .await
                .context("Failed to write to BLE characteristic")
        })?;

        Ok(())
    }

    /// Receive a frame from the device with timeout
    pub fn receive_frame(&mut self, timeout_ms: u64) -> Result<Frame> {
        self.decoder.reset();

        let timeout = Duration::from_millis(timeout_ms);
        let start = Instant::now();

        loop {
            let remaining = timeout.saturating_sub(start.elapsed());
            if remaining.is_zero() {
                bail!("Timeout waiting for BLE response");
            }

            match self.rx_receiver.recv_timeout(remaining) {
                Ok(data) => {
                    for byte in data {
                        if let Some(result) = self.decoder.feed_byte(byte) {
                            return result
                                .map_err(|e| anyhow::anyhow!("Frame decode error: {}", e));
                        }
                    }
                }
                Err(crossbeam_channel::RecvTimeoutError::Timeout) => {
                    bail!("Timeout waiting for BLE response");
                }
                Err(crossbeam_channel::RecvTimeoutError::Disconnected) => {
                    if self.auto_reconnect {
                        self.reconnect()?;
                    } else {
                        bail!("BLE connection lost");
                    }
                }
            }
        }
    }

    /// Send a command and wait for response
    pub fn send_command(&mut self, msg_type: u8, payload: &[u8]) -> Result<Frame> {
        self.send_frame(msg_type, payload)?;
        self.receive_frame(DEFAULT_TIMEOUT_MS)
    }

    /// Ensure we're still connected, reconnect if needed
    fn ensure_connected(&mut self) -> Result<()> {
        if !self.is_connected() {
            if self.auto_reconnect {
                eprintln!("BLE connection lost, reconnecting...");
                self.reconnect()?;
            } else {
                bail!("BLE connection lost");
            }
        }
        Ok(())
    }

    /// Reconnect to the device
    fn reconnect(&mut self) -> Result<()> {
        self.runtime.block_on(async {
            // Try to connect again
            self.peripheral
                .connect()
                .await
                .context("Failed to reconnect to BLE device")?;

            // Re-subscribe to notifications
            self.peripheral
                .subscribe(&self.status_char)
                .await
                .context("Failed to re-subscribe to notifications")?;

            Ok::<(), anyhow::Error>(())
        })?;

        // Set up new notification listener
        self.rx_receiver = setup_notification_listener(&self.runtime, &self.peripheral)?;

        eprintln!("Reconnected to {}", self.device_name);
        Ok(())
    }
}

/// Find a device by name or address
async fn find_device(
    adapter: &Adapter,
    target: &BleTarget,
    timeout: Duration,
) -> Result<(Peripheral, String)> {
    let start = Instant::now();

    while start.elapsed() < timeout {
        let peripherals = adapter
            .peripherals()
            .await
            .context("Failed to get peripherals")?;

        for p in peripherals {
            if let Ok(Some(props)) = p.properties().await {
                let name = props.local_name.clone().unwrap_or_default();
                let addr = p.address().to_string();

                let matches = match target {
                    BleTarget::Name(target_name) => {
                        name.contains(target_name) || name == *target_name
                    }
                    BleTarget::Address(target_addr) => {
                        addr.eq_ignore_ascii_case(target_addr)
                    }
                };

                if matches {
                    return Ok((p, name));
                }
            }
        }

        tokio::time::sleep(Duration::from_millis(100)).await;
    }

    match target {
        BleTarget::Name(name) => bail!("Device '{}' not found after {}s", name, timeout.as_secs()),
        BleTarget::Address(addr) => {
            bail!("Device {} not found after {}s", addr, timeout.as_secs())
        }
    }
}

/// Find the OTA service characteristics
fn find_ota_characteristics(peripheral: &Peripheral) -> Result<(Characteristic, Characteristic)> {
    let services = peripheral.services();

    let ota_service = services
        .iter()
        .find(|s| s.uuid == OTA_SERVICE_UUID)
        .ok_or_else(|| {
            anyhow::anyhow!("OTA service not found. Is the device running DOMES firmware?")
        })?;

    let data_char = ota_service
        .characteristics
        .iter()
        .find(|c| c.uuid == OTA_DATA_CHAR_UUID)
        .cloned()
        .ok_or_else(|| anyhow::anyhow!("OTA Data characteristic not found"))?;

    let status_char = ota_service
        .characteristics
        .iter()
        .find(|c| c.uuid == OTA_STATUS_CHAR_UUID)
        .cloned()
        .ok_or_else(|| anyhow::anyhow!("OTA Status characteristic not found"))?;

    Ok((data_char, status_char))
}

/// Set up a background task to listen for notifications and forward to channel
fn setup_notification_listener(
    runtime: &Runtime,
    peripheral: &Peripheral,
) -> Result<Receiver<Vec<u8>>> {
    let (tx, rx): (Sender<Vec<u8>>, Receiver<Vec<u8>>) = crossbeam_channel::bounded(32);

    let mut notification_stream = runtime
        .block_on(peripheral.notifications())
        .context("Failed to get notification stream")?;

    runtime.spawn(async move {
        while let Some(notification) = notification_stream.next().await {
            if notification.uuid == OTA_STATUS_CHAR_UUID {
                if tx.send(notification.value).is_err() {
                    // Receiver dropped, exit
                    break;
                }
            }
        }
    });

    Ok(rx)
}

impl Drop for BleTransport {
    fn drop(&mut self) {
        // Disconnect cleanly
        let _ = self.runtime.block_on(async {
            let _ = self.peripheral.unsubscribe(&self.status_char).await;
            let _ = self.peripheral.disconnect().await;
        });
    }
}
