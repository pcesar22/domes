//! CLI commands for DOMES CLI

pub mod feature;
pub mod ota;
pub mod rgb;
pub mod wifi;

pub use feature::{feature_disable, feature_enable, feature_list};
pub use ota::ota_flash;
pub use rgb::{rgb_get, rgb_list, rgb_off, rgb_set};
pub use wifi::{wifi_disable, wifi_enable, wifi_status};
