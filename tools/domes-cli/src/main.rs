//! DOMES CLI - Runtime configuration tool for DOMES firmware
//!
//! Usage (Serial):
//!   domes-cli --port /dev/ttyACM0 feature list
//!   domes-cli --port /dev/ttyACM0 feature enable led-effects
//!   domes-cli --port /dev/ttyACM0 feature disable ble
//!   domes-cli --port /dev/ttyACM0 wifi enable
//!   domes-cli --port /dev/ttyACM0 wifi disable
//!   domes-cli --port /dev/ttyACM0 wifi status
//!   domes-cli --port /dev/ttyACM0 led get
//!   domes-cli --port /dev/ttyACM0 led off
//!   domes-cli --port /dev/ttyACM0 led solid --color ff0000
//!   domes-cli --port /dev/ttyACM0 led breathing --color 00ff00 --period 2000
//!   domes-cli --port /dev/ttyACM0 led cycle --period 3000
//!   domes-cli --port /dev/ttyACM0 ota flash firmware.bin --version v1.2.3
//!   domes-cli --port /dev/ttyACM0 trace start
//!   domes-cli --port /dev/ttyACM0 trace stop
//!   domes-cli --port /dev/ttyACM0 trace status
//!   domes-cli --port /dev/ttyACM0 trace dump -o trace.json
//!   domes-cli --port /dev/ttyACM0 system mode
//!   domes-cli --port /dev/ttyACM0 system set-mode triage
//!   domes-cli --port /dev/ttyACM0 system info
//!
//! Usage (WiFi):
//!   domes-cli --wifi 192.168.1.100:5000 feature list
//!   domes-cli --wifi 192.168.1.100:5000 feature enable led-effects
//!   domes-cli --wifi 192.168.1.100:5000 wifi status
//!   domes-cli --wifi 192.168.1.100:5000 led cycle --period 2000
//!   domes-cli --wifi 192.168.1.100:5000 ota flash firmware.bin
//!
//! Usage (BLE):
//!   domes-cli --scan-ble                           # Scan for nearby DOMES devices
//!   domes-cli --ble "DOMES-Pod" feature list       # Connect by name
//!   domes-cli --ble "AA:BB:CC:DD:EE:FF" led solid  # Connect by MAC address
//!
//! Multi-device usage:
//!   domes-cli --port /dev/ttyACM0 --port /dev/ttyACM1 feature list
//!   domes-cli --target pod1 --target pod2 led solid --color ff0000
//!   domes-cli --all feature list
//!
//! Device registry:
//!   domes-cli devices scan
//!   domes-cli devices add pod1 serial /dev/ttyACM0
//!   domes-cli devices add pod2 serial /dev/ttyACM1
//!   domes-cli devices list
//!   domes-cli devices remove pod1

mod commands;
mod device;
mod proto;
mod protocol;
mod transport;

use clap::{Parser, Subcommand};
use proto::config::{Feature, SystemMode};
use std::path::PathBuf;
use std::time::Duration;
use transport::{BleTransport, SerialTransport};

#[derive(Parser)]
#[command(name = "domes-cli")]
#[command(version, about = "DOMES firmware runtime configuration CLI")]
struct Cli {
    /// Serial port(s) to connect to (e.g., /dev/ttyACM0). Can be specified multiple times.
    #[arg(short, long)]
    port: Vec<String>,

    /// WiFi address(es) to connect to (e.g., 192.168.1.100:5000). Can be specified multiple times.
    #[arg(short, long)]
    wifi: Vec<String>,

    /// BLE device name(s) or address(es). Can be specified multiple times.
    #[arg(short, long)]
    ble: Vec<String>,

    /// Target device(s) from registry (~/.domes/devices.toml). Can be specified multiple times.
    #[arg(short, long)]
    target: Vec<String>,

    /// Target all registered devices
    #[arg(long)]
    all: bool,

    /// Scan for nearby BLE devices
    #[arg(long)]
    scan_ble: bool,

    /// Auto-connect to all BLE devices with DOMES-Pod prefix
    #[arg(long)]
    connect_all_ble: bool,

    /// List available serial ports
    #[arg(long)]
    list_ports: bool,

    #[command(subcommand)]
    command: Option<Commands>,
}

#[derive(Subcommand)]
enum Commands {
    /// Manage runtime features
    Feature {
        #[command(subcommand)]
        action: FeatureAction,
    },

    /// Control WiFi subsystem
    Wifi {
        #[command(subcommand)]
        action: WifiAction,
    },

    /// Control LED patterns
    Led {
        #[command(subcommand)]
        action: LedAction,
    },

    /// Over-the-air firmware updates
    Ota {
        #[command(subcommand)]
        action: OtaAction,
    },

    /// Performance tracing (Perfetto compatible)
    Trace {
        #[command(subcommand)]
        action: TraceAction,
    },

    /// IMU (accelerometer) commands
    Imu {
        #[command(subcommand)]
        action: ImuAction,
    },

    /// System mode and info commands
    System {
        #[command(subcommand)]
        action: SystemAction,
    },

    /// Manage device registry
    Devices {
        #[command(subcommand)]
        action: DevicesAction,
    },
}

#[derive(Subcommand)]
enum FeatureAction {
    /// List all features and their current state
    List,

    /// Enable a feature
    Enable {
        /// Feature name (e.g., led-effects, ble, wifi, esp-now, touch, haptic, audio)
        feature: String,
    },

    /// Disable a feature
    Disable {
        /// Feature name (e.g., led-effects, ble, wifi, esp-now, touch, haptic, audio)
        feature: String,
    },
}

#[derive(Subcommand)]
enum WifiAction {
    /// Enable WiFi subsystem
    Enable,

    /// Disable WiFi subsystem
    Disable,

    /// Show WiFi subsystem status
    Status,
}

#[derive(Subcommand)]
enum OtaAction {
    /// Flash firmware to device
    Flash {
        /// Path to firmware binary (.bin file)
        firmware: PathBuf,

        /// Version string (e.g., v1.2.3)
        #[arg(short, long)]
        version: Option<String>,
    },
}

#[derive(Subcommand)]
enum TraceAction {
    /// Start trace recording
    Start,

    /// Stop trace recording
    Stop,

    /// Clear trace buffer
    Clear,

    /// Show trace system status
    Status,

    /// Dump traces to JSON file (Perfetto compatible)
    Dump {
        /// Output file path (default: trace.json)
        #[arg(short, long, default_value = "trace.json")]
        output: PathBuf,
    },
}

#[derive(Subcommand)]
enum ImuAction {
    /// Set triage mode (flash LEDs on tap)
    Triage {
        /// Enable triage mode
        #[arg(long)]
        enable: bool,

        /// Disable triage mode
        #[arg(long)]
        disable: bool,
    },
}

#[derive(Subcommand)]
enum SystemAction {
    /// Get current system mode
    Mode,

    /// Set system mode (e.g., idle, triage, connected, game, error)
    SetMode {
        /// Mode name (idle, triage, connected, game, error)
        mode: String,
    },

    /// Get system information (version, uptime, heap, etc.)
    Info,

    /// Set pod ID (1-255, persisted to NVS, reboot for BLE name change)
    SetPodId {
        /// Pod ID (1-255)
        id: u32,
    },
}

#[derive(Subcommand)]
enum LedAction {
    /// Get current LED pattern
    Get,

    /// Turn LEDs off
    Off,

    /// Set solid color (e.g., led solid --color ff0000)
    Solid {
        /// Hex color (e.g., ff0000 for red)
        #[arg(short, long, default_value = "ffffff")]
        color: String,

        /// Brightness (0-255)
        #[arg(short, long, default_value = "128")]
        brightness: u8,
    },

    /// Set breathing pattern (pulsing brightness)
    Breathing {
        /// Hex color (e.g., 00ff00 for green)
        #[arg(short, long, default_value = "00ff00")]
        color: String,

        /// Breathing period in ms (time for one full cycle)
        #[arg(short, long, default_value = "2000")]
        period: u32,

        /// Brightness (0-255)
        #[arg(short, long, default_value = "128")]
        brightness: u8,
    },

    /// Set color cycle pattern (automatic color transitions)
    Cycle {
        /// Cycle period in ms (time between color changes)
        #[arg(short, long, default_value = "2000")]
        period: u32,

        /// Brightness (0-255)
        #[arg(short, long, default_value = "128")]
        brightness: u8,
    },
}

#[derive(Subcommand)]
enum DevicesAction {
    /// List registered devices
    List,

    /// Add a device to the registry
    Add {
        /// Device name (e.g., pod1, pod2)
        name: String,

        /// Transport type (serial, wifi, ble)
        transport: String,

        /// Address (e.g., /dev/ttyACM0, 192.168.1.100:5000, "DOMES-Pod-01")
        address: String,
    },

    /// Remove a device from the registry
    Remove {
        /// Device name to remove
        name: String,
    },

    /// Scan for all connected DOMES devices
    Scan,
}

fn main() -> anyhow::Result<()> {
    let mut cli = Cli::parse();

    // Handle --list-ports
    if cli.list_ports {
        let ports = SerialTransport::list_ports()?;
        if ports.is_empty() {
            println!("No serial ports found");
        } else {
            println!("Available serial ports:");
            for port in ports {
                println!("  {}", port);
            }
        }
        return Ok(());
    }

    // Handle --connect-all-ble: scan and add DOMES devices to BLE targets
    if cli.connect_all_ble {
        println!("Scanning for DOMES BLE devices (10 seconds)...");
        let ble_devices = BleTransport::scan_devices(Duration::from_secs(10))?;
        let existing: std::collections::HashSet<String> = cli.ble.iter().cloned().collect();
        for (name, addr) in &ble_devices {
            if name.starts_with("DOMES-Pod") && !existing.contains(addr) {
                println!("  Found: {} ({})", name, addr);
                cli.ble.push(addr.clone());
            }
        }
        let has_other_transports =
            !cli.port.is_empty() || !cli.wifi.is_empty() || !cli.target.is_empty() || cli.all;
        if cli.ble.is_empty() && !has_other_transports {
            eprintln!("No DOMES BLE devices found");
            std::process::exit(1);
        } else if cli.ble.is_empty() {
            eprintln!("Warning: no DOMES BLE devices found via scan, using other transports");
        }
        println!();
    }

    // Handle --scan-ble
    if cli.scan_ble {
        println!("Scanning for DOMES devices via BLE (10 seconds)...");
        let devices = BleTransport::scan_devices(Duration::from_secs(10))?;
        if devices.is_empty() {
            println!("No DOMES devices found");
        } else {
            println!("Found DOMES devices:");
            println!("{:<20} {}", "NAME", "ADDRESS");
            println!("{:-<20} {:-<17}", "", "");
            for (name, addr) in devices {
                let display_name = if name.is_empty() { "(unknown)" } else { &name };
                println!("{:<20} {}", display_name, addr);
            }
        }
        return Ok(());
    }

    // Handle devices subcommand (no transport needed)
    if let Some(Commands::Devices { action }) = &cli.command {
        match action {
            DevicesAction::List => {
                let registry = device::load_device_registry()?;
                if registry.is_empty() {
                    println!("No devices registered.");
                    println!(
                        "Use 'domes-cli devices add <name> <transport> <address>' to register."
                    );
                } else {
                    println!("{:<12} {:<10} {}", "NAME", "TRANSPORT", "ADDRESS");
                    println!("{:-<12} {:-<10} {:-<30}", "", "", "");
                    let mut names: Vec<&String> = registry.keys().collect();
                    names.sort();
                    for name in names {
                        let entry = &registry[name];
                        println!(
                            "{:<12} {:<10} {}",
                            name, entry.transport_type, entry.address
                        );
                    }
                }
                return Ok(());
            }
            DevicesAction::Add {
                name,
                transport,
                address,
            } => {
                let entry = device::DeviceEntry {
                    name: name.clone(),
                    transport_type: transport.clone(),
                    address: address.clone(),
                };
                device::save_device_entry(name, &entry)?;
                println!("Added device '{}' ({} @ {})", name, transport, address);
                return Ok(());
            }
            DevicesAction::Remove { name } => {
                if device::remove_device_entry(name)? {
                    println!("Removed device '{}'", name);
                } else {
                    println!("Device '{}' not found", name);
                }
                return Ok(());
            }
            DevicesAction::Scan => {
                println!("Scanning for DOMES devices...\n");

                // Scan serial ports (ttyACM* and domes-pod-* symlinks)
                let ports = SerialTransport::list_ports().unwrap_or_default();
                let domes_symlinks: Vec<String> = std::fs::read_dir("/dev")
                    .ok()
                    .map(|entries| {
                        entries
                            .filter_map(|e| e.ok())
                            .filter(|e| {
                                e.file_name()
                                    .to_str()
                                    .map(|n| n.starts_with("domes-pod-"))
                                    .unwrap_or(false)
                            })
                            .map(|e| format!("/dev/{}", e.file_name().to_string_lossy()))
                            .collect()
                    })
                    .unwrap_or_default();

                if !ports.is_empty() || !domes_symlinks.is_empty() {
                    println!("Serial devices:");
                    for port in &ports {
                        // Try to probe the device for identity
                        let pod_info = SerialTransport::open(port)
                            .ok()
                            .and_then(|mut t| commands::system_info(&mut t).ok());
                        if let Some(info) = pod_info {
                            let pod_label = if info.pod_id > 0 {
                                format!("pod-{}", info.pod_id)
                            } else {
                                "unknown-id".to_string()
                            };
                            println!(
                                "  {:<20} {} (fw: {}, mode: {:?})",
                                port, pod_label, info.firmware_version, info.mode
                            );
                        } else {
                            println!("  {:<20} (not a DOMES device or busy)", port);
                        }
                    }
                    for symlink in &domes_symlinks {
                        if !ports.contains(symlink) {
                            println!("  {:<20} (udev symlink)", symlink);
                        }
                    }
                    println!();
                } else {
                    println!("No serial devices found\n");
                }

                // Scan BLE
                println!("Scanning BLE (10 seconds)...");
                let ble_devices =
                    BleTransport::scan_devices(Duration::from_secs(10)).unwrap_or_default();
                if !ble_devices.is_empty() {
                    println!("BLE devices:");
                    for (name, addr) in &ble_devices {
                        let display_name = if name.is_empty() {
                            "(unknown)"
                        } else {
                            name
                        };
                        let is_domes = display_name.starts_with("DOMES-Pod");
                        println!(
                            "  {:<20} {}{}",
                            display_name,
                            addr,
                            if is_domes { " <-- DOMES" } else { "" }
                        );
                    }
                } else {
                    println!("No BLE devices found");
                }

                return Ok(());
            }
        }
    }

    // All other commands require at least one transport
    let Some(command) = cli.command else {
        eprintln!("No command specified. Use --help for usage.");
        std::process::exit(1);
    };

    // Resolve device connections
    let mut devices = device::resolve_devices(
        &cli.port,
        &cli.wifi,
        &cli.ble,
        &cli.target,
        cli.all,
    )?;

    if devices.is_empty() {
        eprintln!("No transport specified. Use --port, --wifi, --ble, --target, or --all");
        eprintln!("Use --list-ports to see serial ports, --scan-ble for BLE devices.");
        eprintln!("Use 'domes-cli devices add <name> <type> <addr>' to register devices.");
        std::process::exit(1);
    }

    let multi = devices.len() > 1;
    let mut failures: Vec<String> = Vec::new();

    // Execute command on each device
    for dev in devices.iter_mut() {
        let prefix = if multi {
            device::device_prefix(&dev.name)
        } else {
            String::new()
        };
        let transport = dev.transport.as_mut();
        let dev_label = if dev.name.is_empty() {
            "device".to_string()
        } else {
            dev.name.clone()
        };

        if multi {
            println!("--- {} ---", dev_label);
        }

        let result: anyhow::Result<()> = (|| {
        match &command {
            Commands::Feature { action } => match action {
                FeatureAction::List => {
                    let features = commands::feature_list(transport)?;
                    println!("{}Features:", prefix);
                    println!("{}{:<16} {}", prefix, "NAME", "STATUS");
                    println!("{}{:-<16} {:-<8}", prefix, "", "");
                    for state in features {
                        let status = if state.enabled { "enabled" } else { "disabled" };
                        println!("{}{:<16} {}", prefix, state.feature.cli_name(), status);
                    }
                }
                FeatureAction::Enable { feature } => {
                    let feature: Feature = feature
                        .parse()
                        .map_err(|_| anyhow::anyhow!("Unknown feature: {}", feature))?;
                    let state = commands::feature_enable(transport, feature)?;
                    println!(
                        "{}Feature '{}' is now {}",
                        prefix,
                        state.feature.cli_name(),
                        if state.enabled { "enabled" } else { "disabled" }
                    );
                }
                FeatureAction::Disable { feature } => {
                    let feature: Feature = feature
                        .parse()
                        .map_err(|_| anyhow::anyhow!("Unknown feature: {}", feature))?;
                    let state = commands::feature_disable(transport, feature)?;
                    println!(
                        "{}Feature '{}' is now {}",
                        prefix,
                        state.feature.cli_name(),
                        if state.enabled { "enabled" } else { "disabled" }
                    );
                }
            },

            Commands::Wifi { action } => match action {
                WifiAction::Enable => {
                    let enabled = commands::wifi_enable(transport)?;
                    println!(
                        "{}WiFi subsystem {}",
                        prefix,
                        if enabled {
                            "enabled"
                        } else {
                            "failed to enable"
                        }
                    );
                }
                WifiAction::Disable => {
                    let disabled = commands::wifi_disable(transport)?;
                    println!(
                        "{}WiFi subsystem {}",
                        prefix,
                        if disabled {
                            "disabled"
                        } else {
                            "failed to disable"
                        }
                    );
                }
                WifiAction::Status => {
                    let enabled = commands::wifi_status(transport)?;
                    println!(
                        "{}WiFi subsystem: {}",
                        prefix,
                        if enabled { "enabled" } else { "disabled" }
                    );
                }
            },

            Commands::Led { action } => match action {
                LedAction::Get => {
                    let pattern = commands::led_get(transport)?;
                    if multi {
                        println!("{}LED pattern:", prefix);
                    }
                    print_led_pattern(&pattern);
                }
                LedAction::Off => {
                    let pattern = commands::led_off(transport)?;
                    println!("{}LEDs turned off", prefix);
                    print_led_pattern(&pattern);
                }
                LedAction::Solid { color, brightness } => {
                    let (r, g, b) = parse_hex_color(color)?;
                    let mut pattern = crate::protocol::CliLedPattern::solid(r, g, b);
                    pattern.brightness = *brightness;
                    let pattern = commands::led_set(transport, &pattern)?;
                    println!("{}LED pattern set to solid", prefix);
                    print_led_pattern(&pattern);
                }
                LedAction::Breathing {
                    color,
                    period,
                    brightness,
                } => {
                    let (r, g, b) = parse_hex_color(color)?;
                    let mut pattern =
                        crate::protocol::CliLedPattern::breathing(r, g, b, *period);
                    pattern.brightness = *brightness;
                    let pattern = commands::led_set(transport, &pattern)?;
                    println!("{}LED pattern set to breathing", prefix);
                    print_led_pattern(&pattern);
                }
                LedAction::Cycle { period, brightness } => {
                    let colors = vec![
                        (255, 0, 0, 0),
                        (255, 127, 0, 0),
                        (255, 255, 0, 0),
                        (0, 255, 0, 0),
                        (0, 0, 255, 0),
                        (75, 0, 130, 0),
                        (148, 0, 211, 0),
                    ];
                    let mut pattern =
                        crate::protocol::CliLedPattern::color_cycle(colors, *period);
                    pattern.brightness = *brightness;
                    let pattern = commands::led_set(transport, &pattern)?;
                    println!("{}LED pattern set to color cycle", prefix);
                    print_led_pattern(&pattern);
                }
            },

            Commands::Ota { action } => match action {
                OtaAction::Flash { firmware, version } => {
                    if multi {
                        println!("{}Flashing OTA...", prefix);
                    }
                    commands::ota_flash(transport, firmware, version.as_deref())?;
                }
            },

            Commands::Trace { action } => match action {
                TraceAction::Start => {
                    commands::trace_start(transport)?;
                    println!("{}Tracing started", prefix);
                }
                TraceAction::Stop => {
                    commands::trace_stop(transport)?;
                    println!("{}Tracing stopped", prefix);
                }
                TraceAction::Clear => {
                    commands::trace_clear(transport)?;
                    println!("{}Trace buffer cleared", prefix);
                }
                TraceAction::Status => {
                    let status = commands::trace_status(transport)?;
                    println!("{}Trace status:", prefix);
                    println!("{}  Initialized: {}", prefix, status.initialized);
                    println!("{}  Enabled:     {}", prefix, status.enabled);
                    println!("{}  Events:      {}", prefix, status.event_count);
                    println!("{}  Dropped:     {}", prefix, status.dropped_count);
                    println!("{}  Buffer size: {} bytes", prefix, status.buffer_size);
                }
                TraceAction::Dump { output } => {
                    let dump_path = if multi {
                        // Per-device output file
                        let stem = output
                            .file_stem()
                            .unwrap_or_default()
                            .to_string_lossy();
                        let ext = output
                            .extension()
                            .unwrap_or_default()
                            .to_string_lossy();
                        output.with_file_name(format!("{}-{}.{}", stem, dev.name, ext))
                    } else {
                        output.clone()
                    };
                    println!("{}Dumping traces to {}...", prefix, dump_path.display());
                    let result = commands::trace_dump(transport, &dump_path)?;
                    println!("{}Dump complete: {} events", prefix, result.event_count);
                    println!("{}Output: {}", prefix, result.output_path.display());
                }
            },

            Commands::Imu { action } => match action {
                ImuAction::Triage { enable, disable } => {
                    let enabled = if *enable && *disable {
                        anyhow::bail!("Cannot specify both --enable and --disable");
                    } else if *enable {
                        true
                    } else if *disable {
                        false
                    } else {
                        anyhow::bail!("Must specify either --enable or --disable");
                    };
                    let result = commands::imu_triage_set(transport, enabled)?;
                    println!(
                        "{}IMU triage mode {}",
                        prefix,
                        if result { "enabled" } else { "disabled" }
                    );
                }
            },

            Commands::System { action } => match action {
                SystemAction::Mode => {
                    let info = commands::system_get_mode(transport)?;
                    println!("{}System mode: {}", prefix, info.mode);
                    println!("{}  Time in mode: {} ms", prefix, info.time_in_mode_ms);
                }
                SystemAction::SetMode { mode } => {
                    let mode: SystemMode = mode.parse().map_err(|_| {
                        anyhow::anyhow!(
                            "Unknown mode: {}. Valid: idle, triage, connected, game, error",
                            mode
                        )
                    })?;
                    let (new_mode, ok) = commands::system_set_mode(transport, mode)?;
                    if ok {
                        println!("{}System mode set to: {}", prefix, new_mode);
                    } else {
                        println!(
                            "{}Mode transition rejected (current mode: {})",
                            prefix, new_mode
                        );
                    }
                }
                SystemAction::Info => {
                    let info = commands::system_info(transport)?;
                    println!("{}System Information:", prefix);
                    println!("{}  Firmware:   {}", prefix, info.firmware_version);
                    println!("{}  Pod ID:     {}", prefix, if info.pod_id == 0 { "not set".to_string() } else { info.pod_id.to_string() });
                    println!("{}  Mode:       {}", prefix, info.mode);
                    println!("{}  Uptime:     {} s", prefix, info.uptime_s);
                    println!("{}  Free heap:  {} bytes", prefix, info.free_heap);
                    println!("{}  Boot count: {}", prefix, info.boot_count);
                    println!("{}  Features:   0x{:08X}", prefix, info.feature_mask);
                }
                SystemAction::SetPodId { id } => {
                    let new_id = commands::system_set_pod_id(transport, *id)?;
                    println!("{}Pod ID set to {} (reboot device for BLE name change)", prefix, new_id);
                }
            },

            Commands::Devices { .. } => unreachable!(), // Handled above
        }
        Ok(())
        })();

        if let Err(e) = result {
            if multi {
                eprintln!("{}Error: {:#}", prefix, e);
                failures.push(dev_label);
            } else {
                return Err(e);
            }
        }

        if multi {
            println!(); // Blank line between devices
        }
    }

    if !failures.is_empty() {
        eprintln!(
            "Failed on {} device(s): {}",
            failures.len(),
            failures.join(", ")
        );
        std::process::exit(1);
    }

    Ok(())
}

/// Parse hex color string (e.g., "ff0000" or "FF0000") to RGB
fn parse_hex_color(color: &str) -> anyhow::Result<(u8, u8, u8)> {
    let color = color.trim_start_matches('#');
    if color.len() != 6 {
        anyhow::bail!("Color must be 6 hex characters (e.g., ff0000)");
    }

    let r = u8::from_str_radix(&color[0..2], 16)
        .map_err(|_| anyhow::anyhow!("Invalid red component"))?;
    let g = u8::from_str_radix(&color[2..4], 16)
        .map_err(|_| anyhow::anyhow!("Invalid green component"))?;
    let b = u8::from_str_radix(&color[4..6], 16)
        .map_err(|_| anyhow::anyhow!("Invalid blue component"))?;

    Ok((r, g, b))
}

/// Print LED pattern in a human-readable format
fn print_led_pattern(pattern: &crate::protocol::CliLedPattern) {
    use crate::proto::config::LedPatternType;

    let type_name = match pattern.pattern_type {
        LedPatternType::LedPatternOff => "off",
        LedPatternType::LedPatternSolid => "solid",
        LedPatternType::LedPatternBreathing => "breathing",
        LedPatternType::LedPatternColorCycle => "color-cycle",
    };

    println!("  Type:       {}", type_name);

    if let Some((r, g, b, w)) = pattern.color {
        println!("  Color:      #{:02x}{:02x}{:02x} (RGBW: {},{},{},{})", r, g, b, r, g, b, w);
    }

    if !pattern.colors.is_empty() {
        println!("  Colors:     {} colors in cycle", pattern.colors.len());
    }

    println!("  Period:     {} ms", pattern.period_ms);
    println!("  Brightness: {}", pattern.brightness);
}
