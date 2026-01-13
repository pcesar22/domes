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
    let trace_proto = proto_dir.join("trace.proto");

    // Tell Cargo to rerun this if any proto file changes
    println!("cargo:rerun-if-changed={}", config_proto.display());
    println!("cargo:rerun-if-changed={}", trace_proto.display());

    // Generate Rust code from protos
    prost_build::Config::new()
        // Use BTreeMap for deterministic output
        .btree_map(["."])
        // Compile all proto files
        .compile_protos(&[&config_proto, &trace_proto], &[&proto_dir])?;

    Ok(())
}
