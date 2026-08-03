#!/usr/bin/env bash

set -euo pipefail

readonly version=3.44.8
readonly base_url=https://storage.googleapis.com/flutter_infra_release/releases

case "$(uname -s):$(uname -m)" in
    Linux:x86_64)
        readonly archive=stable/linux/flutter_linux_3.44.8-stable.tar.xz
        readonly expected_sha256=672089e001571a9fbb209a495c583580c0c6c73ef98999264ba07fa93ace332d
        ;;
    Darwin:x86_64)
        readonly archive=stable/macos/flutter_macos_3.44.8-stable.zip
        readonly expected_sha256=b2f765234217327a5859d046c9f3b167387b61da5408b5866ed448d905877c66
        ;;
    Darwin:arm64)
        readonly archive=stable/macos/flutter_macos_arm64_3.44.8-stable.zip
        readonly expected_sha256=c3d6fe95078f7001d947a31d42527de91d5bfe62e4cf444a1493a2e8f1fb199d
        ;;
    *)
        echo "Unsupported Flutter CI host: $(uname -s) $(uname -m)" >&2
        exit 1
        ;;
esac

readonly temp_root=${RUNNER_TEMP:-${TMPDIR:-/tmp}}
install_root=$(mktemp -d "$temp_root/flutter-sdk.XXXXXX")
readonly install_root
archive_path="$install_root/$(basename "$archive")"
readonly archive_path

curl --fail --location --retry 3 --show-error \
    --output "$archive_path" "$base_url/$archive"

if command -v sha256sum >/dev/null 2>&1; then
    printf '%s  %s\n' "$expected_sha256" "$archive_path" | sha256sum --check -
elif command -v shasum >/dev/null 2>&1; then
    printf '%s  %s\n' "$expected_sha256" "$archive_path" | shasum -a 256 --check
else
    echo "No SHA-256 verification tool is available" >&2
    exit 1
fi

case "$archive" in
    *.tar.xz) tar -xJf "$archive_path" -C "$install_root" ;;
    *.zip) unzip -q "$archive_path" -d "$install_root" ;;
esac
rm -f -- "$archive_path"

readonly flutter_bin="$install_root/flutter/bin"
test -x "$flutter_bin/flutter"
printf '%s\n' "$flutter_bin" >> "${GITHUB_PATH:?GITHUB_PATH is required}"
"$flutter_bin/flutter" --version | grep -F "Flutter $version "
