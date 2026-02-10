#!/bin/bash
# Generate Dart protobuf code from config.proto
# Same source of truth as firmware (nanopb) and CLI (prost)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$PROJECT_DIR/lib/data/proto/generated"

# Find proto dir: try relative path first, then git repo root, then env override
if [ -n "${PROTO_DIR:-}" ]; then
  : # Use env var
elif [ -d "$PROJECT_DIR/../../../firmware/common/proto" ]; then
  PROTO_DIR="$(cd "$PROJECT_DIR/../../../firmware/common/proto" && pwd)"
else
  # We might be in a worktree -- find the main repo root via git
  REPO_ROOT="$(git -C "$PROJECT_DIR" rev-parse --show-toplevel 2>/dev/null || true)"
  if [ -n "$REPO_ROOT" ] && [ -d "$REPO_ROOT/firmware/common/proto" ]; then
    PROTO_DIR="$REPO_ROOT/firmware/common/proto"
  else
    # Last resort: look relative to the common worktree path
    MAIN_REPO="$(cd "$PROJECT_DIR/../../../../.." && pwd)"
    if [ -d "$MAIN_REPO/firmware/common/proto" ]; then
      PROTO_DIR="$MAIN_REPO/firmware/common/proto"
    else
      echo "ERROR: Cannot find firmware/common/proto directory"
      echo "Set PROTO_DIR env var to the proto directory"
      exit 1
    fi
  fi
fi

echo "Proto source: $PROTO_DIR/config.proto"
echo "Output dir:   $OUT_DIR"

mkdir -p "$OUT_DIR"

protoc \
  --proto_path="$PROTO_DIR" \
  --dart_out="$OUT_DIR" \
  "$PROTO_DIR/config.proto"

echo "Generated Dart protobuf files in $OUT_DIR"
ls -la "$OUT_DIR"
