#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "Usage: $0 <url> <sha256> <output>" >&2
  exit 2
fi

url="$1"
expected_sha256="$2"
output="$3"

curl --fail --location --retry 3 --retry-delay 2 --output "$output" "$url"
test -s "$output" || { echo "Downloaded file is empty: $output" >&2; exit 1; }

actual_sha256="$(sha256sum "$output" | awk '{print $1}')"
if [ "$actual_sha256" != "$expected_sha256" ]; then
  echo "SHA256 mismatch for $output" >&2
  echo "Expected: $expected_sha256" >&2
  echo "Actual:   $actual_sha256" >&2
  rm -f "$output"
  exit 1
fi

echo "Verified SHA256 for $output: $actual_sha256"
