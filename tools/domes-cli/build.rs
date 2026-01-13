//! Build script for domes-cli
//!
//! Generates Rust code from Protocol Buffer definitions.
//! The proto files are the SINGLE SOURCE OF TRUTH for protocol definitions.
//! DO NOT hand-roll protocol types - they are generated here.

use std::path::PathBuf;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Path to proto files (relative to this crate's Cargo.toml)
    let proto_dir = PathBuf::from("../../firmware/common/proto");
    let config_proto = proto_dir.join("config.proto");

    // Tell Cargo to rerun this if the proto file changes
    println!("cargo:rerun-if-changed={}", config_proto.display());

    // Generate Rust code from proto
    prost_build::Config::new()
        // Use BTreeMap for deterministic output
        .btree_map(["."])
        // Compile the proto file
        .compile_protos(&[&config_proto], &[&proto_dir])?;

    Ok(())
}
