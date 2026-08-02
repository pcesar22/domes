//! BLE transport for DOMES CLI
//!
//! Handles Bluetooth Low Energy communication with the ESP32-S3 device.
//! Uses btleplug for BLE Central role (connecting to the device as peripheral).

use super::frame::{encode_frame, Frame, FrameDecoder};
use crate::proto::config::{MsgType as ConfigMsgType, TouchEventNotification};
use anyhow::{bail, Context, Result};
use btleplug::api::{
    Central, Characteristic, Manager as _, Peripheral as _, ScanFilter, WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral};
use crossbeam_channel::{Receiver, Sender};
use futures::stream::StreamExt;
use prost::Message;
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

/// ATT payload guaranteed by the minimum BLE MTU (23 bytes minus 3 bytes overhead).
const SAFE_WRITE_CHUNK_SIZE: usize = 20;

/// Allow a short discovery window after the first name match so an exact or
/// second matching advertisement can arrive before selecting a device.
const NAME_MATCH_SETTLE_TIME: Duration = Duration::from_millis(750);

const TOUCH_EVENT_NOTIFICATION_MSG_TYPE: u8 = ConfigMsgType::TouchEventNtf as u8;

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
    peripheral: Peripheral,
    data_char: Characteristic,
    status_char: Characteristic,
    rx_receiver: Receiver<Vec<u8>>,
    decoder: FrameDecoder,
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
            let (peripheral, device_name) = find_device(&adapter, &target, scan_timeout).await?;

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

        // adapter and target are not stored as they're not needed after connection
        let _ = adapter;

        Ok(Self {
            runtime,
            peripheral,
            data_char,
            status_char,
            rx_receiver,
            decoder: FrameDecoder::new(),
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
                        let is_domes =
                            name.contains("DOMES") || props.services.contains(&OTA_SERVICE_UUID);

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

    /// Check if still connected
    pub fn is_connected(&self) -> bool {
        self.runtime
            .block_on(self.peripheral.is_connected())
            .unwrap_or(false)
    }

    /// Send a frame to the device
    pub fn send_frame(&mut self, msg_type: u8, payload: &[u8]) -> Result<()> {
        self.ensure_connected()?;

        let frame = encode_frame(msg_type, payload)?;

        self.runtime.block_on(async {
            // A characteristic write is atomic: backends reject values larger
            // than the negotiated ATT payload before delivery. Try the complete
            // frame for normal high-MTU links, then retry from byte zero at the
            // minimum guaranteed payload when the backend rejects it.
            match self
                .peripheral
                .write(&self.data_char, &frame, WriteType::WithoutResponse)
                .await
            {
                Ok(()) => Ok::<(), anyhow::Error>(()),
                Err(full_write_error) if frame.len() > SAFE_WRITE_CHUNK_SIZE => {
                    for chunk in frame.chunks(SAFE_WRITE_CHUNK_SIZE) {
                        self.peripheral
                            .write(&self.data_char, chunk, WriteType::WithoutResponse)
                            .await
                            .with_context(|| {
                                format!(
                                    "Failed to write BLE frame fragment after full write was rejected: {full_write_error}"
                                )
                            })?;
                    }
                    Ok(())
                }
                Err(error) => Err(error).context("Failed to write BLE characteristic"),
            }
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
                            let frame =
                                result.map_err(|e| anyhow::anyhow!("Frame decode error: {}", e))?;
                            if let Some(response) = route_received_frame(frame) {
                                return Ok(response);
                            }
                            self.decoder.reset();
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
        self.decoder.reset();

        eprintln!("Reconnected to {}", self.device_name);
        Ok(())
    }
}

fn route_received_frame(frame: Frame) -> Option<Frame> {
    if frame.msg_type != TOUCH_EVENT_NOTIFICATION_MSG_TYPE {
        return Some(frame);
    }

    match TouchEventNotification::decode(frame.payload.as_slice()) {
        Ok(event) => eprintln!(
            "BLE touch event: pod={} pad={} timestamp_us={}",
            event.pod_id, event.pad_index, event.timestamp_us
        ),
        Err(error) => eprintln!("Malformed BLE touch event notification: {error}"),
    }
    None
}

/// Find a device by name or address
async fn find_device(
    adapter: &Adapter,
    target: &BleTarget,
    timeout: Duration,
) -> Result<(Peripheral, String)> {
    let start = Instant::now();
    let mut pending_name_match: Option<(Peripheral, String, String, Instant)> = None;

    while start.elapsed() < timeout {
        let peripherals = adapter
            .peripherals()
            .await
            .context("Failed to get peripherals")?;

        let mut named_candidates = Vec::new();

        for p in peripherals {
            if let Ok(Some(props)) = p.properties().await {
                let name = props.local_name.clone().unwrap_or_default();
                let addr = p.address().to_string();

                match target {
                    BleTarget::Address(target_addr) if addr.eq_ignore_ascii_case(target_addr) => {
                        return Ok((p, name));
                    }
                    BleTarget::Name(_) => named_candidates.push((p, name, addr)),
                    BleTarget::Address(_) => {}
                }
            }
        }

        if let BleTarget::Name(target_name) = target {
            let summaries: Vec<_> = named_candidates
                .iter()
                .map(|(_, name, address)| (name.clone(), address.clone()))
                .collect();
            if let Some(index) = select_name_match(&summaries, target_name)? {
                let (peripheral, name, address) = named_candidates.swap_remove(index);
                let first_seen = pending_name_match
                    .as_ref()
                    .filter(|(_, _, pending_address, _)| pending_address == &address)
                    .map(|(_, _, _, first_seen)| *first_seen)
                    .unwrap_or_else(Instant::now);

                if first_seen.elapsed() >= NAME_MATCH_SETTLE_TIME {
                    return Ok((peripheral, name));
                }
                pending_name_match = Some((peripheral, name, address, first_seen));
            } else {
                pending_name_match = None;
            }
        }

        tokio::time::sleep(Duration::from_millis(100)).await;
    }

    if let Some((peripheral, name, _, _)) = pending_name_match {
        return Ok((peripheral, name));
    }

    match target {
        BleTarget::Name(name) => bail!("Device '{}' not found after {}s", name, timeout.as_secs()),
        BleTarget::Address(addr) => {
            bail!("Device {} not found after {}s", addr, timeout.as_secs())
        }
    }
}

fn select_name_match(candidates: &[(String, String)], target_name: &str) -> Result<Option<usize>> {
    if target_name.is_empty() {
        anyhow::bail!("BLE device name must not be empty");
    }

    let exact: Vec<_> = candidates
        .iter()
        .enumerate()
        .filter(|(_, (name, _))| name == target_name)
        .collect();
    let matches: Vec<_> = if exact.is_empty() {
        candidates
            .iter()
            .enumerate()
            .filter(|(_, (name, _))| name.contains(target_name))
            .collect()
    } else {
        exact
    };

    match matches.as_slice() {
        [] => Ok(None),
        [(index, _)] => Ok(Some(*index)),
        _ => {
            let descriptions = matches
                .iter()
                .map(|(_, (name, address))| format!("{} ({})", name, address))
                .collect::<Vec<_>>()
                .join(", ");
            anyhow::bail!(
                "BLE name '{}' is ambiguous; matched {}. Use --ble with a device address.",
                target_name,
                descriptions
            )
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
            if notification.uuid == OTA_STATUS_CHAR_UUID && tx.send(notification.value).is_err() {
                // Receiver dropped, exit
                break;
            }
        }
    });

    Ok(rx)
}

impl Drop for BleTransport {
    fn drop(&mut self) {
        // Disconnect cleanly
        self.runtime.block_on(async {
            let _ = self.peripheral.unsubscribe(&self.status_char).await;
            let _ = self.peripheral.disconnect().await;
        });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn exact_ble_name_wins_over_partial_matches() {
        let candidates = vec![
            ("DOMES-Pod-01".to_string(), "AA:00".to_string()),
            ("DOMES-Pod".to_string(), "BB:00".to_string()),
        ];

        assert_eq!(
            select_name_match(&candidates, "DOMES-Pod").unwrap(),
            Some(1)
        );
    }

    #[test]
    fn ambiguous_ble_name_requires_an_address() {
        let candidates = vec![
            ("DOMES-Pod-01".to_string(), "AA:00".to_string()),
            ("DOMES-Pod-02".to_string(), "BB:00".to_string()),
        ];

        let error = select_name_match(&candidates, "DOMES-Pod")
            .unwrap_err()
            .to_string();
        assert!(error.contains("ambiguous"));
        assert!(error.contains("AA:00"));
        assert!(error.contains("BB:00"));
    }

    #[test]
    fn empty_ble_name_is_rejected() {
        let error = select_name_match(&[], "").unwrap_err().to_string();
        assert!(error.contains("must not be empty"));
    }

    #[test]
    fn touch_notifications_are_observed_instead_of_returned_as_responses() {
        assert_eq!(TOUCH_EVENT_NOTIFICATION_MSG_TYPE, 0x50);
        let touch = Frame {
            msg_type: TOUCH_EVENT_NOTIFICATION_MSG_TYPE,
            payload: vec![0x08, 0x01, 0x10, 0x02],
        };

        assert!(route_received_frame(touch).is_none());

        let response = Frame {
            msg_type: 0x21,
            payload: vec![],
        };
        assert_eq!(route_received_frame(response).unwrap().msg_type, 0x21);
    }
}
