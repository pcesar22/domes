//! DOMES CLI - Runtime configuration tool for DOMES firmware
//!
//! Usage:
//!   domes-cli --port /dev/ttyACM0 feature list
//!   domes-cli --port /dev/ttyACM0 feature enable led-effects
//!   domes-cli --port /dev/ttyACM0 feature disable ble

mod commands;
mod protocol;
mod transport;

use clap::{Parser, Subcommand};
use protocol::Feature;
use transport::SerialTransport;

#[derive(Parser)]
#[command(name = "domes-cli")]
#[command(version, about = "DOMES firmware runtime configuration CLI")]
struct Cli {
    /// Serial port to connect to (e.g., /dev/ttyACM0, COM3)
    #[arg(short, long)]
    port: Option<String>,

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

    // Commands require a port
    let Some(command) = cli.command else {
        eprintln!("No command specified. Use --help for usage.");
        std::process::exit(1);
    };

    let Some(port) = cli.port else {
        eprintln!("No port specified. Use --port <PORT> or --list-ports to see available ports.");
        std::process::exit(1);
    };

    // Open serial connection
    let mut transport = SerialTransport::open(&port)?;

    match command {
        Commands::Feature { action } => match action {
            FeatureAction::List => {
                let features = commands::feature_list(&mut transport)?;

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

                let state = commands::feature_enable(&mut transport, feature)?;
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

                let state = commands::feature_disable(&mut transport, feature)?;
                println!(
                    "Feature '{}' is now {}",
                    state.feature.name(),
                    if state.enabled { "enabled" } else { "disabled" }
                );
            }
        },
    }

    Ok(())
}
