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

mod commands;
mod proto;
mod protocol;
mod transport;

use clap::{Parser, Subcommand};
use proto::config::Feature;
use std::path::PathBuf;
use std::time::Duration;
use transport::{BleTarget, BleTransport, SerialTransport, TcpTransport, Transport};

#[derive(Parser)]
#[command(name = "domes-cli")]
#[command(version, about = "DOMES firmware runtime configuration CLI")]
struct Cli {
    /// Serial port to connect to (e.g., /dev/ttyACM0, COM3)
    #[arg(short, long)]
    port: Option<String>,

    /// WiFi address to connect to (e.g., 192.168.1.100:5000)
    #[arg(short, long)]
    wifi: Option<String>,

    /// BLE device name or address (e.g., "DOMES-Pod" or "AA:BB:CC:DD:EE:FF")
    #[arg(short, long)]
    ble: Option<String>,

    /// Scan for nearby BLE devices
    #[arg(long)]
    scan_ble: bool,

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

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();

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

    // Commands require a transport (port, wifi, or ble)
    let Some(command) = cli.command else {
        eprintln!("No command specified. Use --help for usage.");
        std::process::exit(1);
    };

    // Create transport based on connection type
    let mut transport: Box<dyn Transport> = if let Some(ble_target) = cli.ble {
        // BLE transport
        let target = BleTarget::parse(&ble_target);
        println!("Scanning for BLE device '{}'...", ble_target);
        let ble = BleTransport::connect(target, Duration::from_secs(10), true)?;
        println!("Connected to {} ({})", ble.device_name(), ble.device_address());
        Box::new(ble)
    } else if let Some(wifi_addr) = cli.wifi {
        // WiFi/TCP transport
        println!("Connecting to {} via WiFi...", wifi_addr);
        let tcp = TcpTransport::connect(&wifi_addr)?;
        println!("Connected to {}", tcp.peer_addr()?);
        Box::new(tcp)
    } else if let Some(port) = cli.port {
        // Serial transport
        Box::new(SerialTransport::open(&port)?)
    } else {
        eprintln!("No transport specified. Use --port <PORT>, --wifi <IP:PORT>, or --ble <NAME>");
        eprintln!("Use --list-ports to see available serial ports.");
        eprintln!("Use --scan-ble to scan for nearby BLE devices.");
        std::process::exit(1);
    };

    match command {
        Commands::Feature { action } => match action {
            FeatureAction::List => {
                let features = commands::feature_list(transport.as_mut())?;

                println!("Features:");
                println!("{:<16} {}", "NAME", "STATUS");
                println!("{:-<16} {:-<8}", "", "");

                for state in features {
                    let status = if state.enabled { "enabled" } else { "disabled" };
                    println!("{:<16} {}", state.feature.cli_name(), status);
                }
            }

            FeatureAction::Enable { feature } => {
                let feature: Feature = feature
                    .parse()
                    .map_err(|_| anyhow::anyhow!("Unknown feature: {}", feature))?;

                let state = commands::feature_enable(transport.as_mut(), feature)?;
                println!(
                    "Feature '{}' is now {}",
                    state.feature.cli_name(),
                    if state.enabled { "enabled" } else { "disabled" }
                );
            }

            FeatureAction::Disable { feature } => {
                let feature: Feature = feature
                    .parse()
                    .map_err(|_| anyhow::anyhow!("Unknown feature: {}", feature))?;

                let state = commands::feature_disable(transport.as_mut(), feature)?;
                println!(
                    "Feature '{}' is now {}",
                    state.feature.cli_name(),
                    if state.enabled { "enabled" } else { "disabled" }
                );
            }
        },

        Commands::Wifi { action } => match action {
            WifiAction::Enable => {
                let enabled = commands::wifi_enable(transport.as_mut())?;
                if enabled {
                    println!("WiFi subsystem enabled");
                } else {
                    println!("WiFi subsystem failed to enable");
                }
            }

            WifiAction::Disable => {
                let disabled = commands::wifi_disable(transport.as_mut())?;
                if disabled {
                    println!("WiFi subsystem disabled");
                } else {
                    println!("WiFi subsystem failed to disable");
                }
            }

            WifiAction::Status => {
                let enabled = commands::wifi_status(transport.as_mut())?;
                println!("WiFi subsystem: {}", if enabled { "enabled" } else { "disabled" });
            }
        },

        Commands::Led { action } => match action {
            LedAction::Get => {
                let pattern = commands::led_get(transport.as_mut())?;
                print_led_pattern(&pattern);
            }

            LedAction::Off => {
                let pattern = commands::led_off(transport.as_mut())?;
                println!("LEDs turned off");
                print_led_pattern(&pattern);
            }

            LedAction::Solid { color, brightness } => {
                let (r, g, b) = parse_hex_color(&color)?;
                let mut pattern = crate::protocol::CliLedPattern::solid(r, g, b);
                pattern.brightness = brightness;
                let pattern = commands::led_set(transport.as_mut(), &pattern)?;
                println!("LED pattern set to solid");
                print_led_pattern(&pattern);
            }

            LedAction::Breathing { color, period, brightness } => {
                let (r, g, b) = parse_hex_color(&color)?;
                let mut pattern = crate::protocol::CliLedPattern::breathing(r, g, b, period);
                pattern.brightness = brightness;
                let pattern = commands::led_set(transport.as_mut(), &pattern)?;
                println!("LED pattern set to breathing");
                print_led_pattern(&pattern);
            }

            LedAction::Cycle { period, brightness } => {
                // Default rainbow colors for color cycle
                let colors = vec![
                    (255, 0, 0, 0),     // Red
                    (255, 127, 0, 0),   // Orange
                    (255, 255, 0, 0),   // Yellow
                    (0, 255, 0, 0),     // Green
                    (0, 0, 255, 0),     // Blue
                    (75, 0, 130, 0),    // Indigo
                    (148, 0, 211, 0),   // Violet
                ];
                let mut pattern = crate::protocol::CliLedPattern::color_cycle(colors, period);
                pattern.brightness = brightness;
                let pattern = commands::led_set(transport.as_mut(), &pattern)?;
                println!("LED pattern set to color cycle");
                print_led_pattern(&pattern);
            }
        },

        Commands::Ota { action } => match action {
            OtaAction::Flash { firmware, version } => {
                commands::ota_flash(
                    transport.as_mut(),
                    &firmware,
                    version.as_deref(),
                )?;
            }
        },

        Commands::Trace { action } => match action {
            TraceAction::Start => {
                commands::trace_start(transport.as_mut())?;
                println!("Tracing started");
            }

            TraceAction::Stop => {
                commands::trace_stop(transport.as_mut())?;
                println!("Tracing stopped");
            }

            TraceAction::Clear => {
                commands::trace_clear(transport.as_mut())?;
                println!("Trace buffer cleared");
            }

            TraceAction::Status => {
                let status = commands::trace_status(transport.as_mut())?;
                println!("Trace status:");
                println!("  Initialized: {}", status.initialized);
                println!("  Enabled:     {}", status.enabled);
                println!("  Events:      {}", status.event_count);
                println!("  Dropped:     {}", status.dropped_count);
                println!("  Buffer size: {} bytes", status.buffer_size);
            }

            TraceAction::Dump { output } => {
                println!("Dumping traces to {}...", output.display());
                let result = commands::trace_dump(transport.as_mut(), &output)?;
                println!("Dump complete:");
                println!("  Events:   {}", result.event_count);
                println!("  Dropped:  {}", result.dropped_count);
                println!("  Duration: {} us ({:.2} ms)", result.duration_us, result.duration_us as f64 / 1000.0);
                println!("  Output:   {}", result.output_path.display());
                println!("\nOpen trace in https://ui.perfetto.dev");
            }
        },

        Commands::Imu { action } => match action {
            ImuAction::Triage { enable, disable } => {
                let enabled = if enable && disable {
                    anyhow::bail!("Cannot specify both --enable and --disable");
                } else if enable {
                    true
                } else if disable {
                    false
                } else {
                    anyhow::bail!("Must specify either --enable or --disable");
                };

                let result = commands::imu_triage_set(transport.as_mut(), enabled)?;
                println!(
                    "IMU triage mode {}",
                    if result { "enabled" } else { "disabled" }
                );
            }
        },
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
