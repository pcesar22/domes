//! DOMES CLI - Runtime configuration tool for DOMES firmware
//!
//! Usage (Serial):
//!   domes-cli --port /dev/ttyACM0 feature list
//!   domes-cli --port /dev/ttyACM0 feature enable led-effects
//!   domes-cli --port /dev/ttyACM0 feature disable ble
//!   domes-cli --port /dev/ttyACM0 wifi enable
//!   domes-cli --port /dev/ttyACM0 wifi disable
//!   domes-cli --port /dev/ttyACM0 wifi status
//!
//! Usage (WiFi):
//!   domes-cli --wifi 192.168.1.100:5000 feature list
//!   domes-cli --wifi 192.168.1.100:5000 feature enable led-effects
//!   domes-cli --wifi 192.168.1.100:5000 wifi status

mod commands;
mod protocol;
mod transport;

use clap::{Parser, Subcommand};
use protocol::Feature;
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
                    println!("{:<16} {}", state.feature.name(), status);
                }
            }

            FeatureAction::Enable { feature } => {
                let feature: Feature = feature
                    .parse()
                    .map_err(|_| anyhow::anyhow!("Unknown feature: {}", feature))?;

                let state = commands::feature_enable(transport.as_mut(), feature)?;
                println!(
                    "Feature '{}' is now {}",
                    state.feature.name(),
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
                    state.feature.name(),
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
    }

    Ok(())
}
