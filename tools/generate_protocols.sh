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
cleanup() {
    if [[ -n "$tmp_dir" && -d "$tmp_dir" ]]; then
        rm -rf "$tmp_dir"
    fi
}
trap cleanup EXIT

if $check; then
    tmp_dir="$(mktemp -d)"
fi

generate_nanopb() {
    local output_dir="$proto_dir"
    if $check; then
        output_dir="$tmp_dir/nanopb"
        mkdir -p "$output_dir"
    fi

    (
        cd "$proto_dir"
        python3 "$nanopb_generator" -I . -D "$output_dir" config.proto trace.proto
    )

    # nanopb appends a schema-dependent number of blank lines. Normalize the
    # committed artifacts so protocol changes do not create trailing-whitespace
    # churn or fail `git diff --check`.
    for generated in config.pb.c config.pb.h trace.pb.c trace.pb.h; do
        python3 - "$output_dir/$generated" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
path.write_bytes(path.read_bytes().rstrip() + b"\n")
PY
    done

    if $check; then
        for generated in config.pb.c config.pb.h trace.pb.c trace.pb.h; do
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
        "$proto_dir/config.proto"

    if $check; then
        diff -ru "$dart_dir" "$output_dir"
    fi
}

case "$target" in
    all)
        generate_nanopb
        generate_dart
        ;;
    nanopb) generate_nanopb ;;
    dart) generate_dart ;;
esac

if $check; then
    echo "Protocol bindings are current ($target)."
else
    echo "Protocol bindings generated ($target)."
fi
