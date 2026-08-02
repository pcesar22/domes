use assert_cmd::cargo::cargo_bin_cmd;
use crc32fast::Hasher;
use predicates::prelude::*;
use std::fs;
use std::io::{Read, Write};
use std::net::TcpListener;
use std::thread;

#[test]
fn unsupported_trace_transport_exits_nonzero_before_connecting() {
    let mut command = cargo_bin_cmd!("domes-cli");
    command.args(["--wifi", "127.0.0.1:1", "trace", "status"]);

    command.assert().failure().stderr(predicate::str::contains(
        "not supported over the WiFi/TCP config transport",
    ));
}

#[test]
fn rejected_mode_transition_exits_nonzero() {
    // Outer status OK, current mode IDLE, transition_ok false (omitted/default).
    let (address, server) = serve_config_response(0x32, 0x33, &[0x00, 0x08, 0x01]);
    let mut command = cargo_bin_cmd!("domes-cli");
    command.args(["--wifi", &address, "system", "set-mode", "game"]);

    let output = command.output().unwrap();
    server.join().unwrap();

    assert!(!output.status.success());
    assert!(
        String::from_utf8_lossy(&output.stderr).contains("Mode transition to 'game' was rejected")
    );
}

#[test]
fn feature_status_uses_get_feature_contract() {
    // Outer status OK and GetFeatureResponse{FeatureState{wifi, enabled}}.
    let response_payload = [0x00, 0x0A, 0x04, 0x08, 0x03, 0x10, 0x01];
    let (address, server) = serve_config_response(0x24, 0x25, &response_payload);
    let mut command = cargo_bin_cmd!("domes-cli");
    command.args(["--wifi", &address, "feature", "status", "wifi"]);

    let output = command.output().unwrap();
    server.join().unwrap();

    assert!(output.status.success());
    assert!(String::from_utf8_lossy(&output.stdout).contains("Feature 'wifi' is enabled"));
}

#[test]
fn failed_self_test_exits_nonzero() {
    // Outer status OK, tests_run=1, tests_passed=0, one failed result.
    let (address, server) = serve_config_response(0x44, 0x45, &[0x00, 0x08, 0x01, 0x1A, 0x00]);
    let mut command = cargo_bin_cmd!("domes-cli");
    command.args(["--wifi", &address, "system", "self-test"]);

    let output = command.output().unwrap();
    server.join().unwrap();

    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr).contains("On-device self-test failed"));
}

#[test]
fn mismatched_set_pod_id_response_exits_nonzero() {
    // Outer status OK, but the response echoes pod ID 2 for a request to set 1.
    let (address, server) = serve_config_response(0x36, 0x37, &[0x00, 0x08, 0x02]);
    let mut command = cargo_bin_cmd!("domes-cli");
    command.args(["--wifi", &address, "system", "set-pod-id", "1"]);

    let output = command.output().unwrap();
    server.join().unwrap();

    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stderr)
        .contains("Set pod ID response did not match request: expected 1, got 2"));
}

#[test]
fn removing_an_unknown_registry_device_exits_nonzero() {
    let home = unique_temp_dir("remove-missing");
    fs::create_dir_all(&home).unwrap();

    let mut command = cargo_bin_cmd!("domes-cli");
    command
        .env("HOME", &home)
        .args(["devices", "remove", "missing"]);
    command
        .assert()
        .failure()
        .stderr(predicate::str::contains("Device 'missing' not found"));

    fs::remove_dir_all(home).unwrap();
}

#[test]
fn multi_device_memory_json_is_one_valid_document() {
    let (address1, server1) = serve_config_response(
        0x42,
        0x43,
        &[0x00, 0x08, 0x64, 0x10, 0x32, 0x18, 0x28, 0x20, 0xC8, 0x01],
    );
    let (address2, server2) = serve_config_response(
        0x42,
        0x43,
        &[0x00, 0x08, 0x50, 0x10, 0x28, 0x18, 0x20, 0x20, 0xA0, 0x01],
    );
    let mut command = cargo_bin_cmd!("domes-cli");
    command.args([
        "--wifi", &address1, "--wifi", &address2, "system", "memory", "--json",
    ]);

    let output = command.output().unwrap();
    server1.join().unwrap();
    server2.join().unwrap();

    assert!(output.status.success());
    let document: serde_json::Value = serde_json::from_slice(&output.stdout).unwrap();
    assert_eq!(document["devices"]["wifi-0"]["current_free_heap"], 100);
    assert_eq!(document["devices"]["wifi-1"]["current_free_heap"], 80);
}

#[test]
fn unsafe_system_health_exits_nonzero() {
    // Outer status OK; current and historical internal heaps are both 8192
    // bytes, and the task snapshot is empty.
    let health_payload = [0x00, 0x08, 0x80, 0x40, 0x10, 0x80, 0x40];
    let (address, server) = serve_config_response(0x38, 0x39, &health_payload);
    let mut command = cargo_bin_cmd!("domes-cli");
    command.args(["--wifi", &address, "system", "health"]);

    let output = command.output().unwrap();
    server.join().unwrap();

    assert!(!output.status.success());
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(stderr.contains("System health check failed"));
    assert!(stderr.contains("current internal free heap is 8192 bytes"));
    assert!(stderr.contains("task snapshot is empty"));
}

#[test]
fn connection_failure_does_not_block_healthy_target() {
    // Outer status OK, current mode IDLE, no time-in-mode value.
    let (address, server) = serve_config_response(0x30, 0x31, &[0x00, 0x08, 0x01]);
    let home = unique_temp_dir("multi-target");
    let registry_dir = home.join(".domes");
    fs::create_dir_all(&registry_dir).unwrap();
    fs::write(
        registry_dir.join("devices.toml"),
        format!(
            "[devices.offline]\ntransport = \"wifi\"\naddress = \"127.0.0.1:1\"\n\n[devices.healthy]\ntransport = \"wifi\"\naddress = \"{address}\"\n"
        ),
    )
    .unwrap();

    let mut command = cargo_bin_cmd!("domes-cli");
    command.env("HOME", &home).args([
        "--target", "offline", "--target", "healthy", "system", "mode",
    ]);

    let output = command.output().unwrap();
    server.join().unwrap();
    fs::remove_dir_all(home).unwrap();

    assert!(!output.status.success());
    assert!(String::from_utf8_lossy(&output.stdout).contains("[healthy] System mode: idle"));
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(stderr.contains("[offline] Connection error"));
    assert!(stderr.contains("Failed on 1 device(s): offline"));
}

fn serve_config_response(
    expected_request_type: u8,
    response_type: u8,
    response_payload: &[u8],
) -> (String, thread::JoinHandle<()>) {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = listener.local_addr().unwrap().to_string();
    let response = encode_frame(response_type, response_payload);

    let server = thread::spawn(move || {
        let (mut stream, _) = listener.accept().unwrap();
        let mut request = [0u8; 2048];
        let request_len = stream.read(&mut request).unwrap();
        assert!(request_len >= 5);
        assert_eq!(request[4], expected_request_type);
        stream.write_all(&response).unwrap();
    });

    (address, server)
}

fn encode_frame(msg_type: u8, payload: &[u8]) -> Vec<u8> {
    let mut hasher = Hasher::new();
    hasher.update(&[msg_type]);
    hasher.update(payload);
    let crc = hasher.finalize();

    let mut frame = vec![0xAA, 0x55];
    let wire_length = u16::try_from(payload.len() + 1).unwrap();
    frame.extend_from_slice(&wire_length.to_le_bytes());
    frame.push(msg_type);
    frame.extend_from_slice(payload);
    frame.extend_from_slice(&crc.to_le_bytes());
    frame
}

fn unique_temp_dir(label: &str) -> std::path::PathBuf {
    let nonce = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    std::env::temp_dir().join(format!("domes-cli-{label}-{}-{nonce}", std::process::id()))
}
