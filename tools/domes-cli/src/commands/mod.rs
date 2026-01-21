//! CLI commands for DOMES CLI

pub mod feature;
pub mod led;
pub mod ota;
pub mod trace;
pub mod wifi;

pub use feature::{feature_disable, feature_enable, feature_list};
pub use led::{led_get, led_off, led_set};
pub use ota::ota_flash;
pub use trace::{trace_clear, trace_dump, trace_start, trace_status, trace_stop};
pub use wifi::{wifi_disable, wifi_enable, wifi_status};
