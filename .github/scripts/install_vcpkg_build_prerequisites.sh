#!/usr/bin/env bash
set -euo pipefail

platform="${1:-}"

case "$platform" in
  linux|linux-appimage)
    if [ "$(id -u)" -eq 0 ]; then
      apt=(apt-get)
    elif command -v sudo >/dev/null 2>&1; then
      apt=(sudo apt-get)
    else
      echo "Linux prerequisites require root or sudo." >&2
      exit 1
    fi

    "${apt[@]}" update
    packages=(
      build-essential cmake ninja-build pkg-config gettext autopoint \
      autoconf autoconf-archive automake libtool libltdl-dev curl unzip zip \
      libx11-dev libxau-dev libxdmcp-dev x11proto-dev libxi-dev libxtst-dev \
      libxrender-dev libgtk-3-dev libglib2.0-dev libsecret-1-dev \
      libpango1.0-dev libatk1.0-dev libcairo2-dev libgdk-pixbuf-2.0-dev \
      libxkbcommon-dev libgl1-mesa-dev libglu1-mesa-dev ripgrep xvfb xauth \
      x11-utils locales mono-complete
    )
    if [ "$platform" = linux-appimage ]; then
      packages+=(gcc-11 g++-11 patchelf imagemagick file desktop-file-utils
        shared-mime-info libglib2.0-bin libgpg-error0 libgcrypt20 xdotool)
    fi
    "${apt[@]}" install -y "${packages[@]}"
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
