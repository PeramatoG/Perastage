#!/usr/bin/env bash
set -euo pipefail

platform="${1:-}"

case "$platform" in
  linux)
    sudo apt-get update
    sudo apt-get install -y \
      build-essential cmake ninja-build pkg-config gettext autopoint \
      autoconf autoconf-archive automake libtool libltdl-dev curl unzip zip \
      libx11-dev libxau-dev libxdmcp-dev x11proto-dev libxi-dev libxtst-dev \
      libxrender-dev libgtk-3-dev libglib2.0-dev libsecret-1-dev \
      libpango1.0-dev libatk1.0-dev libcairo2-dev libgdk-pixbuf-2.0-dev \
      libxkbcommon-dev libgl1-mesa-dev libglu1-mesa-dev ripgrep xvfb xauth \
      x11-utils locales mono-complete
    ;;
  macos)
    brew update
    brew install autoconf autoconf-archive automake gettext libtool ninja ripgrep
    command -v mono >/dev/null || brew install mono
    ;;
  *)
    echo "Usage: $0 <linux|macos>" >&2
    exit 2
    ;;
esac
