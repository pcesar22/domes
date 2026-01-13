//! DOMES CLI - Runtime configuration tool for DOMES firmware
//!
//! Usage (Serial):
//!   domes-cli --port /dev/ttyACM0 feature list
//!   domes-cli --port /dev/ttyACM0 feature enable led-effects
//!   domes-cli --port /dev/ttyACM0 feature disable ble
//!   domes-cli --port /dev/ttyACM0 wifi enable
//!   domes-cli --port /dev/ttyACM0 wifi disable
//!   domes-cli --port /dev/ttyACM0 wifi status
//!   domes-cli --port /dev/ttyACM0 ota flash firmware.bin --version v1.2.3
//!
//! Usage (WiFi):
//!   domes-cli --wifi 192.168.1.100:5000 feature list
//!   domes-cli --wifi 192.168.1.100:5000 feature enable led-effects
//!   domes-cli --wifi 192.168.1.100:5000 wifi status
//!   domes-cli --wifi 192.168.1.100:5000 ota flash firmware.bin

mod commands;
mod proto;
mod protocol;
mod transport;

use clap::{Parser, Subcommand};
use proto::config::{Feature, RgbPattern};
use std::path::PathBuf;
use transport::{SerialTransport, TcpTransport, Transport};

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

    /// Over-the-air firmware updates
    Ota {
        #[command(subcommand)]
        action: OtaAction,
    },

    /// Control RGB LED patterns
    Rgb {
        #[command(subcommand)]
        action: RgbAction,
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
enum RgbAction {
    /// List all available RGB patterns
    List,

    /// Get the current RGB pattern state
    Status,

    /// Turn off all LEDs
    Off,

    /// Set a specific pattern
    Set {
        /// Pattern name (off, solid, rainbow-chase, comet-tail, sparkle-fire)
        pattern: String,

        /// Primary color as hex (e.g., ff0000 for red) or name (red, green, blue, etc.)
        #[arg(short, long)]
        color: Option<String>,

        /// Animation speed in milliseconds (lower = faster)
        #[arg(short, long)]
        speed: Option<u32>,

        /// Brightness (0-255)
        #[arg(short, long)]
        brightness: Option<u8>,
    },

    /// Set rainbow chase pattern
    Rainbow {
        /// Animation speed in milliseconds
        #[arg(short, long, default_value = "50")]
        speed: u32,

        /// Brightness (0-255)
        #[arg(short, long, default_value = "128")]
        brightness: u8,
    },

    /// Set comet tail pattern
    Comet {
        /// Comet color as hex or name
        #[arg(short, long, default_value = "00ffff")]
        color: String,

        /// Animation speed in milliseconds
        #[arg(short, long, default_value = "30")]
        speed: u32,

        /// Brightness (0-255)
        #[arg(short, long, default_value = "200")]
        brightness: u8,
    },

    /// Set sparkle fire pattern
    Fire {
        /// Animation speed in milliseconds
        #[arg(short, long, default_value = "20")]
        speed: u32,

        /// Brightness (0-255)
        #[arg(short, long, default_value = "200")]
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

    // Commands require a transport (port or wifi)
    let Some(command) = cli.command else {
        eprintln!("No command specified. Use --help for usage.");
        std::process::exit(1);
    };

    // Create transport based on connection type
    let mut transport: Box<dyn Transport> = if let Some(wifi_addr) = cli.wifi {
        // WiFi/TCP transport
        println!("Connecting to {} via WiFi...", wifi_addr);
        let tcp = TcpTransport::connect(&wifi_addr)?;
        println!("Connected to {}", tcp.peer_addr()?);
        Box::new(tcp)
    } else if let Some(port) = cli.port {
        // Serial transport
        Box::new(SerialTransport::open(&port)?)
    } else {
        eprintln!("No transport specified. Use --port <PORT> or --wifi <IP:PORT>");
        eprintln!("Use --list-ports to see available serial ports.");
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

        Commands::Ota { action } => match action {
            OtaAction::Flash { firmware, version } => {
                commands::ota_flash(
                    transport.as_mut(),
                    &firmware,
                    version.as_deref(),
                )?;
            }
        },

        Commands::Rgb { action } => match action {
            RgbAction::List => {
                let patterns = commands::rgb_list(transport.as_mut())?;

                println!("Available RGB patterns:");
                println!("{:<16} {}", "NAME", "DESCRIPTION");
                println!("{:-<16} {:-<50}", "", "");

                for info in patterns {
                    println!("{:<16} {}", info.name, info.description);
                }
            }

            RgbAction::Status => {
                let state = commands::rgb_get(transport.as_mut())?;
                let (r, g, b) = state.primary_color;

                println!("RGB LED Status:");
                println!("  Pattern:    {}", state.pattern.cli_name());
                println!("  Color:      #{:02x}{:02x}{:02x}", r, g, b);
                println!("  Speed:      {} ms", state.speed_ms);
                println!("  Brightness: {}", state.brightness);
            }

            RgbAction::Off => {
                commands::rgb_off(transport.as_mut())?;
                println!("RGB LEDs turned off");
            }

            RgbAction::Set {
                pattern,
                color,
                speed,
                brightness,
            } => {
                let pattern: RgbPattern = pattern
                    .parse()
                    .map_err(|_| anyhow::anyhow!("Unknown pattern: {}", pattern))?;

                let color = color.map(|c| parse_color(&c)).transpose()?;
                let active = commands::rgb_set(
                    transport.as_mut(),
                    pattern,
                    color,
                    speed,
                    brightness,
                )?;
                println!("RGB pattern set to: {}", active.cli_name());
            }

            RgbAction::Rainbow { speed, brightness } => {
                let active = commands::rgb_set(
                    transport.as_mut(),
                    RgbPattern::RainbowChase,
                    None,
                    Some(speed),
                    Some(brightness),
                )?;
                println!("RGB pattern set to: {}", active.cli_name());
            }

            RgbAction::Comet {
                color,
                speed,
                brightness,
            } => {
                let color = parse_color(&color)?;
                let active = commands::rgb_set(
                    transport.as_mut(),
                    RgbPattern::CometTail,
                    Some(color),
                    Some(speed),
                    Some(brightness),
                )?;
                println!("RGB pattern set to: {}", active.cli_name());
            }

            RgbAction::Fire { speed, brightness } => {
                let active = commands::rgb_set(
                    transport.as_mut(),
                    RgbPattern::SparkleFire,
                    None,
                    Some(speed),
                    Some(brightness),
                )?;
                println!("RGB pattern set to: {}", active.cli_name());
            }
        },
    }

    Ok(())
}

/// Parse a color from hex string or color name
fn parse_color(s: &str) -> anyhow::Result<(u8, u8, u8)> {
    // Try named colors first
    match s.to_lowercase().as_str() {
        "red" => return Ok((255, 0, 0)),
        "green" => return Ok((0, 255, 0)),
        "blue" => return Ok((0, 0, 255)),
        "white" => return Ok((255, 255, 255)),
        "yellow" => return Ok((255, 255, 0)),
        "cyan" => return Ok((0, 255, 255)),
        "magenta" | "purple" => return Ok((255, 0, 255)),
        "orange" => return Ok((255, 128, 0)),
        "pink" => return Ok((255, 105, 180)),
        _ => {}
    }

    // Try hex parsing (with or without # prefix)
    let hex = s.trim_start_matches('#');
    if hex.len() == 6 {
        let r = u8::from_str_radix(&hex[0..2], 16)?;
        let g = u8::from_str_radix(&hex[2..4], 16)?;
        let b = u8::from_str_radix(&hex[4..6], 16)?;
        return Ok((r, g, b));
    }

    anyhow::bail!(
        "Invalid color: {}. Use hex (e.g., ff0000) or name (red, green, blue, etc.)",
        s
    )
}
