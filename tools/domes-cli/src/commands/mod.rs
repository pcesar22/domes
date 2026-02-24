//! CLI commands for DOMES CLI

pub mod espnow;
pub mod feature;
pub mod health;
pub mod imu;
pub mod led;
pub mod ota;
pub mod system;
pub mod trace;
pub mod wifi;

pub use espnow::{espnow_bench, espnow_status};
pub use feature::{feature_disable, feature_enable, feature_list};
pub use health::system_health;
pub use imu::imu_triage_set;
pub use led::{led_get, led_off, led_set};
pub use ota::ota_flash;
pub use system::{system_get_mode, system_info, system_set_mode, system_set_pod_id};
pub use trace::{trace_clear, trace_dump, trace_start, trace_status, trace_stop};
pub use wifi::{wifi_disable, wifi_enable, wifi_status};
