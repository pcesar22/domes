//! Multi-device support for DOMES CLI
//!
//! Provides device targeting, registry, and multi-transport management.

use crate::transport::{BleTarget, BleTransport, SerialTransport, TcpTransport, Transport};
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, HashMap};
use std::fs::{self, File, OpenOptions};
use std::io::Write;
use std::net::SocketAddr;
use std::path::{Path, PathBuf};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

/// A named device connection
pub struct DeviceConnection {
    pub name: String,
    pub transport: Box<dyn Transport>,
}

/// A device that could not be connected while resolving a multi-device command.
#[derive(Debug)]
pub struct DeviceConnectionFailure {
    pub name: String,
    pub error: String,
}

/// Successful and failed device connections from one CLI selection.
pub struct DeviceResolution {
    pub connections: Vec<DeviceConnection>,
    pub failures: Vec<DeviceConnectionFailure>,
}

impl DeviceResolution {
    pub fn is_multi(&self) -> bool {
        self.connections.len() + self.failures.len() > 1
    }
}

/// Device registry entry from config file
#[derive(Debug, Clone, PartialEq, Eq, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
pub struct DeviceEntry {
    #[serde(rename = "transport")]
    pub transport_type: String,
    pub address: String,
}

#[derive(Debug, Default, Deserialize, Serialize)]
#[serde(deny_unknown_fields)]
struct DeviceRegistryFile {
    #[serde(default)]
    devices: BTreeMap<String, DeviceEntry>,
}

/// Parse devices.toml config file
///
/// Format:
/// ```toml
/// [devices.pod1]
/// transport = "serial"
/// address = "/dev/ttyUSB0"
///
/// [devices.pod2]
/// transport = "serial"
/// address = "/dev/serial/by-id/usb-Silicon_Labs_CP2102N..."
/// ```
pub fn load_device_registry() -> Result<HashMap<String, DeviceEntry>> {
    load_device_registry_from(&get_config_path())
}

fn load_device_registry_from(config_path: &Path) -> Result<HashMap<String, DeviceEntry>> {
    if !config_path.exists() {
        return Ok(HashMap::new());
    }

    let content = fs::read_to_string(config_path)
        .with_context(|| format!("Failed to read {}", config_path.display()))?;
    let registry: DeviceRegistryFile = toml::from_str(&content)
        .with_context(|| format!("Malformed device registry {}", config_path.display()))?;

    for (name, entry) in &registry.devices {
        validate_device_name(name).with_context(|| {
            format!(
                "Invalid device name '{}' in {}",
                name,
                config_path.display()
            )
        })?;
        validate_device_entry(entry)
            .with_context(|| format!("Invalid device '{}' in {}", name, config_path.display()))?;
    }

    Ok(registry.devices.into_iter().collect())
}

/// Save a device entry to the registry
pub fn save_device_entry(name: &str, entry: &DeviceEntry) -> Result<()> {
    save_device_entry_to(&get_config_path(), name, entry)
}

fn save_device_entry_to(config_path: &Path, name: &str, entry: &DeviceEntry) -> Result<()> {
    validate_device_name(name)?;
    validate_device_entry(entry)?;

    let mut devices = load_device_registry_from(config_path)?;
    let endpoint = device_endpoint_identity(entry);
    if let Some((existing_name, _)) = devices.iter().find(|(existing_name, existing_entry)| {
        existing_name.as_str() != name && device_endpoint_identity(existing_entry) == endpoint
    }) {
        anyhow::bail!(
            "Device endpoint is already registered as '{}'; remove or update that entry first",
            existing_name
        );
    }
    devices.insert(name.to_string(), entry.clone());
    write_device_registry(config_path, &devices)
}

/// Remove a device from the registry
pub fn remove_device_entry(name: &str) -> Result<bool> {
    remove_device_entry_from(&get_config_path(), name)
}

fn remove_device_entry_from(config_path: &Path, name: &str) -> Result<bool> {
    validate_device_name(name)?;
    if !config_path.exists() {
        return Ok(false);
    }

    let mut devices = load_device_registry_from(config_path)?;
    let removed = devices.remove(name).is_some();

    if removed {
        write_device_registry(config_path, &devices)?;
    }
    Ok(removed)
}

/// Connect to a device by registry entry
pub fn connect_device(entry: &DeviceEntry) -> Result<Box<dyn Transport>> {
    validate_device_entry(entry).context("Invalid device registry entry")?;

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
) -> Result<DeviceResolution> {
    let mut resolution = DeviceResolution {
        connections: Vec::new(),
        failures: Vec::new(),
    };
    let mut selected_devices = HashMap::new();

    // If --all, load entire registry
    if all {
        let registry = load_device_registry()?;
        if registry.is_empty() {
            anyhow::bail!("No devices in registry. Use 'devices add' to register devices.");
        }
        let mut entries: Vec<_> = registry.iter().collect();
        entries.sort_by(|(left, _), (right, _)| left.cmp(right));
        for (name, entry) in entries {
            if !reserve_device_endpoint(
                &mut selected_devices,
                name,
                &entry.transport_type,
                &entry.address,
            ) {
                continue;
            }
            eprintln!(
                "Connecting to {} ({} @ {})...",
                name, entry.transport_type, entry.address
            );
            record_connection(
                &mut resolution,
                name.clone(),
                connect_device(entry).with_context(|| format!("Failed to connect to {}", name)),
            );
        }
        return Ok(resolution);
    }

    // If --target, look up in registry
    if !targets.is_empty() {
        let registry = load_device_registry()?;
        for target_name in targets {
            let Some(entry) = registry.get(target_name) else {
                resolution.failures.push(DeviceConnectionFailure {
                    name: target_name.clone(),
                    error: format!("Device '{}' not found in registry", target_name),
                });
                continue;
            };
            if !reserve_device_endpoint(
                &mut selected_devices,
                target_name,
                &entry.transport_type,
                &entry.address,
            ) {
                continue;
            }
            eprintln!(
                "Connecting to {} ({} @ {})...",
                target_name, entry.transport_type, entry.address
            );
            record_connection(
                &mut resolution,
                target_name.clone(),
                connect_device(entry)
                    .with_context(|| format!("Failed to connect to {}", target_name)),
            );
        }
    }

    // Direct connections via --port
    for (i, port) in ports.iter().enumerate() {
        let name = format!("serial-{}", i);
        if !reserve_device_endpoint(&mut selected_devices, &name, "serial", port) {
            continue;
        }
        let connection = SerialTransport::open(port)
            .map(|transport| Box::new(transport) as Box<dyn Transport>)
            .with_context(|| format!("Failed to connect to serial port {}", port));
        record_connection(&mut resolution, name, connection);
    }

    // Direct connections via --wifi
    for (i, addr) in wifis.iter().enumerate() {
        let name = format!("wifi-{}", i);
        if !reserve_device_endpoint(&mut selected_devices, &name, "wifi", addr) {
            continue;
        }
        eprintln!("Connecting to {} via WiFi...", addr);
        let connection = TcpTransport::connect(addr)
            .map(|transport| Box::new(transport) as Box<dyn Transport>)
            .with_context(|| format!("Failed to connect to WiFi device {}", addr));
        record_connection(&mut resolution, name, connection);
    }

    // Direct connections via --ble
    for (i, ble_target) in bles.iter().enumerate() {
        let name = format!("ble-{}", i);
        if !reserve_device_endpoint(&mut selected_devices, &name, "ble", ble_target) {
            continue;
        }
        eprintln!("Scanning for BLE device '{}'...", ble_target);
        let target = BleTarget::parse(ble_target);
        let connection = BleTransport::connect(target, Duration::from_secs(10), true)
            .map(|transport| Box::new(transport) as Box<dyn Transport>)
            .with_context(|| format!("Failed to connect to BLE device {}", ble_target));
        record_connection(&mut resolution, name, connection);
    }

    Ok(resolution)
}

fn record_connection(
    resolution: &mut DeviceResolution,
    name: String,
    connection: Result<Box<dyn Transport>>,
) {
    match connection {
        Ok(transport) => resolution
            .connections
            .push(DeviceConnection { name, transport }),
        Err(error) => resolution.failures.push(DeviceConnectionFailure {
            name,
            error: format!("{error:#}"),
        }),
    }
}

fn reserve_device_endpoint(
    selected: &mut HashMap<String, (String, String)>,
    label: &str,
    transport_type: &str,
    address: &str,
) -> bool {
    let identity = endpoint_identity(transport_type, address);
    if let Some((existing_label, existing_address)) = selected.get(&identity) {
        eprintln!(
            "Warning: target '{}' ({} @ {}) resolves to the same endpoint as '{}' ({}); ignoring duplicate",
            label,
            transport_type,
            address,
            existing_label,
            existing_address
        );
        return false;
    }

    selected.insert(identity, (label.to_string(), address.to_string()));
    true
}

fn serial_device_identity(address: &str) -> String {
    if let Ok(canonical) = fs::canonicalize(address) {
        return canonical.to_string_lossy().into_owned();
    }

    #[cfg(windows)]
    {
        address.to_ascii_uppercase()
    }

    #[cfg(not(windows))]
    {
        address.to_string()
    }
}

fn device_endpoint_identity(entry: &DeviceEntry) -> String {
    endpoint_identity(&entry.transport_type, &entry.address)
}

fn endpoint_identity(transport_type: &str, address: &str) -> String {
    match transport_type {
        "serial" => format!("serial:{}", serial_device_identity(address)),
        "wifi" | "tcp" => format!("tcp:{}", address.to_ascii_lowercase()),
        "ble" if is_ble_mac(address) => {
            format!("ble-mac:{}", address.to_ascii_lowercase())
        }
        "ble" => format!("ble-name:{}", address),
        _ => format!("{}:{}", transport_type, address),
    }
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

fn validate_device_name(name: &str) -> Result<()> {
    const MAX_DEVICE_NAME_LEN: usize = 64;

    if name.is_empty() || name.len() > MAX_DEVICE_NAME_LEN {
        anyhow::bail!(
            "Device name must contain 1-{} ASCII characters",
            MAX_DEVICE_NAME_LEN
        );
    }

    let mut characters = name.chars();
    if !characters
        .next()
        .is_some_and(|character| character.is_ascii_alphanumeric())
        || !characters
            .all(|character| character.is_ascii_alphanumeric() || matches!(character, '-' | '_'))
    {
        anyhow::bail!(
            "Invalid device name '{}': use ASCII letters, digits, '-' or '_', starting with a letter or digit",
            name
        );
    }

    Ok(())
}

fn validate_device_entry(entry: &DeviceEntry) -> Result<()> {
    const MAX_ADDRESS_LEN: usize = 512;

    if entry.address.is_empty() || entry.address.len() > MAX_ADDRESS_LEN {
        anyhow::bail!(
            "Device address must contain 1-{} characters",
            MAX_ADDRESS_LEN
        );
    }
    if entry.address.trim() != entry.address || entry.address.chars().any(char::is_control) {
        anyhow::bail!(
            "Device address must not contain surrounding whitespace or control characters"
        );
    }

    match entry.transport_type.as_str() {
        "serial" => validate_serial_address(&entry.address),
        "wifi" | "tcp" => validate_tcp_address(&entry.address),
        "ble" => validate_ble_address(&entry.address),
        other => anyhow::bail!(
            "Unsupported transport '{}': expected serial, wifi, tcp, or ble",
            other
        ),
    }
}

fn validate_serial_address(address: &str) -> Result<()> {
    let tty_usb = address
        .strip_prefix("/dev/ttyUSB")
        .is_some_and(|suffix| !suffix.is_empty() && suffix.chars().all(|c| c.is_ascii_digit()));
    let by_id = address
        .strip_prefix("/dev/serial/by-id/")
        .is_some_and(|identifier| {
            !identifier.is_empty() && !matches!(identifier, "." | "..") && !identifier.contains('/')
        });
    let windows_com = address
        .strip_prefix("COM")
        .is_some_and(|suffix| !suffix.is_empty() && suffix.chars().all(|c| c.is_ascii_digit()));

    if !tty_usb && !by_id && !windows_com {
        anyhow::bail!(
            "Invalid serial address '{}': use /dev/ttyUSBN, /dev/serial/by-id/..., or COMN",
            address
        );
    }

    Ok(())
}

fn validate_tcp_address(address: &str) -> Result<()> {
    if let Ok(socket) = address.parse::<SocketAddr>() {
        if socket.port() == 0 {
            anyhow::bail!("TCP port must be between 1 and 65535");
        }
        return Ok(());
    }

    let (host, port) = address
        .rsplit_once(':')
        .ok_or_else(|| anyhow::anyhow!("Invalid TCP address '{}': expected HOST:PORT", address))?;
    let valid_host = !host.is_empty()
        && !host.contains(':')
        && host.chars().all(|character| {
            character.is_ascii_alphanumeric() || matches!(character, '.' | '-' | '_')
        });
    if !valid_host {
        anyhow::bail!("Invalid TCP host '{}'", host);
    }

    let port = port
        .parse::<u16>()
        .with_context(|| format!("Invalid TCP port '{}'", port))?;
    if port == 0 {
        anyhow::bail!("TCP port must be between 1 and 65535");
    }

    Ok(())
}

fn validate_ble_address(address: &str) -> Result<()> {
    if !address.contains(':') {
        return Ok(());
    }

    if !is_ble_mac(address) {
        anyhow::bail!(
            "Invalid BLE address '{}': expected an advertised name or six hexadecimal octets",
            address
        );
    }

    Ok(())
}

fn is_ble_mac(address: &str) -> bool {
    let octets: Vec<&str> = address.split(':').collect();
    octets.len() == 6
        && octets.iter().all(|octet| {
            octet.len() == 2 && octet.chars().all(|character| character.is_ascii_hexdigit())
        })
}

fn write_device_registry(config_path: &Path, devices: &HashMap<String, DeviceEntry>) -> Result<()> {
    let mut ordered_devices = BTreeMap::new();
    for (name, entry) in devices {
        validate_device_name(name)?;
        validate_device_entry(entry).with_context(|| format!("Invalid device '{}'", name))?;
        ordered_devices.insert(name.clone(), entry.clone());
    }

    let registry = DeviceRegistryFile {
        devices: ordered_devices,
    };
    let serialized = toml::to_string_pretty(&registry).context("Failed to serialize registry")?;
    let content = format!(
        "# DOMES device registry\n# Managed by: domes-cli devices add/remove\n\n{}",
        serialized
    );

    let parent = config_path
        .parent()
        .context("Device registry path has no parent directory")?;
    fs::create_dir_all(parent).with_context(|| format!("Failed to create {}", parent.display()))?;

    let (temporary_path, mut temporary_file) = create_atomic_temp(config_path)?;
    let write_result = (|| -> Result<()> {
        temporary_file
            .write_all(content.as_bytes())
            .with_context(|| format!("Failed to write {}", temporary_path.display()))?;
        temporary_file
            .sync_all()
            .with_context(|| format!("Failed to sync {}", temporary_path.display()))?;
        drop(temporary_file);
        fs::rename(&temporary_path, config_path).with_context(|| {
            format!(
                "Failed to atomically replace {} with {}",
                config_path.display(),
                temporary_path.display()
            )
        })?;
        Ok(())
    })();

    if write_result.is_err() {
        let _ = fs::remove_file(&temporary_path);
    }
    write_result
}

fn create_atomic_temp(config_path: &Path) -> Result<(PathBuf, File)> {
    let parent = config_path
        .parent()
        .context("Device registry path has no parent directory")?;
    let file_name = config_path
        .file_name()
        .and_then(|name| name.to_str())
        .unwrap_or("devices.toml");
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();

    for attempt in 0..100 {
        let temporary_path = parent.join(format!(
            ".{}.tmp-{}-{}-{}",
            file_name,
            std::process::id(),
            nonce,
            attempt
        ));
        match OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&temporary_path)
        {
            Ok(file) => return Ok((temporary_path, file)),
            Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => continue,
            Err(error) => {
                return Err(error).with_context(|| {
                    format!(
                        "Failed to create temporary registry in {}",
                        parent.display()
                    )
                });
            }
        }
    }

    anyhow::bail!(
        "Failed to create a unique temporary registry in {}",
        parent.display()
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::transport::Frame;

    struct TestTransport;

    impl Transport for TestTransport {
        fn send_frame(&mut self, _msg_type: u8, _payload: &[u8]) -> Result<()> {
            Ok(())
        }

        fn receive_frame(&mut self, _timeout_ms: u64) -> Result<Frame> {
            anyhow::bail!("not used")
        }

        fn send_command(&mut self, _msg_type: u8, _payload: &[u8]) -> Result<Frame> {
            anyhow::bail!("not used")
        }
    }

    #[test]
    fn connection_failures_do_not_discard_healthy_devices() {
        let mut resolution = DeviceResolution {
            connections: Vec::new(),
            failures: Vec::new(),
        };

        record_connection(
            &mut resolution,
            "offline".to_string(),
            Err(anyhow::anyhow!("connection refused")),
        );
        record_connection(
            &mut resolution,
            "healthy".to_string(),
            Ok(Box::new(TestTransport)),
        );

        assert!(resolution.is_multi());
        assert_eq!(resolution.connections.len(), 1);
        assert_eq!(resolution.connections[0].name, "healthy");
        assert_eq!(resolution.failures.len(), 1);
        assert_eq!(resolution.failures[0].name, "offline");
        assert!(resolution.failures[0].error.contains("connection refused"));
    }

    #[test]
    fn duplicate_serial_registry_targets_are_reserved_once() {
        let mut selected = HashMap::new();

        assert!(reserve_device_endpoint(
            &mut selected,
            "pod1",
            "serial",
            "/dev/ttyUSB0"
        ));
        assert!(!reserve_device_endpoint(
            &mut selected,
            "pod1-alias",
            "serial",
            "/dev/ttyUSB0"
        ));
        assert_eq!(selected.len(), 1);
    }

    #[test]
    fn ble_and_tcp_aliases_are_reserved_once() {
        let mut selected = HashMap::new();

        assert!(reserve_device_endpoint(
            &mut selected,
            "ble-registry",
            "ble",
            "94:A9:90:0A:EB:C2"
        ));
        assert!(!reserve_device_endpoint(
            &mut selected,
            "ble-direct",
            "ble",
            "94:a9:90:0a:eb:c2"
        ));
        assert!(reserve_device_endpoint(
            &mut selected,
            "wifi-registry",
            "wifi",
            "POD.LOCAL:5000"
        ));
        assert!(!reserve_device_endpoint(
            &mut selected,
            "tcp-direct",
            "tcp",
            "pod.local:5000"
        ));
        assert!(reserve_device_endpoint(
            &mut selected,
            "ble-name-upper",
            "ble",
            "Lab:Pod"
        ));
        assert!(reserve_device_endpoint(
            &mut selected,
            "ble-name-lower",
            "ble",
            "lab:pod"
        ));
        assert_eq!(selected.len(), 4);
    }

    #[cfg(unix)]
    #[test]
    fn serial_symlink_and_device_path_share_one_identity() {
        use std::os::unix::fs::symlink;

        let directory = temporary_registry_path("serial-alias")
            .parent()
            .unwrap()
            .to_path_buf();
        fs::create_dir_all(&directory).unwrap();
        let device = directory.join("ttyUSB0");
        let by_id = directory.join("usb-Silicon_Labs_CP2102N-test");
        fs::write(&device, []).unwrap();
        symlink(&device, &by_id).unwrap();

        let mut selected = HashMap::new();
        assert!(reserve_device_endpoint(
            &mut selected,
            "direct",
            "serial",
            device.to_str().unwrap()
        ));
        assert!(!reserve_device_endpoint(
            &mut selected,
            "registry-pod",
            "serial",
            by_id.to_str().unwrap()
        ));
        assert_eq!(selected.len(), 1);

        fs::remove_dir_all(directory).unwrap();
    }

    #[test]
    fn registry_round_trip_uses_toml_escaping_and_leaves_no_temp_file() {
        let path = temporary_registry_path("round-trip");
        let serial = DeviceEntry {
            transport_type: "serial".to_string(),
            address: "/dev/serial/by-id/usb-Silicon_Labs_CP2102N-test".to_string(),
        };
        let ble = DeviceEntry {
            transport_type: "ble".to_string(),
            address: "DOMES \"Lab\" \\ Pod".to_string(),
        };

        save_device_entry_to(&path, "pod_1", &serial).unwrap();
        save_device_entry_to(&path, "lab-pod", &ble).unwrap();

        let loaded = load_device_registry_from(&path).unwrap();
        assert_eq!(loaded.get("pod_1"), Some(&serial));
        assert_eq!(loaded.get("lab-pod"), Some(&ble));

        let content = fs::read_to_string(&path).unwrap();
        let parsed: DeviceRegistryFile = toml::from_str(&content).unwrap();
        assert_eq!(parsed.devices.get("lab-pod"), Some(&ble));
        assert!(fs::read_dir(path.parent().unwrap())
            .unwrap()
            .filter_map(|entry| entry.ok())
            .all(|entry| !entry.file_name().to_string_lossy().contains(".tmp-")));

        remove_test_registry(&path);
    }

    #[test]
    fn registry_rejects_aliases_for_the_same_endpoint() {
        let path = temporary_registry_path("duplicate-endpoint");
        let serial = DeviceEntry {
            transport_type: "serial".to_string(),
            address: "/dev/ttyUSB0".to_string(),
        };

        save_device_entry_to(&path, "pod1", &serial).unwrap();
        let error = save_device_entry_to(&path, "pod1-alias", &serial)
            .unwrap_err()
            .to_string();
        assert!(error.contains("already registered as 'pod1'"));

        let replacement = DeviceEntry {
            transport_type: "serial".to_string(),
            address: "/dev/ttyUSB1".to_string(),
        };
        save_device_entry_to(&path, "pod1", &replacement).unwrap();
        assert_eq!(
            load_device_registry_from(&path).unwrap().get("pod1"),
            Some(&replacement)
        );
        remove_test_registry(&path);
    }

    #[test]
    fn malformed_registry_blocks_update_without_overwriting_original() {
        let path = temporary_registry_path("malformed");
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        let malformed = "[devices.broken\ntransport = \"serial\"\n";
        fs::write(&path, malformed).unwrap();
        let entry = DeviceEntry {
            transport_type: "serial".to_string(),
            address: "/dev/ttyUSB0".to_string(),
        };

        let error = save_device_entry_to(&path, "pod1", &entry)
            .unwrap_err()
            .to_string();

        assert!(error.contains("Malformed device registry"));
        assert_eq!(fs::read_to_string(&path).unwrap(), malformed);
        remove_test_registry(&path);
    }

    #[test]
    fn registry_rejects_unknown_fields_and_invalid_entries() {
        let path = temporary_registry_path("unknown-field");
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        fs::write(
            &path,
            "[devices.pod1]\ntransport = \"serial\"\naddress = \"/dev/ttyUSB0\"\nunexpected = true\n",
        )
        .unwrap();
        assert!(load_device_registry_from(&path).is_err());
        remove_test_registry(&path);

        for invalid_name in ["", "-pod", "pod one", "pod.one", "pod/one"] {
            assert!(
                validate_device_name(invalid_name).is_err(),
                "{invalid_name}"
            );
        }
        for valid_name in ["pod1", "pod-1", "pod_1", "1pod"] {
            validate_device_name(valid_name).unwrap();
        }

        for entry in [
            DeviceEntry {
                transport_type: "bogus".to_string(),
                address: "value".to_string(),
            },
            DeviceEntry {
                transport_type: "serial".to_string(),
                address: "/dev/ttyACM0".to_string(),
            },
            DeviceEntry {
                transport_type: "wifi".to_string(),
                address: "domes.local".to_string(),
            },
            DeviceEntry {
                transport_type: "ble".to_string(),
                address: "AA:BB:CC:DD:EE".to_string(),
            },
        ] {
            assert!(validate_device_entry(&entry).is_err(), "{entry:?}");
        }
    }

    #[test]
    fn registry_accepts_supported_transport_addresses_and_removes_atomically() {
        let path = temporary_registry_path("valid-addresses");
        let entries = [
            (
                "usb",
                DeviceEntry {
                    transport_type: "serial".to_string(),
                    address: "/dev/ttyUSB12".to_string(),
                },
            ),
            (
                "stable",
                DeviceEntry {
                    transport_type: "serial".to_string(),
                    address: "/dev/serial/by-id/usb-Silicon_Labs_CP2102N-test".to_string(),
                },
            ),
            (
                "wifi",
                DeviceEntry {
                    transport_type: "wifi".to_string(),
                    address: "domes-pod.local:5000".to_string(),
                },
            ),
            (
                "ipv6",
                DeviceEntry {
                    transport_type: "tcp".to_string(),
                    address: "[::1]:5000".to_string(),
                },
            ),
            (
                "ble",
                DeviceEntry {
                    transport_type: "ble".to_string(),
                    address: "AA:BB:CC:DD:EE:FF".to_string(),
                },
            ),
        ];

        for (name, entry) in &entries {
            save_device_entry_to(&path, name, entry).unwrap();
        }
        assert_eq!(
            load_device_registry_from(&path).unwrap().len(),
            entries.len()
        );
        assert!(remove_device_entry_from(&path, "wifi").unwrap());
        assert!(!load_device_registry_from(&path)
            .unwrap()
            .contains_key("wifi"));
        assert!(!remove_device_entry_from(&path, "missing").unwrap());

        remove_test_registry(&path);
    }

    fn temporary_registry_path(label: &str) -> PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        std::env::temp_dir()
            .join(format!(
                "domes-cli-registry-{label}-{}-{nonce}",
                std::process::id()
            ))
            .join("devices.toml")
    }

    fn remove_test_registry(path: &Path) {
        if let Some(parent) = path.parent() {
            fs::remove_dir_all(parent).unwrap();
        }
    }
}
