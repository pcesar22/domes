#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: tools/generate_protocols.sh [--check] [all|nanopb|dart]

Generate committed protocol bindings from firmware/common/proto.

  --check  Generate into a temporary directory and fail on drift.
  all      Check or generate nanopb and Dart bindings (default).
  nanopb   Check or generate firmware C bindings only.
  dart     Check or generate Flutter Dart bindings only.

Rust prost bindings are generated in Cargo's build directory by tools/domes-cli/build.rs.
EOF
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
proto_dir="$repo_root/firmware/common/proto"
nanopb_generator="$repo_root/firmware/third_party/nanopb/generator/nanopb_generator.py"
dart_dir="$repo_root/ios/domes_app/lib/data/proto/generated"
check=false
target=all

for arg in "$@"; do
    case "$arg" in
        --check) check=true ;;
        all|nanopb|dart) target="$arg" ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

tmp_dir=""
descriptor_file=""
cleanup() {
    if [[ -n "$tmp_dir" && -d "$tmp_dir" ]]; then
        rm -rf "$tmp_dir"
    fi
    if [[ -n "$descriptor_file" && -f "$descriptor_file" ]]; then
        rm -f "$descriptor_file"
    fi
}
trap cleanup EXIT

if $check; then
    tmp_dir="$(mktemp -d)"
fi

validate_peer_drill_descriptor() {
    local -a descriptor_protoc=(protoc)
    if ! command -v protoc >/dev/null 2>&1; then
        if ! python3 -c 'import grpc_tools.protoc' >/dev/null 2>&1; then
            echo "protoc is missing and Python grpc_tools.protoc is unavailable" >&2
            return 1
        fi
        descriptor_protoc=(python3 -m grpc_tools.protoc)
    fi
    descriptor_file="$(mktemp)"
    "${descriptor_protoc[@]}" \
        --proto_path="$proto_dir" \
        --descriptor_set_out="$descriptor_file" \
        "$proto_dir/peer_drill.proto"
    python3 - "$descriptor_file" <<'PY'
from pathlib import Path
import sys

from google.protobuf import descriptor_pb2

descriptor_set = descriptor_pb2.FileDescriptorSet()
descriptor_set.ParseFromString(Path(sys.argv[1]).read_bytes())
peer_file = next(file for file in descriptor_set.file if file.name == "peer_drill.proto")
peer_message = next(message for message in peer_file.message_type if message.name == "PeerMessage")
payload = next(oneof for oneof in peer_message.oneof_decl if oneof.name == "payload")
payload_index = list(peer_message.oneof_decl).index(payload)
actual_payload = {
    field.name: field.number
    for field in peer_message.field
    if field.HasField("oneof_index") and field.oneof_index == payload_index
}
expected_payload = {
    "beacon": 0x01,
    "ping": 0x02,
    "pong": 0x03,
    "join_game": 0x10,
    "arm_touch": 0x11,
    "set_color": 0x12,
    "stop_all": 0x13,
    "simulate_touch": 0x14,
    "touch_event": 0x20,
    "timeout_event": 0x21,
}
if actual_payload != expected_payload:
    raise SystemExit(
        f"peer_drill payload tags must equal the complete Legacy-V1 type map: {actual_payload}"
    )

actual_metadata = {
    field.name: field.number
    for field in peer_message.field
    if not field.HasField("oneof_index")
}
expected_metadata = {"protocol_version": 256, "sender_mac": 257, "timestamp_us": 258}
if actual_metadata != expected_metadata:
    raise SystemExit(f"peer_drill metadata tags changed unexpectedly: {actual_metadata}")

peer_role = next(enum for enum in peer_file.enum_type if enum.name == "PeerRole")
actual_roles = {value.name: value.number for value in peer_role.value}
expected_roles = {
    "PEER_ROLE_UNSPECIFIED": 0,
    "PEER_ROLE_MASTER": 1,
    "PEER_ROLE_SLAVE": 2,
}
if actual_roles != expected_roles:
    raise SystemExit(f"peer_drill role values changed unexpectedly: {actual_roles}")
PY
}

generate_nanopb() {
    local output_dir="$proto_dir"
    if $check; then
        output_dir="$tmp_dir/nanopb"
        mkdir -p "$output_dir"
    fi

    (
        cd "$proto_dir"
        python3 "$nanopb_generator" -I . -D "$output_dir" \
            config.proto peer_drill.proto trace.proto
    )

    # nanopb appends a schema-dependent number of blank lines. Normalize the
    # committed artifacts so protocol changes do not create trailing-whitespace
    # churn or fail `git diff --check`.
    for generated in \
        config.pb.c config.pb.h \
        peer_drill.pb.c peer_drill.pb.h \
        trace.pb.c trace.pb.h; do
        python3 - "$output_dir/$generated" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
path.write_bytes(path.read_bytes().rstrip() + b"\n")
PY
    done

    if $check; then
        for generated in \
            config.pb.c config.pb.h \
            peer_drill.pb.c peer_drill.pb.h \
            trace.pb.c trace.pb.h; do
            diff -u "$proto_dir/$generated" "$output_dir/$generated"
        done
    fi
}

find_dart_plugin() {
    if command -v protoc-gen-dart >/dev/null 2>&1; then
        command -v protoc-gen-dart
    elif [[ -x "$HOME/.pub-cache/bin/protoc-gen-dart" ]]; then
        printf '%s\n' "$HOME/.pub-cache/bin/protoc-gen-dart"
    else
        echo "protoc-gen-dart is missing; run: dart pub global activate protoc_plugin 25.0.0" >&2
        return 1
    fi
}

generate_dart() {
    local output_dir="$dart_dir"
    local plugin
    plugin="$(find_dart_plugin)"
    if $check; then
        output_dir="$tmp_dir/dart"
        mkdir -p "$output_dir"
    fi

    protoc \
        --plugin="protoc-gen-dart=$plugin" \
        --proto_path="$proto_dir" \
        --dart_out="$output_dir" \
        "$proto_dir/config.proto" \
        "$proto_dir/peer_drill.proto"

    if $check; then
        diff -ru "$dart_dir" "$output_dir"
    fi
}

case "$target" in
    all)
        validate_peer_drill_descriptor
        generate_nanopb
        generate_dart
        ;;
    nanopb)
        validate_peer_drill_descriptor
        generate_nanopb
        ;;
    dart)
        validate_peer_drill_descriptor
        generate_dart
        ;;
esac

if $check; then
    echo "Protocol bindings are current ($target)."
else
    echo "Protocol bindings generated ($target)."
fi
