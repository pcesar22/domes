//! Transport layer for DOMES CLI
//!
//! Provides frame encoding/decoding and serial port communication.

pub mod frame;
pub mod serial;

pub use frame::{encode_frame, Frame, FrameDecoder, FrameError};
pub use serial::SerialTransport;
