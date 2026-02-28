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

    /// ESP-NOW peer-to-peer subsystem
    Espnow {
        #[command(subcommand)]
        action: EspnowAction,
    },

    /// Inject simulated touches
    Touch {
        #[command(subcommand)]
        action: TouchAction,
    },

    /// Manage device registry
    Devices {
        #[command(subcommand)]
        action: DevicesAction,
    },

    /// Protocol sniffer - capture and decode DOMES frames
    Sniff {
        /// Filter by protocol (config, trace, ota). Comma-separated.
        #[arg(short, long)]
        filter: Option<String>,

        /// Output raw hex bytes instead of decoded output
        #[arg(long)]
        raw: bool,

        /// Output JSON lines (one JSON object per frame)
        #[arg(long)]
        json: bool,

        /// Stop after N frames
        #[arg(short = 'n', long)]
        count: Option<u32>,
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

    /// Check for available firmware updates (via GitHub releases)
    Check,

    /// Configure auto-update setting
    AutoUpdate {
        /// Enable auto-update
        #[arg(long)]
        enable: bool,

        /// Disable auto-update
        #[arg(long)]
        disable: bool,
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

        /// Span name mapping file (e.g., trace_names.json)
        #[arg(short, long)]
        names: Option<PathBuf>,
    },

    /// Stream trace events in real-time over WiFi/TCP
    Stream {
        /// WiFi address of device (e.g., 192.168.1.100)
        #[arg(long)]
        wifi: String,
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

    /// Get system health diagnostics (heap, tasks, RSSI)
    Health,

    /// Get crash dump (last panic backtrace from NVS)
    CrashDump {
        /// Clear the crash dump after displaying
        #[arg(long)]
        clear: bool,
    },

    /// Get memory profile (heap stats + historical samples)
    Memory {
        /// Output as JSON
        #[arg(long)]
        json: bool,
    },

    /// Run on-device self-test suite (NVS, Heap, Flash, WiFi, BLE)
    SelfTest,
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
enum EspnowAction {
    /// Show ESP-NOW subsystem status (peers, channel, packet stats)
    Status,

    /// Run latency benchmark (ping-pong RTT measurement)
    Bench {
        /// Number of ping-pong rounds (1-1000, default: 100)
        #[arg(short, long, default_value = "100")]
        rounds: u32,
    },

    /// Enable/disable sim drill mode (auto-inject touches during drills)
    SimMode {
        /// Enable or disable sim mode
        #[arg(value_parser = ["on", "off"])]
        state: String,

        /// Delay in ms before touch injection (0 = miss/timeout, default: 500)
        #[arg(long, default_value = "500")]
        delay_ms: u32,

        /// Pad index to inject touches on (0-3, default: 0)
        #[arg(long, default_value = "0")]
        pad: u32,
    },
}

#[derive(Subcommand)]
enum TouchAction {
    /// Inject a simulated touch on a specific pad
    Simulate {
        /// Pad index to inject (0-3)
        #[arg(long, default_value = "0")]
        pad: u32,
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

    // Handle sniff subcommand (manages its own transport)
    if let Some(Commands::Sniff {
        filter,
        raw,
        json,
        count,
    }) = &cli.command
    {
        use commands::sniff::{OutputFormat, ProtocolFilter, SniffOptions};

        let filters: Vec<ProtocolFilter> = filter
            .as_deref()
            .unwrap_or("")
            .split(',')
            .filter(|s| !s.is_empty())
            .filter_map(ProtocolFilter::from_str)
            .collect();

        let format = if *json {
            OutputFormat::Json
        } else if *raw {
            OutputFormat::Raw
        } else {
            OutputFormat::Pretty
        };

        let opts = SniffOptions {
            filters,
            format,
            count: *count,
        };

        // Sniff requires exactly one serial port
        if cli.port.len() != 1 {
            eprintln!("Sniff requires exactly one serial port (--port /dev/ttyACM0)");
            std::process::exit(1);
        }

        return commands::sniff::sniff_serial(&cli.port[0], &opts);
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
                OtaAction::Check => {
                    println!("{}Checking for firmware updates...", prefix);
                    let info = commands::ota_check(transport)?;
                    println!("{}Current version:  {}", prefix,
                        if info.current_version.is_empty() { "unknown" } else { &info.current_version });
                    println!("{}Auto-update:      {}", prefix,
                        if info.auto_update_enabled { "enabled" } else { "disabled" });
                    if info.update_available {
                        println!("{}Update available: {} ({} bytes)", prefix,
                            info.available_version, info.firmware_size);
                    } else {
                        println!("{}No update available", prefix);
                    }
                }
                OtaAction::AutoUpdate { enable, disable } => {
                    let enabled = if *enable && *disable {
                        anyhow::bail!("Cannot specify both --enable and --disable");
                    } else if *enable {
                        true
                    } else if *disable {
                        false
                    } else {
                        anyhow::bail!("Must specify either --enable or --disable");
                    };
                    let result = commands::ota_auto_update(transport, enabled)?;
                    println!("{}Auto-update {}", prefix,
                        if result { "enabled" } else { "disabled" });
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
                    println!("{}  Streaming:   {}", prefix, status.streaming);
                    println!("{}  Events:      {}", prefix, status.event_count);
                    println!("{}  Dropped:     {}", prefix, status.dropped_count);
                    println!("{}  Buffer size: {} bytes", prefix, status.buffer_size);
                }
                TraceAction::Stream { wifi } => {
                    commands::trace_stream(wifi)?;
                }
                TraceAction::Dump { output, names } => {
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
                    let result = commands::trace_dump(transport, &dump_path, names.as_deref())?;
                    println!("{}Dump complete: {} events (pod_id={})", prefix, result.event_count, result.pod_id);
                    if result.dropped_count > 0 {
                        println!("{}  Dropped: {} events", prefix, result.dropped_count);
                    }
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
                SystemAction::Health => {
                    let health = commands::system_health(transport)?;
                    println!("{}System Health:", prefix);
                    println!("{}  Free heap:     {} bytes", prefix, health.free_heap);
                    println!("{}  Min free heap: {} bytes", prefix, health.min_free_heap);
                    println!("{}  Uptime:        {} s", prefix, health.uptime_seconds);
                    if health.wifi_rssi != 0 {
                        println!("{}  WiFi RSSI:     {} dBm", prefix, health.wifi_rssi);
                    } else {
                        println!("{}  WiFi RSSI:     n/a (not connected)", prefix);
                    }
                    if !health.tasks.is_empty() {
                        println!("{}  Tasks ({}):", prefix, health.tasks.len());
                        println!("{}    {:<16} {:>6} {:>4} {:>4}", prefix, "NAME", "STACK", "PRI", "CORE");
                        println!("{}    {:-<16} {:->6} {:->4} {:->4}", prefix, "", "", "", "");
                        for task in &health.tasks {
                            println!("{}    {:<16} {:>6} {:>4} {:>4}",
                                prefix, task.name, task.stack_high_water, task.priority, task.core);
                        }
                    }
                }
                SystemAction::CrashDump { clear } => {
                    let dump = commands::system_crash_dump(transport)?;
                    if dump.has_dump {
                        println!("{}Crash Dump:", prefix);
                        println!("{}  Reason:    {}", prefix, dump.reason);
                        println!("{}  Task:      {}", prefix, dump.task_name);
                        println!("{}  Uptime:    {} s", prefix, dump.uptime_s);
                        println!("{}  Free heap: {} bytes", prefix, dump.free_heap);
                        if !dump.backtrace.is_empty() {
                            println!("{}  Backtrace:", prefix);
                            for (i, addr) in dump.backtrace.iter().enumerate() {
                                println!("{}    #{}: 0x{:08X}", prefix, i, addr);
                            }
                            println!("{}  (use addr2line -e build/domes.elf to resolve)", prefix);
                        }
                        if *clear {
                            let cleared = commands::system_clear_crash_dump(transport)?;
                            if cleared {
                                println!("{}Crash dump cleared.", prefix);
                            } else {
                                println!("{}Failed to clear crash dump.", prefix);
                            }
                        }
                    } else {
                        println!("{}No crash dump stored.", prefix);
                    }
                }
                SystemAction::Memory { json } => {
                    let profile = commands::system_memory_profile(transport)?;
                    if *json {
                        // JSON output
                        println!("{{");
                        println!("  \"current_free_heap\": {},", profile.current_free_heap);
                        println!("  \"current_min_free_heap\": {},", profile.current_min_free_heap);
                        println!("  \"current_largest_block\": {},", profile.current_largest_block);
                        println!("  \"total_heap\": {},", profile.total_heap);
                        println!("  \"usage_pct\": {:.1},",
                            if profile.total_heap > 0 {
                                (1.0 - profile.current_free_heap as f64 / profile.total_heap as f64) * 100.0
                            } else { 0.0 });
                        println!("  \"samples\": [");
                        for (i, s) in profile.samples.iter().enumerate() {
                            let comma = if i + 1 < profile.samples.len() { "," } else { "" };
                            println!("    {{\"t\": {}, \"free\": {}, \"largest\": {}, \"min_free\": {}}}{}",
                                s.timestamp_s, s.free_heap, s.largest_block, s.min_free_heap, comma);
                        }
                        println!("  ]");
                        println!("}}");
                    } else {
                        let usage_pct = if profile.total_heap > 0 {
                            (1.0 - profile.current_free_heap as f64 / profile.total_heap as f64) * 100.0
                        } else { 0.0 };
                        println!("{}Memory Profile:", prefix);
                        println!("{}  Total heap:      {} bytes", prefix, profile.total_heap);
                        println!("{}  Free heap:       {} bytes ({:.1}% used)", prefix, profile.current_free_heap, usage_pct);
                        println!("{}  Min free heap:   {} bytes", prefix, profile.current_min_free_heap);
                        println!("{}  Largest block:   {} bytes", prefix, profile.current_largest_block);
                        if !profile.samples.is_empty() {
                            println!("{}  History ({} samples):", prefix, profile.samples.len());
                            // Sparkline using free heap values
                            let values: Vec<u32> = profile.samples.iter().map(|s| s.free_heap).collect();
                            let min_val = *values.iter().min().unwrap_or(&0);
                            let max_val = *values.iter().max().unwrap_or(&1);
                            let range = if max_val > min_val { max_val - min_val } else { 1 };
                            let spark_chars = ['▁', '▂', '▃', '▄', '▅', '▆', '▇', '█'];
                            let sparkline: String = values.iter().map(|v| {
                                let idx = (((*v - min_val) as f64 / range as f64) * 7.0) as usize;
                                spark_chars[idx.min(7)]
                            }).collect();
                            println!("{}    Free heap: {} ({}-{} bytes)", prefix, sparkline, min_val, max_val);
                        }
                    }
                }
                SystemAction::SelfTest => {
                    println!("{}Running on-device self-test suite...", prefix);
                    let info = commands::system_self_test(transport)?;
                    println!("{}Self-Test Results: {}/{} passed", prefix, info.tests_passed, info.tests_run);
                    println!("{}{:<8} {:<6} {}", prefix, "TEST", "STATUS", "MESSAGE");
                    println!("{}{:-<8} {:-<6} {:-<40}", prefix, "", "", "");
                    for result in &info.results {
                        let status = if result.passed { "PASS" } else { "FAIL" };
                        println!("{}{:<8} {:<6} {}", prefix, result.name, status, result.message);
                    }
                    if info.tests_passed == info.tests_run {
                        println!("{}All tests passed!", prefix);
                    } else {
                        println!("{}{} test(s) FAILED", prefix, info.tests_run - info.tests_passed);
                    }
                }
            },

            Commands::Espnow { action } => match action {
                EspnowAction::Status => {
                    let status = commands::espnow_status(transport)?;
                    println!("{}ESP-NOW Status:", prefix);
                    println!("{}  State:      {}", prefix, status.discovery_state);
                    println!("{}  Channel:    {}", prefix, status.channel);
                    println!("{}  Peers:      {}", prefix, status.peer_count);
                    println!("{}  TX packets: {}", prefix, status.tx_count);
                    println!("{}  RX packets: {}", prefix, status.rx_count);
                    println!("{}  TX fails:   {}", prefix, status.tx_fail_count);
                    if status.last_rtt_us > 0 {
                        println!("{}  Last RTT:   {} us", prefix, status.last_rtt_us);
                    }
                    if !status.peers.is_empty() {
                        println!("{}  Discovered peers:", prefix);
                        println!("{}    {:<20} {:>6} {:>10}", prefix, "MAC", "RSSI", "LAST SEEN");
                        println!("{}    {:-<20} {:->6} {:->10}", prefix, "", "", "");
                        for peer in &status.peers {
                            println!("{}    {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}   {:>4} {:>8} ms",
                                prefix,
                                peer.mac[0], peer.mac[1], peer.mac[2],
                                peer.mac[3], peer.mac[4], peer.mac[5],
                                peer.rssi, peer.last_seen_ms);
                        }
                    }
                }
                EspnowAction::Bench { rounds } => {
                    println!("{}Running ESP-NOW latency benchmark ({} rounds)...", prefix, rounds);
                    let result = commands::espnow_bench(transport, *rounds)?;
                    println!("{}ESP-NOW Benchmark Results:", prefix);
                    println!("{}  Rounds:     {}/{} completed ({} failed)",
                        prefix, result.rounds_completed,
                        result.rounds_completed + result.rounds_failed,
                        result.rounds_failed);
                    if result.rounds_completed > 0 {
                        println!("{}  Min RTT:    {} us ({:.2} ms)",
                            prefix, result.min_rtt_us, result.min_rtt_us as f64 / 1000.0);
                        println!("{}  Max RTT:    {} us ({:.2} ms)",
                            prefix, result.max_rtt_us, result.max_rtt_us as f64 / 1000.0);
                        println!("{}  Mean RTT:   {} us ({:.2} ms)",
                            prefix, result.mean_rtt_us, result.mean_rtt_us as f64 / 1000.0);
                        println!("{}  P50 RTT:    {} us ({:.2} ms)",
                            prefix, result.p50_rtt_us, result.p50_rtt_us as f64 / 1000.0);
                        println!("{}  P95 RTT:    {} us ({:.2} ms)",
                            prefix, result.p95_rtt_us, result.p95_rtt_us as f64 / 1000.0);
                        println!("{}  P99 RTT:    {} us ({:.2} ms)",
                            prefix, result.p99_rtt_us, result.p99_rtt_us as f64 / 1000.0);
                    }
                }
                EspnowAction::SimMode { state, delay_ms, pad } => {
                    let enabled = state == "on";
                    let result = commands::espnow_sim_mode(transport, enabled, *delay_ms, *pad)?;
                    println!("{}Sim mode: {}", prefix, if result.enabled { "ON" } else { "OFF" });
                    if result.enabled {
                        println!("{}  Delay:  {} ms", prefix, result.delay_ms);
                        println!("{}  Pad:    {}", prefix, result.pad_index);
                    }
                }
            },

            Commands::Touch { action } => match action {
                TouchAction::Simulate { pad } => {
                    commands::touch_simulate(transport, *pad)?;
                    println!("{}Injected touch on pad {}", prefix, pad);
                }
            },

            Commands::Devices { .. } | Commands::Sniff { .. } => unreachable!(), // Handled above
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
