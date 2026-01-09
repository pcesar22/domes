#!/usr/bin/env python3
"""
Mock GitHub Releases API server for OTA testing.

Usage:
    python server.py [--port 8080] [--firmware build/domes.bin] [--version v1.1.0]

This server simulates the GitHub Releases API endpoints:
- GET /repos/{owner}/{repo}/releases/latest
- GET /download/domes.bin (firmware download)

The device can be configured to use this server by calling:
    githubClient.setEndpoint("http://<host>:8080/repos/pcesar22/domes/releases/latest");
"""

import argparse
import hashlib
import json
import os
import sys
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse


def get_firmware_sha256(firmware_path: str) -> str:
    """Calculate SHA-256 hash of firmware binary."""
    if not os.path.exists(firmware_path):
        return "0" * 64

    with open(firmware_path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def get_firmware_size(firmware_path: str) -> int:
    """Get firmware file size in bytes."""
    if not os.path.exists(firmware_path):
        return 0
    return os.path.getsize(firmware_path)


class MockGithubHandler(BaseHTTPRequestHandler):
    """Handler for mock GitHub API requests."""

    firmware_path: str = "build/domes.bin"
    version: str = "v1.1.0"

    def log_message(self, format, *args):
        """Override to provide cleaner logging."""
        print(f"[{self.address_string()}] {format % args}")

    def do_GET(self):
        """Handle GET requests."""
        parsed = urlparse(self.path)
        path = parsed.path

        if "/releases/latest" in path:
            self.send_release_info()
        elif "/download/" in path:
            self.send_firmware()
        elif path == "/health":
            self.send_health()
        else:
            self.send_error(404, f"Not found: {path}")

    def send_release_info(self):
        """Send GitHub release JSON response."""
        host = self.headers.get("Host", "localhost:8080")
        firmware_size = get_firmware_size(self.firmware_path)
        firmware_sha256 = get_firmware_sha256(self.firmware_path)

        response = {
            "tag_name": self.version,
            "name": f"DOMES Firmware {self.version}",
            "body": f"## Changelog\n\n- New features\n- Bug fixes\n\nSHA-256: {firmware_sha256}",
            "draft": False,
            "prerelease": False,
            "assets": [
                {
                    "name": "domes.bin",
                    "browser_download_url": f"http://{host}/download/domes.bin",
                    "size": firmware_size,
                    "content_type": "application/octet-stream"
                }
            ]
        }

        body = json.dumps(response, indent=2).encode("utf-8")

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)

        print(f"Served release info: {self.version} ({firmware_size} bytes)")

    def send_firmware(self):
        """Send firmware binary."""
        if not os.path.exists(self.firmware_path):
            self.send_error(404, f"Firmware not found: {self.firmware_path}")
            return

        with open(self.firmware_path, "rb") as f:
            data = f.read()

        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", len(data))
        self.send_header("Content-Disposition", 'attachment; filename="domes.bin"')
        self.end_headers()
        self.wfile.write(data)

        print(f"Served firmware: {len(data)} bytes")

    def send_health(self):
        """Send health check response."""
        response = {"status": "ok", "version": self.version}
        body = json.dumps(response).encode("utf-8")

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body)


def main():
    parser = argparse.ArgumentParser(description="Mock GitHub Releases API server")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on")
    parser.add_argument("--firmware", default="build/domes.bin", help="Path to firmware binary")
    parser.add_argument("--version", default="v1.1.0", help="Version tag to report")
    parser.add_argument("--host", default="0.0.0.0", help="Host to bind to")
    args = parser.parse_args()

    # Configure handler
    MockGithubHandler.firmware_path = args.firmware
    MockGithubHandler.version = args.version

    # Print configuration
    print("=" * 60)
    print("Mock OTA Server")
    print("=" * 60)
    print(f"  Host:     {args.host}:{args.port}")
    print(f"  Firmware: {args.firmware}")
    print(f"  Version:  {args.version}")

    if os.path.exists(args.firmware):
        size = get_firmware_size(args.firmware)
        sha256 = get_firmware_sha256(args.firmware)
        print(f"  Size:     {size} bytes")
        print(f"  SHA-256:  {sha256[:16]}...")
    else:
        print(f"  WARNING: Firmware file not found!")

    print("=" * 60)
    print()
    print("Endpoints:")
    print(f"  GET http://{args.host}:{args.port}/repos/OWNER/REPO/releases/latest")
    print(f"  GET http://{args.host}:{args.port}/download/domes.bin")
    print(f"  GET http://{args.host}:{args.port}/health")
    print()
    print("Configure device with:")
    print(f'  githubClient.setEndpoint("http://<your-ip>:{args.port}/repos/pcesar22/domes/releases/latest");')
    print()
    print("Press Ctrl+C to stop")
    print()

    # Start server
    server = HTTPServer((args.host, args.port), MockGithubHandler)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        server.shutdown()


if __name__ == "__main__":
    main()
