//! Multi-device support for DOMES CLI
//!
//! Provides device targeting, registry, and multi-transport management.

use crate::transport::{BleTarget, BleTransport, SerialTransport, TcpTransport, Transport};
use anyhow::{Context, Result};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use std::time::Duration;

/// A named device connection
pub struct DeviceConnection {
    pub name: String,
    pub transport: Box<dyn Transport>,
}

/// Device registry entry from config file
#[derive(Debug, Clone)]
pub struct DeviceEntry {
    pub name: String,
    pub transport_type: String,
    pub address: String,
}

/// Parse devices.toml config file
///
/// Format:
/// ```toml
/// [devices.pod1]
/// transport = "serial"
/// address = "/dev/ttyACM0"
///
/// [devices.pod2]
/// transport = "serial"
/// address = "/dev/ttyACM1"
/// ```
pub fn load_device_registry() -> Result<HashMap<String, DeviceEntry>> {
    let config_path = get_config_path();
    if !config_path.exists() {
        return Ok(HashMap::new());
    }

    let content = fs::read_to_string(&config_path)
        .with_context(|| format!("Failed to read {}", config_path.display()))?;

    parse_devices_toml(&content)
}

/// Save a device entry to the registry
pub fn save_device_entry(name: &str, entry: &DeviceEntry) -> Result<()> {
    let config_path = get_config_path();

    // Ensure directory exists
    if let Some(parent) = config_path.parent() {
        fs::create_dir_all(parent)?;
    }

    let mut devices = load_device_registry().unwrap_or_default();
    devices.insert(name.to_string(), entry.clone());

    let content = serialize_devices_toml(&devices);
    fs::write(&config_path, content)?;
    Ok(())
}

/// Remove a device from the registry
pub fn remove_device_entry(name: &str) -> Result<bool> {
    let config_path = get_config_path();
    if !config_path.exists() {
        return Ok(false);
    }

    let mut devices = load_device_registry()?;
    let removed = devices.remove(name).is_some();

    if removed {
        let content = serialize_devices_toml(&devices);
        fs::write(&config_path, content)?;
    }
    Ok(removed)
}

/// Connect to a device by registry entry
pub fn connect_device(entry: &DeviceEntry) -> Result<Box<dyn Transport>> {
    match entry.transport_type.as_str() {
        "serial" => {
            let transport = SerialTransport::open(&entry.address)?;
            Ok(Box::new(transport))
        }
        "wifi" | "tcp" => {
            let transport = TcpTransport::connect(&entry.address)?;
            Ok(Box::new(transport))
        }
        "ble" => {
            let target = BleTarget::parse(&entry.address);
            let transport = BleTransport::connect(target, Duration::from_secs(10), true)?;
            Ok(Box::new(transport))
        }
        other => anyhow::bail!("Unknown transport type: {}", other),
    }
}

/// Resolve CLI arguments into device connections
///
/// Priority:
/// 1. --target names (look up in registry)
/// 2. --port / --wifi / --ble (direct connections)
/// 3. If --all, connect to all registry devices
pub fn resolve_devices(
    ports: &[String],
    wifis: &[String],
    bles: &[String],
    targets: &[String],
    all: bool,
) -> Result<Vec<DeviceConnection>> {
    let mut connections = Vec::new();

    // If --all, load entire registry
    if all {
        let registry = load_device_registry()?;
        if registry.is_empty() {
            anyhow::bail!("No devices in registry. Use 'devices add' to register devices.");
        }
        for (name, entry) in &registry {
            println!(
                "Connecting to {} ({} @ {})...",
                name, entry.transport_type, entry.address
            );
            let transport = connect_device(entry)
                .with_context(|| format!("Failed to connect to {}", name))?;
            connections.push(DeviceConnection {
                name: name.clone(),
                transport,
            });
        }
        return Ok(connections);
    }

    // If --target, look up in registry
    if !targets.is_empty() {
        let registry = load_device_registry()?;
        for target_name in targets {
            let entry = registry
                .get(target_name)
                .with_context(|| format!("Device '{}' not found in registry", target_name))?;
            println!(
                "Connecting to {} ({} @ {})...",
                target_name, entry.transport_type, entry.address
            );
            let transport = connect_device(entry)
                .with_context(|| format!("Failed to connect to {}", target_name))?;
            connections.push(DeviceConnection {
                name: target_name.clone(),
                transport,
            });
        }
    }

    // Direct connections via --port
    for (i, port) in ports.iter().enumerate() {
        let name = if ports.len() == 1 && wifis.is_empty() && bles.is_empty() && targets.is_empty()
        {
            // Single device, no label needed (backward compat)
            String::new()
        } else {
            format!("serial-{}", i)
        };
        let transport = SerialTransport::open(port)?;
        connections.push(DeviceConnection {
            name,
            transport: Box::new(transport),
        });
    }

    // Direct connections via --wifi
    for (i, addr) in wifis.iter().enumerate() {
        let name = if wifis.len() == 1 && ports.is_empty() && bles.is_empty() && targets.is_empty()
        {
            String::new()
        } else {
            format!("wifi-{}", i)
        };
        println!("Connecting to {} via WiFi...", addr);
        let transport = TcpTransport::connect(addr)?;
        connections.push(DeviceConnection {
            name,
            transport: Box::new(transport),
        });
    }

    // Direct connections via --ble
    for (i, ble_target) in bles.iter().enumerate() {
        let name = if bles.len() == 1 && ports.is_empty() && wifis.is_empty() && targets.is_empty()
        {
            String::new()
        } else {
            format!("ble-{}", i)
        };
        println!("Scanning for BLE device '{}'...", ble_target);
        let target = BleTarget::parse(ble_target);
        let transport = BleTransport::connect(target, Duration::from_secs(10), true)?;
        connections.push(DeviceConnection {
            name,
            transport: Box::new(transport),
        });
    }

    Ok(connections)
}

/// Format a device label prefix for output
pub fn device_prefix(name: &str) -> String {
    if name.is_empty() {
        String::new()
    } else {
        format!("[{}] ", name)
    }
}

fn get_config_path() -> PathBuf {
    let home = std::env::var("HOME").unwrap_or_else(|_| ".".to_string());
    PathBuf::from(home).join(".domes").join("devices.toml")
}

/// Simple TOML parser for devices (avoids adding toml dependency)
fn parse_devices_toml(content: &str) -> Result<HashMap<String, DeviceEntry>> {
    let mut devices = HashMap::new();
    let mut current_name: Option<String> = None;
    let mut current_transport = String::new();
    let mut current_address = String::new();

    for line in content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }

        // Parse [devices.name]
        if line.starts_with("[devices.") && line.ends_with(']') {
            // Save previous device if any
            if let Some(name) = current_name.take() {
                if !current_transport.is_empty() && !current_address.is_empty() {
                    devices.insert(
                        name.clone(),
                        DeviceEntry {
                            name,
                            transport_type: current_transport.clone(),
                            address: current_address.clone(),
                        },
                    );
                }
            }
            current_name = Some(line[9..line.len() - 1].to_string());
            current_transport.clear();
            current_address.clear();
        } else if let Some((_key, value)) = line.split_once('=') {
            let key = _key.trim();
            let value = value.trim().trim_matches('"');
            match key {
                "transport" => current_transport = value.to_string(),
                "address" => current_address = value.to_string(),
                _ => {}
            }
        }
    }

    // Save last device
    if let Some(name) = current_name {
        if !current_transport.is_empty() && !current_address.is_empty() {
            devices.insert(
                name.clone(),
                DeviceEntry {
                    name,
                    transport_type: current_transport,
                    address: current_address,
                },
            );
        }
    }

    Ok(devices)
}

fn serialize_devices_toml(devices: &HashMap<String, DeviceEntry>) -> String {
    let mut output =
        String::from("# DOMES device registry\n# Managed by: domes-cli devices add/remove\n\n");

    let mut names: Vec<&String> = devices.keys().collect();
    names.sort();

    for name in names {
        let entry = &devices[name];
        output.push_str(&format!("[devices.{}]\n", name));
        output.push_str(&format!("transport = \"{}\"\n", entry.transport_type));
        output.push_str(&format!("address = \"{}\"\n\n", entry.address));
    }

    output
}
