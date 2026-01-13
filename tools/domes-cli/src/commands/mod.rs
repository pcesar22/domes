//! CLI commands for DOMES CLI

pub mod feature;
pub mod wifi;

pub use feature::{feature_disable, feature_enable, feature_list};
pub use wifi::{wifi_disable, wifi_enable, wifi_status};
