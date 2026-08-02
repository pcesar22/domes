//! DOMES CLI - Runtime configuration tool for DOMES firmware
//!
//! Usage (Serial):
//!   domes-cli --port /dev/ttyUSB0 feature list
//!   domes-cli --port /dev/ttyUSB0 feature enable led-effects
//!   domes-cli --port /dev/ttyUSB0 feature disable ble
//!   domes-cli --port /dev/ttyUSB0 wifi enable
//!   domes-cli --port /dev/ttyUSB0 wifi disable
//!   domes-cli --port /dev/ttyUSB0 wifi status
//!   domes-cli --port /dev/ttyUSB0 led get
//!   domes-cli --port /dev/ttyUSB0 led off
//!   domes-cli --port /dev/ttyUSB0 led solid --color ff0000
//!   domes-cli --port /dev/ttyUSB0 led breathing --color 00ff00 --period 2000
//!   domes-cli --port /dev/ttyUSB0 led cycle --period 3000
//!   domes-cli --port /dev/ttyUSB0 ota flash firmware.bin --version v1.2.3
//!   domes-cli --port /dev/ttyUSB0 trace start
//!   domes-cli --port /dev/ttyUSB0 trace stop
//!   domes-cli --port /dev/ttyUSB0 trace status
//!   domes-cli --port /dev/ttyUSB0 trace dump -o trace.json
//!   domes-cli --port /dev/ttyUSB0 system mode
//!   domes-cli --port /dev/ttyUSB0 system set-mode triage
//!   domes-cli --port /dev/ttyUSB0 system info
//!
//! Usage (WiFi):
//!   domes-cli --wifi 192.168.1.100:5000 feature list
//!   domes-cli --wifi 192.168.1.100:5000 feature enable led-effects
//!   domes-cli --wifi 192.168.1.100:5000 wifi status
//!   domes-cli --wifi 192.168.1.100:5000 led cycle --period 2000
//!   domes-cli --wifi 192.168.1.100:5000 ota check
//!
//! Usage (BLE):
//!   domes-cli --scan-ble                           # Scan for nearby DOMES devices
//!   domes-cli --ble "DOMES-Pod-01" feature list    # Connect by exact name
//!   domes-cli --ble "AA:BB:CC:DD:EE:FF" led solid  # Connect by MAC address
//!
//! Multi-device usage:
//!   domes-cli --port /dev/ttyUSB0 --port /dev/ttyUSB1 feature list
//!   domes-cli --target pod1 --target pod2 led solid --color ff0000
//!   domes-cli --all feature list
//!
//! Device registry:
//!   domes-cli devices scan
//!   domes-cli devices add pod1 serial /dev/serial/by-id/usb-Silicon_Labs_CP2102N...
//!   domes-cli devices add pod2 serial /dev/ttyUSB1
//!   domes-cli devices list
//!   domes-cli devices remove pod1

mod commands;
mod device;
mod proto;
mod protocol;
mod transport;

use anyhow::Context;
use clap::{Parser, Subcommand};
use proto::config::{Feature, SystemMode};
use std::path::PathBuf;
use std::time::Duration;
use transport::{BleTransport, SerialTransport};

#[derive(Parser)]
#[command(name = "domes-cli")]
#[command(version, about = "DOMES firmware runtime configuration CLI")]
struct Cli {
    /// CP210 UART port(s) (e.g., /dev/ttyUSB0 or /dev/serial/by-id/...). Repeatable.
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
    #[arg(
        long,
        conflicts_with_all = ["port", "wifi", "ble", "target", "connect_all_ble"]
    )]
    all: bool,

    /// Scan for nearby BLE devices
    #[arg(long)]
    scan_ble: bool,

    /// Auto-connect to all BLE devices with DOMES-Pod prefix
    #[arg(long)]
    connect_all_ble: bool,

    /// List available serial ports without opening them
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

    /// Manage the WiFi runtime feature flag
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
        #[arg(short, long, value_parser = validate_sniff_filter_list)]
        filter: Option<String>,

        /// Output raw hex bytes instead of decoded output
        #[arg(long, conflicts_with = "json")]
        raw: bool,

        /// Output JSON lines (one JSON object per frame)
        #[arg(long, conflicts_with = "raw")]
        json: bool,

        /// Stop after N frames
        #[arg(short = 'n', long, value_parser = clap::value_parser!(u32).range(1..))]
        count: Option<u32>,
    },
}

#[derive(Subcommand)]
enum FeatureAction {
    /// List all features and their current state
    List,

    /// Show one feature's current state
    Status {
        /// Feature name (e.g., led-effects, ble, wifi, esp-now, touch, haptic, audio)
        feature: String,
    },

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
    /// Enable the WiFi feature flag
    Enable,

    /// Disable the WiFi feature flag
    Disable,

    /// Show the WiFi feature flag (not AP connection state)
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
        version: String,
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
        #[arg(value_parser = ["idle", "triage", "connected", "game", "error"])]
        mode: String,
    },

    /// Get system information (version, uptime, heap, etc.)
    Info,

    /// Set pod ID (1-255, persisted to NVS, reboot for BLE name change)
    SetPodId {
        /// Pod ID (1-255)
        #[arg(value_parser = clap::value_parser!(u32).range(1..=255))]
        id: u32,
    },

    /// Get system health diagnostics (heap, tasks, RSSI)
    Health,

    /// Get the clean-restart diagnostic snapshot stored in NVS
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

    /// Run the on-device system and peripheral initialization checks
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
        #[arg(short, long, default_value = "2000", value_parser = clap::value_parser!(u32).range(1..))]
        period: u32,

        /// Brightness (0-255)
        #[arg(short, long, default_value = "128")]
        brightness: u8,
    },

    /// Set color cycle pattern (automatic color transitions)
    Cycle {
        /// Cycle period in ms (time between color changes)
        #[arg(short, long, default_value = "2000", value_parser = clap::value_parser!(u32).range(1..))]
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
        #[arg(
            short,
            long,
            default_value = "100",
            value_parser = clap::value_parser!(u32).range(1..=1000)
        )]
        rounds: u32,
    },

    /// Enable/disable sim drill mode (auto-inject touches during drills)
    SimMode {
        /// Enable or disable sim mode
        #[arg(value_parser = ["on", "off"])]
        state: String,

        /// Delay in ms before touch injection (0 = miss/timeout, default: 500)
        #[arg(
            long,
            default_value = "500",
            value_parser = clap::value_parser!(u32).range(0..=3000)
        )]
        delay_ms: u32,

        /// Pad index to inject touches on (0-3, default: 0)
        #[arg(long, default_value = "0", value_parser = clap::value_parser!(u32).range(0..=3))]
        pad: u32,
    },
}

#[derive(Subcommand)]
enum TouchAction {
    /// Inject a simulated touch on a specific pad
    Simulate {
        /// Pad index to inject (0-3)
        #[arg(long, default_value = "0", value_parser = clap::value_parser!(u32).range(0..=3))]
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

        /// Transport type (serial, wifi, tcp, ble)
        #[arg(value_parser = ["serial", "wifi", "tcp", "ble"])]
        transport: String,

        /// Address (e.g., /dev/ttyUSB0, /dev/serial/by-id/..., HOST:PORT, DOMES-Pod-01)
        address: String,
    },

    /// Remove a device from the registry
    Remove {
        /// Device name to remove
        name: String,
    },

    /// Enumerate serial ports without opening them and scan BLE advertisements
    Scan,
}

fn main() -> anyhow::Result<()> {
    let mut cli = Cli::parse();

    // Reject unsupported command/transport combinations before any transport
    // discovery or connection attempt.
    validate_ota_flash_transport_selection(&cli)?;
    validate_trace_transport_selection(&cli)?;

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

    // Trace streaming connects directly to the dedicated TCP port and does not
    // use the generic serial/TCP/BLE transport resolver.
    if let Some(Commands::Trace {
        action: TraceAction::Stream { wifi },
    }) = &cli.command
    {
        validate_trace_stream_selection(&cli)?;
        return commands::trace_stream(wifi);
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
            println!("{:<20} ADDRESS", "NAME");
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
            .map(|filter| {
                filter
                    .split(',')
                    .map(|value| {
                        ProtocolFilter::from_str(value)
                            .expect("sniff filters were validated by clap")
                    })
                    .collect()
            })
            .unwrap_or_default();

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
            eprintln!("Sniff requires exactly one serial port (--port /dev/ttyUSB0)");
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
                    println!("{:<12} {:<10} ADDRESS", "NAME", "TRANSPORT");
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
                    anyhow::bail!("Device '{}' not found", name);
                }
                return Ok(());
            }
            DevicesAction::Scan => {
                println!("Enumerating serial ports and scanning DOMES BLE advertisements...\n");

                // Enumeration does not open ports, so scanning cannot toggle the
                // CP210 modem-control lines or reset a working board.
                let ports: Vec<String> = SerialTransport::list_ports()?
                    .into_iter()
                    .filter(|port| is_runtime_serial_candidate(port))
                    .collect();
                if !ports.is_empty() {
                    println!("Serial ports (not probed):");
                    for port in &ports {
                        println!("  {:<32} (CP210/UART candidate)", port);
                    }
                    println!();
                } else {
                    println!("No serial devices found\n");
                }

                // Scan BLE
                println!("Scanning BLE (10 seconds)...");
                let ble_devices = BleTransport::scan_devices(Duration::from_secs(10)).context(
                    "BLE scan failed; check the adapter, BlueZ service, and permissions",
                )?;
                if !ble_devices.is_empty() {
                    println!("BLE devices:");
                    for (name, addr) in &ble_devices {
                        let display_name = if name.is_empty() { "(unknown)" } else { name };
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

    let Some(command) = cli.command else {
        eprintln!("No command specified. Use --help for usage.");
        std::process::exit(1);
    };

    // Resolve device connections
    let resolution = device::resolve_devices(&cli.port, &cli.wifi, &cli.ble, &cli.target, cli.all)?;
    let multi = resolution.is_multi();
    let mut devices = resolution.connections;
    let mut failures: Vec<String> = Vec::new();

    for failure in resolution.failures {
        eprintln!("[{}] Connection error: {}", failure.name, failure.error);
        failures.push(failure.name);
    }

    if devices.is_empty() {
        if !failures.is_empty() {
            return failed_devices(&failures);
        }
        eprintln!("No transport specified. Use --port, --wifi, --ble, --target, or --all");
        eprintln!("Use --list-ports to see serial ports, --scan-ble for BLE devices.");
        eprintln!("Use 'domes-cli devices add <name> <type> <addr>' to register devices.");
        std::process::exit(1);
    }

    // Keep machine-readable memory output free of connection banners and
    // per-device separators. Multi-device output is one JSON document keyed
    // by the resolved device labels.
    if matches!(
        &command,
        Commands::System {
            action: SystemAction::Memory { json: true }
        }
    ) {
        let mut profiles = serde_json::Map::new();
        for dev in devices.iter_mut() {
            match commands::system_memory_profile(dev.transport.as_mut()) {
                Ok(profile) => {
                    profiles.insert(dev.name.clone(), memory_profile_json(&profile));
                }
                Err(error) => {
                    eprintln!("[{}] Error: {:#}", dev.name, error);
                    failures.push(dev.name.clone());
                }
            }
        }

        let output = if multi {
            serde_json::json!({ "devices": profiles })
        } else {
            profiles
                .into_values()
                .next()
                .unwrap_or_else(|| serde_json::json!({}))
        };
        println!("{}", serde_json::to_string_pretty(&output)?);
        return failed_devices(&failures);
    }

    // Execute command on each device
    for dev in devices.iter_mut() {
        let prefix = if multi {
            device::device_prefix(&dev.name)
        } else {
            String::new()
        };
        let transport = dev.transport.as_mut();
        let dev_label = dev.name.clone();

        if multi {
            println!("--- {} ---", dev_label);
        }

        let result: anyhow::Result<()> = (|| {
            match &command {
                Commands::Feature { action } => match action {
                    FeatureAction::List => {
                        let features = commands::feature_list(transport)?;
                        println!("{}Features:", prefix);
                        println!("{}{:<16} STATUS", prefix, "NAME");
                        println!("{}{:-<16} {:-<8}", prefix, "", "");
                        for state in features {
                            let status = if state.enabled { "enabled" } else { "disabled" };
                            println!("{}{:<16} {}", prefix, state.feature.cli_name(), status);
                        }
                    }
                    FeatureAction::Status { feature } => {
                        let feature: Feature = feature
                            .parse()
                            .map_err(|_| anyhow::anyhow!("Unknown feature: {}", feature))?;
                        let state = commands::feature_status(transport, feature)?;
                        ensure_feature_identity(feature, &state)?;
                        println!(
                            "{}Feature '{}' is {}",
                            prefix,
                            state.feature.cli_name(),
                            if state.enabled { "enabled" } else { "disabled" }
                        );
                    }
                    FeatureAction::Enable { feature } => {
                        let feature: Feature = feature
                            .parse()
                            .map_err(|_| anyhow::anyhow!("Unknown feature: {}", feature))?;
                        let state = commands::feature_enable(transport, feature)?;
                        ensure_feature_state(feature, true, &state)?;
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
                        ensure_feature_state(feature, false, &state)?;
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
                        ensure_state("WiFi feature flag", true, enabled)?;
                        println!("{}WiFi feature flag enabled", prefix);
                    }
                    WifiAction::Disable => {
                        let disabled = commands::wifi_disable(transport)?;
                        ensure_command_succeeded("disable WiFi feature flag", disabled)?;
                        println!("{}WiFi feature flag disabled", prefix);
                    }
                    WifiAction::Status => {
                        let enabled = commands::wifi_status(transport)?;
                        println!(
                            "{}WiFi feature flag: {}",
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
                        commands::ota_flash(transport, firmware, version)?;
                    }
                    OtaAction::Check => {
                        println!("{}Checking for firmware updates...", prefix);
                        let info = commands::ota_check(transport)?;
                        println!(
                            "{}Current version:  {}",
                            prefix,
                            if info.current_version.is_empty() {
                                "unknown"
                            } else {
                                &info.current_version
                            }
                        );
                        println!(
                            "{}Auto-update:      {}",
                            prefix,
                            if info.auto_update_enabled {
                                "enabled"
                            } else {
                                "disabled"
                            }
                        );
                        if info.update_available {
                            println!(
                                "{}Update available: {} ({} bytes)",
                                prefix, info.available_version, info.firmware_size
                            );
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
                        ensure_state("Auto-update", enabled, result)?;
                        println!(
                            "{}Auto-update {}",
                            prefix,
                            if result { "enabled" } else { "disabled" }
                        );
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
                    TraceAction::Stream { .. } => {
                        unreachable!("trace stream is handled before transport resolution")
                    }
                    TraceAction::Dump { output, names } => {
                        let dump_path = if multi {
                            // Per-device output file
                            let stem = output.file_stem().unwrap_or_default().to_string_lossy();
                            let ext = output.extension().unwrap_or_default().to_string_lossy();
                            output.with_file_name(format!("{}-{}.{}", stem, dev.name, ext))
                        } else {
                            output.clone()
                        };
                        println!("{}Dumping traces to {}...", prefix, dump_path.display());
                        let result = commands::trace_dump(transport, &dump_path, names.as_deref())?;
                        println!(
                            "{}Dump complete: {} events (pod_id={})",
                            prefix, result.event_count, result.pod_id
                        );
                        if result.dropped_count > 0 {
                            println!("{}  Dropped: {} events", prefix, result.dropped_count);
                        }
                        if result.duration_us > 0 {
                            println!("{}  Duration: {} us", prefix, result.duration_us);
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
                        ensure_state("IMU triage mode", enabled, result)?;
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
                        ensure_mode_transition(mode, new_mode, ok)?;
                        println!("{}System mode set to: {}", prefix, new_mode);
                    }
                    SystemAction::Info => {
                        let info = commands::system_info(transport)?;
                        println!("{}System Information:", prefix);
                        println!("{}  Firmware:   {}", prefix, info.firmware_version);
                        println!(
                            "{}  Pod ID:     {}",
                            prefix,
                            if info.pod_id == 0 {
                                "not set".to_string()
                            } else {
                                info.pod_id.to_string()
                            }
                        );
                        println!("{}  Mode:       {}", prefix, info.mode);
                        println!("{}  Uptime:     {} s", prefix, info.uptime_s);
                        println!("{}  Free heap:  {} bytes", prefix, info.free_heap);
                        println!("{}  Boot count: {}", prefix, info.boot_count);
                        println!(
                            "{}  Reset:      {} ({})",
                            prefix,
                            info.reset_reason.cli_name(),
                            info.reset_reason as i32
                        );
                        println!("{}  Features:   0x{:08X}", prefix, info.feature_mask);
                    }
                    SystemAction::SetPodId { id } => {
                        let new_id = commands::system_set_pod_id(transport, *id)?;
                        println!(
                            "{}Pod ID set to {} (reboot device for BLE name change)",
                            prefix, new_id
                        );
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
                            println!(
                                "{}    {:<16} {:>6} {:>4} {:>4}",
                                prefix, "NAME", "STACK", "PRI", "CORE"
                            );
                            println!("{}    {:-<16} {:->6} {:->4} {:->4}", prefix, "", "", "", "");
                            for task in &health.tasks {
                                println!(
                                    "{}    {:<16} {:>6} {:>4} {:>4}",
                                    prefix,
                                    task.name,
                                    task.stack_high_water,
                                    task.priority,
                                    task.core
                                );
                            }
                        }
                        validate_system_health(&health)?;
                    }
                    SystemAction::CrashDump { clear } => {
                        let dump = commands::system_crash_dump(transport)?;
                        if dump.has_dump {
                            println!("{}Crash Dump:", prefix);
                            println!("{}  Reason:    {}", prefix, dump.reason);
                            println!("{}  Task:      {}", prefix, dump.task_name);
                            println!("{}  Uptime:    {} s", prefix, dump.uptime_s);
                            println!("{}  Free heap: {} bytes", prefix, dump.free_heap);
                            println!("{}  Boot count: {}", prefix, dump.timestamp);
                            if !dump.backtrace.is_empty() {
                                println!("{}  Backtrace:", prefix);
                                for (i, addr) in dump.backtrace.iter().enumerate() {
                                    println!("{}    #{}: 0x{:08X}", prefix, i, addr);
                                }
                                println!(
                                    "{}  (use addr2line -e build/domes.elf to resolve)",
                                    prefix
                                );
                            }
                            if *clear {
                                let cleared = commands::system_clear_crash_dump(transport)?;
                                ensure_command_succeeded("clear crash dump", cleared)?;
                                println!("{}Crash dump cleared.", prefix);
                            }
                        } else {
                            println!("{}No crash dump stored.", prefix);
                        }
                    }
                    SystemAction::Memory { json } => {
                        let profile = commands::system_memory_profile(transport)?;
                        if *json {
                            println!(
                                "{}",
                                serde_json::to_string_pretty(&memory_profile_json(&profile))?
                            );
                        } else {
                            let usage_pct = if profile.total_heap > 0 {
                                (1.0 - profile.current_free_heap as f64 / profile.total_heap as f64)
                                    * 100.0
                            } else {
                                0.0
                            };
                            println!("{}Memory Profile:", prefix);
                            println!("{}  Total heap:      {} bytes", prefix, profile.total_heap);
                            println!(
                                "{}  Free heap:       {} bytes ({:.1}% used)",
                                prefix, profile.current_free_heap, usage_pct
                            );
                            println!(
                                "{}  Min free heap:   {} bytes",
                                prefix, profile.current_min_free_heap
                            );
                            println!(
                                "{}  Largest block:   {} bytes",
                                prefix, profile.current_largest_block
                            );
                            if !profile.samples.is_empty() {
                                println!(
                                    "{}  History ({} samples):",
                                    prefix,
                                    profile.samples.len()
                                );
                                // Sparkline using free heap values
                                let values: Vec<u32> =
                                    profile.samples.iter().map(|s| s.free_heap).collect();
                                let min_val = *values.iter().min().unwrap_or(&0);
                                let max_val = *values.iter().max().unwrap_or(&1);
                                let range = if max_val > min_val {
                                    max_val - min_val
                                } else {
                                    1
                                };
                                let spark_chars = ['▁', '▂', '▃', '▄', '▅', '▆', '▇', '█'];
                                let sparkline: String = values
                                    .iter()
                                    .map(|v| {
                                        let idx =
                                            (((*v - min_val) as f64 / range as f64) * 7.0) as usize;
                                        spark_chars[idx.min(7)]
                                    })
                                    .collect();
                                println!(
                                    "{}    Free heap: {} ({}-{} bytes)",
                                    prefix, sparkline, min_val, max_val
                                );
                            }
                        }
                    }
                    SystemAction::SelfTest => {
                        println!("{}Running on-device self-test suite...", prefix);
                        let info = commands::system_self_test(transport)?;
                        println!(
                            "{}Self-Test Results: {}/{} passed",
                            prefix, info.tests_passed, info.tests_run
                        );
                        println!("{}{:<8} {:<6} MESSAGE", prefix, "TEST", "STATUS");
                        println!("{}{:-<8} {:-<6} {:-<40}", prefix, "", "", "");
                        for result in &info.results {
                            let status = if result.passed { "PASS" } else { "FAIL" };
                            println!(
                                "{}{:<8} {:<6} {}",
                                prefix, result.name, status, result.message
                            );
                        }
                        if info.tests_passed == info.tests_run {
                            println!("{}All tests passed!", prefix);
                        } else {
                            println!(
                                "{}{} test(s) FAILED",
                                prefix,
                                info.tests_run - info.tests_passed
                            );
                        }
                        ensure_self_test_passed(&info)?;
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
                            println!(
                                "{}    {:<20} {:>6} {:>10}",
                                prefix, "MAC", "RSSI", "LAST SEEN"
                            );
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
                        println!(
                            "{}Running ESP-NOW latency benchmark ({} rounds)...",
                            prefix, rounds
                        );
                        let result = commands::espnow_bench(transport, *rounds)?;
                        println!("{}ESP-NOW Benchmark Results:", prefix);
                        println!(
                            "{}  Rounds:     {}/{} completed ({} failed)",
                            prefix,
                            result.rounds_completed,
                            result.rounds_completed + result.rounds_failed,
                            result.rounds_failed
                        );
                        if result.rounds_completed > 0 {
                            println!(
                                "{}  Min RTT:    {} us ({:.2} ms)",
                                prefix,
                                result.min_rtt_us,
                                result.min_rtt_us as f64 / 1000.0
                            );
                            println!(
                                "{}  Max RTT:    {} us ({:.2} ms)",
                                prefix,
                                result.max_rtt_us,
                                result.max_rtt_us as f64 / 1000.0
                            );
                            println!(
                                "{}  Mean RTT:   {} us ({:.2} ms)",
                                prefix,
                                result.mean_rtt_us,
                                result.mean_rtt_us as f64 / 1000.0
                            );
                            println!(
                                "{}  P50 RTT:    {} us ({:.2} ms)",
                                prefix,
                                result.p50_rtt_us,
                                result.p50_rtt_us as f64 / 1000.0
                            );
                            println!(
                                "{}  P95 RTT:    {} us ({:.2} ms)",
                                prefix,
                                result.p95_rtt_us,
                                result.p95_rtt_us as f64 / 1000.0
                            );
                            println!(
                                "{}  P99 RTT:    {} us ({:.2} ms)",
                                prefix,
                                result.p99_rtt_us,
                                result.p99_rtt_us as f64 / 1000.0
                            );
                        }
                        ensure_benchmark_complete(*rounds, &result)?;
                    }
                    EspnowAction::SimMode {
                        state,
                        delay_ms,
                        pad,
                    } => {
                        let enabled = state == "on";
                        let result =
                            commands::espnow_sim_mode(transport, enabled, *delay_ms, *pad)?;
                        ensure_sim_mode_state(enabled, *delay_ms, *pad, &result)?;
                        println!(
                            "{}Sim mode: {}",
                            prefix,
                            if result.enabled { "ON" } else { "OFF" }
                        );
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

    failed_devices(&failures)
}

fn validate_trace_stream_selection(cli: &Cli) -> anyhow::Result<()> {
    let has_global_selector = !cli.port.is_empty()
        || !cli.wifi.is_empty()
        || !cli.ble.is_empty()
        || !cli.target.is_empty()
        || cli.all
        || cli.scan_ble
        || cli.connect_all_ble;

    if has_global_selector {
        anyhow::bail!(
            "trace stream supports one WiFi host via `trace stream --wifi HOST`; do not combine it with global transport or multi-device selectors"
        );
    }

    Ok(())
}

fn validate_sniff_filter_list(value: &str) -> Result<String, String> {
    if value.is_empty() {
        return Err("filter list must not be empty".to_string());
    }

    for filter in value.split(',') {
        if filter.is_empty() {
            return Err("filter list must not contain empty entries".to_string());
        }
        if commands::sniff::ProtocolFilter::from_str(filter).is_none() {
            return Err(format!(
                "unknown protocol filter '{filter}'; expected config, trace, or ota"
            ));
        }
    }

    Ok(value.to_string())
}

fn memory_profile_json(profile: &crate::protocol::CliMemoryProfile) -> serde_json::Value {
    let usage_pct = if profile.total_heap > 0 {
        (1.0 - profile.current_free_heap as f64 / profile.total_heap as f64) * 100.0
    } else {
        0.0
    };
    let samples: Vec<_> = profile
        .samples
        .iter()
        .map(|sample| {
            serde_json::json!({
                "t": sample.timestamp_s,
                "free": sample.free_heap,
                "largest": sample.largest_block,
                "min_free": sample.min_free_heap,
            })
        })
        .collect();

    serde_json::json!({
        "current_free_heap": profile.current_free_heap,
        "current_min_free_heap": profile.current_min_free_heap,
        "current_largest_block": profile.current_largest_block,
        "total_heap": profile.total_heap,
        "usage_pct": usage_pct,
        "samples": samples,
    })
}

fn validate_trace_transport_selection(cli: &Cli) -> anyhow::Result<()> {
    let uses_config_transport = matches!(
        cli.command.as_ref(),
        Some(Commands::Trace {
            action: TraceAction::Start
                | TraceAction::Stop
                | TraceAction::Clear
                | TraceAction::Status
                | TraceAction::Dump { .. }
        })
    );

    if !uses_config_transport {
        return Ok(());
    }

    if !cli.wifi.is_empty() {
        anyhow::bail!(
            "Trace control and dump commands are not supported over the WiFi/TCP config transport; use serial or BLE, or use `trace stream --wifi HOST` for live streaming"
        );
    }

    if cli.target.is_empty() && !cli.all {
        return Ok(());
    }

    let registry = device::load_device_registry()?;
    let unsupported_targets = selected_wifi_registry_targets(cli, &registry);

    if !unsupported_targets.is_empty() {
        anyhow::bail!(
            "Trace control and dump commands are not supported over the WiFi/TCP config transport (selected target(s): {}); use serial or BLE targets",
            unsupported_targets.join(", ")
        );
    }

    Ok(())
}

fn validate_ota_flash_transport_selection(cli: &Cli) -> anyhow::Result<()> {
    if !matches!(
        cli.command.as_ref(),
        Some(Commands::Ota {
            action: OtaAction::Flash { .. }
        })
    ) {
        return Ok(());
    }

    if !cli.wifi.is_empty() {
        anyhow::bail!(
            "Raw OTA flash is not supported over WiFi/TCP; use --port for serial or --ble for BLE"
        );
    }

    if cli.target.is_empty() && !cli.all {
        return Ok(());
    }

    let registry = device::load_device_registry()?;
    let unsupported_targets = selected_wifi_registry_targets(cli, &registry);

    if !unsupported_targets.is_empty() {
        anyhow::bail!(
            "Raw OTA flash is not supported over WiFi/TCP (selected target(s): {}); use serial or BLE targets",
            unsupported_targets.join(", ")
        );
    }

    Ok(())
}

fn is_wifi_transport_type(transport_type: &str) -> bool {
    matches!(transport_type, "wifi" | "tcp")
}

fn is_runtime_serial_candidate(port: &str) -> bool {
    port.starts_with("/dev/ttyUSB")
        || (port.starts_with("/dev/serial/by-id/") && port.contains("CP2102N"))
}

fn selected_wifi_registry_targets(
    cli: &Cli,
    registry: &std::collections::HashMap<String, device::DeviceEntry>,
) -> Vec<String> {
    let mut selected: Vec<String> = if cli.all {
        registry
            .iter()
            .filter(|(_, entry)| is_wifi_transport_type(&entry.transport_type))
            .map(|(name, _)| name.clone())
            .collect()
    } else {
        cli.target
            .iter()
            .filter(|name| {
                registry
                    .get(*name)
                    .map(|entry| is_wifi_transport_type(&entry.transport_type))
                    .unwrap_or(false)
            })
            .cloned()
            .collect()
    };
    selected.sort();
    selected
}

fn ensure_feature_state(
    requested_feature: Feature,
    expected_enabled: bool,
    state: &crate::protocol::CliFeatureState,
) -> anyhow::Result<()> {
    ensure_feature_identity(requested_feature, state)?;

    ensure_state(
        &format!("Feature '{}'", requested_feature),
        expected_enabled,
        state.enabled,
    )
}

fn ensure_feature_identity(
    requested_feature: Feature,
    state: &crate::protocol::CliFeatureState,
) -> anyhow::Result<()> {
    if state.feature != requested_feature {
        anyhow::bail!(
            "Feature command returned state for '{}', expected '{}'",
            state.feature,
            requested_feature
        );
    }

    Ok(())
}

fn ensure_state(subject: &str, expected_enabled: bool, actual_enabled: bool) -> anyhow::Result<()> {
    if expected_enabled != actual_enabled {
        anyhow::bail!(
            "{} command failed: device reported it as {}",
            subject,
            if actual_enabled {
                "enabled"
            } else {
                "disabled"
            }
        );
    }

    Ok(())
}

fn ensure_command_succeeded(action: &str, succeeded: bool) -> anyhow::Result<()> {
    if !succeeded {
        anyhow::bail!("Device failed to {}", action);
    }

    Ok(())
}

fn ensure_mode_transition(
    requested_mode: SystemMode,
    current_mode: SystemMode,
    transition_ok: bool,
) -> anyhow::Result<()> {
    if !transition_ok {
        anyhow::bail!(
            "Mode transition to '{}' was rejected (current mode: '{}')",
            requested_mode,
            current_mode
        );
    }

    if current_mode != requested_mode {
        anyhow::bail!(
            "Mode transition reported success but device remained in '{}' (requested '{}')",
            current_mode,
            requested_mode
        );
    }

    Ok(())
}

fn ensure_self_test_passed(info: &crate::protocol::CliSelfTestInfo) -> anyhow::Result<()> {
    crate::protocol::validate_self_test_consistency(info)?;

    if info.tests_run == 0 {
        anyhow::bail!("On-device self-test did not run any tests");
    }

    let failed_results = info.results.iter().filter(|result| !result.passed).count();
    if info.tests_passed != info.tests_run || failed_results > 0 {
        anyhow::bail!(
            "On-device self-test failed: {}/{} passed, {} failing result(s)",
            info.tests_passed,
            info.tests_run,
            failed_results
        );
    }

    Ok(())
}

fn ensure_sim_mode_state(
    expected_enabled: bool,
    expected_delay_ms: u32,
    expected_pad_index: u32,
    state: &crate::protocol::CliSimModeState,
) -> anyhow::Result<()> {
    let expected = (expected_enabled, expected_delay_ms, expected_pad_index);
    let actual = (state.enabled, state.delay_ms, state.pad_index);
    if actual != expected {
        anyhow::bail!(
            "ESP-NOW sim mode response did not match request: expected enabled={}, delay_ms={}, pad={}, got enabled={}, delay_ms={}, pad={}",
            expected_enabled,
            expected_delay_ms,
            expected_pad_index,
            state.enabled,
            state.delay_ms,
            state.pad_index
        );
    }

    Ok(())
}

fn ensure_benchmark_complete(
    requested_rounds: u32,
    result: &crate::protocol::CliBenchResult,
) -> anyhow::Result<()> {
    if result.rounds_completed != requested_rounds || result.rounds_failed != 0 {
        anyhow::bail!(
            "ESP-NOW benchmark incomplete: {}/{} rounds completed, {} failed",
            result.rounds_completed,
            requested_rounds,
            result.rounds_failed
        );
    }

    Ok(())
}

const MIN_HEALTH_FREE_HEAP_BYTES: u32 = 16 * 1024;
const MIN_TASK_STACK_HEADROOM_BYTES: u32 = 256;

fn validate_system_health(health: &crate::protocol::CliHealthInfo) -> anyhow::Result<()> {
    let mut failures = Vec::new();

    if health.free_heap < MIN_HEALTH_FREE_HEAP_BYTES {
        failures.push(format!(
            "current internal free heap is {} bytes (minimum {})",
            health.free_heap, MIN_HEALTH_FREE_HEAP_BYTES
        ));
    }
    if health.min_free_heap < MIN_HEALTH_FREE_HEAP_BYTES {
        failures.push(format!(
            "historical internal free heap is {} bytes (minimum {})",
            health.min_free_heap, MIN_HEALTH_FREE_HEAP_BYTES
        ));
    }
    if health.min_free_heap > health.free_heap {
        failures.push(format!(
            "historical minimum heap {} exceeds current heap {}",
            health.min_free_heap, health.free_heap
        ));
    }

    if health.tasks.is_empty() {
        failures.push("task snapshot is empty".to_string());
    }
    for task in &health.tasks {
        if task.stack_high_water < MIN_TASK_STACK_HEADROOM_BYTES {
            failures.push(format!(
                "task '{}' has {} bytes of stack headroom (minimum {})",
                task.name, task.stack_high_water, MIN_TASK_STACK_HEADROOM_BYTES
            ));
        }
    }

    if failures.is_empty() {
        return Ok(());
    }

    anyhow::bail!("System health check failed: {}", failures.join("; "))
}

fn failed_devices(failures: &[String]) -> anyhow::Result<()> {
    if failures.is_empty() {
        return Ok(());
    }

    anyhow::bail!(
        "Failed on {} device(s): {}",
        failures.len(),
        failures.join(", ")
    )
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
        println!(
            "  Color:      #{:02x}{:02x}{:02x} (RGBW: {},{},{},{})",
            r, g, b, r, g, b, w
        );
    }

    if !pattern.colors.is_empty() {
        println!("  Colors:     {} colors in cycle", pattern.colors.len());
    }

    println!("  Period:     {} ms", pattern.period_ms);
    println!("  Brightness: {}", pattern.brightness);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn trace_stream_uses_its_subcommand_wifi_without_global_transport() {
        let cli = Cli::try_parse_from(["domes-cli", "trace", "stream", "--wifi", "192.168.1.100"])
            .unwrap();

        validate_trace_stream_selection(&cli).unwrap();
        assert!(matches!(
            cli.command,
            Some(Commands::Trace {
                action: TraceAction::Stream { wifi }
            }) if wifi == "192.168.1.100"
        ));
    }

    #[test]
    fn trace_stream_rejects_global_and_multi_device_selectors() {
        let cli = Cli::try_parse_from([
            "domes-cli",
            "--wifi",
            "192.168.1.100:5000",
            "trace",
            "stream",
            "--wifi",
            "192.168.1.100",
        ])
        .unwrap();

        let error = validate_trace_stream_selection(&cli)
            .unwrap_err()
            .to_string();
        assert!(error.contains("do not combine"));
    }

    #[test]
    fn raw_ota_flash_rejects_direct_wifi_before_transport_resolution() {
        let cli = Cli::try_parse_from([
            "domes-cli",
            "--wifi",
            "192.168.1.100:5000",
            "ota",
            "flash",
            "firmware.bin",
            "--version",
            "v1.2.3",
        ])
        .unwrap();

        let error = validate_ota_flash_transport_selection(&cli)
            .unwrap_err()
            .to_string();
        assert!(error.contains("not supported over WiFi/TCP"));
    }

    #[test]
    fn raw_ota_flash_keeps_serial_and_ble_targets_supported() {
        for args in [
            vec![
                "domes-cli",
                "--port",
                "/dev/ttyUSB0",
                "ota",
                "flash",
                "firmware.bin",
                "--version",
                "v1.2.3",
            ],
            vec![
                "domes-cli",
                "--ble",
                "DOMES-Pod",
                "ota",
                "flash",
                "firmware.bin",
                "--version",
                "v1.2.3",
            ],
        ] {
            let cli = Cli::try_parse_from(args).unwrap();
            validate_ota_flash_transport_selection(&cli).unwrap();
        }

        assert!(is_wifi_transport_type("wifi"));
        assert!(is_wifi_transport_type("tcp"));
        assert!(!is_wifi_transport_type("serial"));
        assert!(!is_wifi_transport_type("ble"));

        let wifi_check =
            Cli::try_parse_from(["domes-cli", "--wifi", "192.168.1.100:5000", "ota", "check"])
                .unwrap();
        validate_ota_flash_transport_selection(&wifi_check).unwrap();
    }

    #[test]
    fn raw_ota_flash_requires_declared_version() {
        let result = Cli::try_parse_from([
            "domes-cli",
            "--port",
            "/dev/ttyUSB0",
            "ota",
            "flash",
            "firmware.bin",
        ]);

        assert!(result.is_err());
    }

    #[test]
    fn trace_control_rejects_wifi_before_transport_resolution() {
        let cli = Cli::try_parse_from([
            "domes-cli",
            "--wifi",
            "192.168.1.100:5000",
            "trace",
            "status",
        ])
        .unwrap();

        let error = validate_trace_transport_selection(&cli)
            .unwrap_err()
            .to_string();
        assert!(error.contains("not supported over the WiFi/TCP config transport"));

        let serial =
            Cli::try_parse_from(["domes-cli", "--port", "/dev/null", "trace", "status"]).unwrap();
        validate_trace_transport_selection(&serial).unwrap();
    }

    #[test]
    fn registry_transport_preflight_finds_selected_wifi_targets() {
        let registry = std::collections::HashMap::from([
            (
                "serial-pod".to_string(),
                device::DeviceEntry {
                    transport_type: "serial".to_string(),
                    address: "/dev/null".to_string(),
                },
            ),
            (
                "wifi-pod".to_string(),
                device::DeviceEntry {
                    transport_type: "wifi".to_string(),
                    address: "127.0.0.1:5000".to_string(),
                },
            ),
        ]);
        let selected = Cli::try_parse_from([
            "domes-cli",
            "--target",
            "serial-pod",
            "--target",
            "wifi-pod",
            "trace",
            "status",
        ])
        .unwrap();
        assert_eq!(
            selected_wifi_registry_targets(&selected, &registry),
            vec!["wifi-pod"]
        );

        let all = Cli::try_parse_from(["domes-cli", "--all", "trace", "status"]).unwrap();
        assert_eq!(
            selected_wifi_registry_targets(&all, &registry),
            vec!["wifi-pod"]
        );
    }

    #[test]
    fn runtime_serial_discovery_excludes_console_and_legacy_ports() {
        assert!(is_runtime_serial_candidate("/dev/ttyUSB0"));
        assert!(is_runtime_serial_candidate(
            "/dev/serial/by-id/usb-Silicon_Labs_CP2102N-test"
        ));
        assert!(!is_runtime_serial_candidate("/dev/ttyACM0"));
        assert!(!is_runtime_serial_candidate("/dev/ttyS0"));
    }

    #[test]
    fn animated_led_period_must_be_nonzero() {
        assert!(Cli::try_parse_from([
            "domes-cli",
            "--port",
            "/dev/ttyUSB0",
            "led",
            "breathing",
            "--period",
            "0",
        ])
        .is_err());
        assert!(Cli::try_parse_from([
            "domes-cli",
            "--port",
            "/dev/ttyUSB0",
            "led",
            "cycle",
            "--period",
            "0",
        ])
        .is_err());
    }

    #[test]
    fn all_selector_conflicts_with_explicit_targets_and_transports() {
        for selector in ["--port", "--wifi", "--ble", "--target"] {
            assert!(Cli::try_parse_from([
                "domes-cli",
                "--all",
                selector,
                "target-value",
                "system",
                "info",
            ])
            .is_err());
        }

        assert!(Cli::try_parse_from(
            ["domes-cli", "--all", "--connect-all-ble", "system", "info",]
        )
        .is_err());
    }

    #[test]
    fn sniff_options_reject_ambiguous_or_empty_contracts() {
        for args in [
            vec!["domes-cli", "sniff", "--filter", ""],
            vec!["domes-cli", "sniff", "--filter", "config,unknown"],
            vec!["domes-cli", "sniff", "--filter", "config,,trace"],
            vec!["domes-cli", "sniff", "--raw", "--json"],
            vec!["domes-cli", "sniff", "--count", "0"],
        ] {
            assert!(Cli::try_parse_from(args).is_err());
        }

        Cli::try_parse_from([
            "domes-cli",
            "sniff",
            "--filter",
            "config,trace,ota",
            "--count",
            "1",
        ])
        .unwrap();
    }

    #[test]
    fn set_mode_rejects_device_managed_booting_mode() {
        assert!(Cli::try_parse_from([
            "domes-cli",
            "--port",
            "/dev/ttyUSB0",
            "system",
            "set-mode",
            "booting",
        ])
        .is_err());
    }

    #[test]
    fn pod_id_is_bounded_at_parse_time() {
        for id in ["1", "255"] {
            Cli::try_parse_from([
                "domes-cli",
                "--port",
                "/dev/ttyUSB0",
                "system",
                "set-pod-id",
                id,
            ])
            .unwrap();
        }

        for id in ["0", "256"] {
            assert!(Cli::try_parse_from([
                "domes-cli",
                "--port",
                "/dev/ttyUSB0",
                "system",
                "set-pod-id",
                id,
            ])
            .is_err());
        }
    }

    #[test]
    fn failed_device_summary_is_an_error() {
        let error = failed_devices(&["pod-2".to_string(), "pod-4".to_string()])
            .unwrap_err()
            .to_string();

        assert!(error.contains("Failed on 2 device(s): pod-2, pod-4"));
        failed_devices(&[]).unwrap();
    }

    #[test]
    fn rejected_or_inconsistent_mode_transition_is_an_error() {
        let rejected = ensure_mode_transition(SystemMode::Game, SystemMode::Idle, false)
            .unwrap_err()
            .to_string();
        assert!(rejected.contains("was rejected"));

        let inconsistent = ensure_mode_transition(SystemMode::Game, SystemMode::Idle, true)
            .unwrap_err()
            .to_string();
        assert!(inconsistent.contains("reported success"));

        ensure_mode_transition(SystemMode::Game, SystemMode::Game, true).unwrap();
    }

    #[test]
    fn failed_or_empty_self_test_is_an_error() {
        use crate::protocol::{CliSelfTestInfo, CliSelfTestResult};

        let failed = CliSelfTestInfo {
            tests_run: 2,
            tests_passed: 1,
            results: vec![
                CliSelfTestResult {
                    name: "heap".to_string(),
                    passed: true,
                    message: String::new(),
                },
                CliSelfTestResult {
                    name: "nvs".to_string(),
                    passed: false,
                    message: "write failed".to_string(),
                },
            ],
        };
        assert!(ensure_self_test_passed(&failed).is_err());

        let empty = CliSelfTestInfo {
            tests_run: 0,
            tests_passed: 0,
            results: Vec::new(),
        };
        assert!(ensure_self_test_passed(&empty).is_err());

        let passed = CliSelfTestInfo {
            tests_run: 1,
            tests_passed: 1,
            results: vec![CliSelfTestResult {
                name: "heap".to_string(),
                passed: true,
                message: String::new(),
            }],
        };
        ensure_self_test_passed(&passed).unwrap();

        let mismatched_counts = CliSelfTestInfo {
            tests_run: 2,
            tests_passed: 2,
            results: vec![CliSelfTestResult {
                name: "heap".to_string(),
                passed: true,
                message: String::new(),
            }],
        };
        assert!(ensure_self_test_passed(&mismatched_counts).is_err());
    }

    #[test]
    fn sim_mode_echo_must_match_every_requested_field() {
        let matching = crate::protocol::CliSimModeState {
            enabled: true,
            delay_ms: 500,
            pad_index: 2,
        };
        ensure_sim_mode_state(true, 500, 2, &matching).unwrap();

        let mismatched = crate::protocol::CliSimModeState {
            enabled: true,
            delay_ms: 501,
            pad_index: 2,
        };
        assert!(ensure_sim_mode_state(true, 500, 2, &mismatched).is_err());
    }

    #[test]
    fn partial_or_failed_benchmark_is_an_error() {
        let complete = crate::protocol::CliBenchResult {
            rounds_completed: 20,
            rounds_failed: 0,
            min_rtt_us: 1,
            max_rtt_us: 2,
            mean_rtt_us: 1,
            p50_rtt_us: 1,
            p95_rtt_us: 2,
            p99_rtt_us: 2,
        };
        ensure_benchmark_complete(20, &complete).unwrap();

        let mut failed = complete.clone();
        failed.rounds_completed = 19;
        failed.rounds_failed = 1;
        assert!(ensure_benchmark_complete(20, &failed).is_err());

        let mut short = complete;
        short.rounds_completed = 19;
        assert!(ensure_benchmark_complete(20, &short).is_err());
    }

    #[test]
    fn espnow_benchmark_rounds_are_bounded_at_parse_time() {
        for rounds in ["1", "1000"] {
            Cli::try_parse_from([
                "domes-cli",
                "--port",
                "/dev/ttyUSB0",
                "espnow",
                "bench",
                "--rounds",
                rounds,
            ])
            .unwrap();
        }

        for rounds in ["0", "1001"] {
            assert!(Cli::try_parse_from([
                "domes-cli",
                "--port",
                "/dev/ttyUSB0",
                "espnow",
                "bench",
                "--rounds",
                rounds,
            ])
            .is_err());
        }
    }

    #[test]
    fn system_health_rejects_low_heap_missing_tasks_and_low_stack() {
        use crate::protocol::{CliHealthInfo, CliTaskHealth};

        let low_heap = CliHealthInfo {
            free_heap: MIN_HEALTH_FREE_HEAP_BYTES - 1,
            min_free_heap: MIN_HEALTH_FREE_HEAP_BYTES - 1,
            uptime_seconds: 1,
            wifi_rssi: 0,
            tasks: vec![healthy_task("main")],
        };
        let error = validate_system_health(&low_heap).unwrap_err().to_string();
        assert!(error.contains("current internal free heap"));
        assert!(error.contains("historical internal free heap"));

        let no_tasks = CliHealthInfo {
            free_heap: MIN_HEALTH_FREE_HEAP_BYTES,
            min_free_heap: MIN_HEALTH_FREE_HEAP_BYTES,
            uptime_seconds: 1,
            wifi_rssi: 0,
            tasks: Vec::new(),
        };
        assert!(validate_system_health(&no_tasks)
            .unwrap_err()
            .to_string()
            .contains("task snapshot is empty"));

        let low_stack = CliHealthInfo {
            free_heap: MIN_HEALTH_FREE_HEAP_BYTES,
            min_free_heap: MIN_HEALTH_FREE_HEAP_BYTES,
            uptime_seconds: 1,
            wifi_rssi: 0,
            tasks: vec![CliTaskHealth {
                name: "mem_prof".to_string(),
                stack_high_water: MIN_TASK_STACK_HEADROOM_BYTES - 1,
                priority: 5,
                core: 0,
            }],
        };
        let error = validate_system_health(&low_stack).unwrap_err().to_string();
        assert!(error.contains("task 'mem_prof'"));
        assert!(error.contains("255 bytes of stack headroom"));
    }

    #[test]
    fn system_health_accepts_threshold_values() {
        let health = crate::protocol::CliHealthInfo {
            free_heap: MIN_HEALTH_FREE_HEAP_BYTES,
            min_free_heap: MIN_HEALTH_FREE_HEAP_BYTES,
            uptime_seconds: 1,
            wifi_rssi: 0,
            tasks: vec![healthy_task("main")],
        };

        validate_system_health(&health).unwrap();
    }

    fn healthy_task(name: &str) -> crate::protocol::CliTaskHealth {
        crate::protocol::CliTaskHealth {
            name: name.to_string(),
            stack_high_water: MIN_TASK_STACK_HEADROOM_BYTES,
            priority: 5,
            core: 0,
        }
    }
}
