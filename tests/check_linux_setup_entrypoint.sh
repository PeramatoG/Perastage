#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT

TEST_REPOSITORY="$TEMP_DIR/repository"
FAKE_BIN="$TEMP_DIR/bin"
COMMAND_LOG="$TEMP_DIR/commands.log"
mkdir -p "$TEST_REPOSITORY/scripts/linux" "$FAKE_BIN"
cp "$ROOT_DIR/setup.sh" "$TEST_REPOSITORY/setup.sh"
cp "$ROOT_DIR/scripts/linux/PerastageLinuxBootstrap.sh" "$TEST_REPOSITORY/scripts/linux/PerastageLinuxBootstrap.sh"
chmod +x "$TEST_REPOSITORY/setup.sh" "$TEST_REPOSITORY/scripts/linux/PerastageLinuxBootstrap.sh"

ln -s "$(command -v bash)" "$FAKE_BIN/bash"
ln -s "$(command -v cat)" "$FAKE_BIN/cat"
ln -s "$(command -v dirname)" "$FAKE_BIN/dirname"

cat >"$FAKE_BIN/cmake" <<EOF
#!/usr/bin/env bash
printf 'cmake:%s:%s\n' "\$PWD" "\$*" >>"$COMMAND_LOG"
EOF
cat >"$FAKE_BIN/sudo" <<EOF
#!/usr/bin/env bash
printf 'sudo:%s\n' "\$*" >>"$COMMAND_LOG"
EOF
chmod +x "$FAKE_BIN/cmake" "$FAKE_BIN/sudo"

run_setup() {
    PATH="$FAKE_BIN" "$TEST_REPOSITORY/setup.sh" "$@"
}

help_output="$(run_setup --help)"
grep -Fq './setup.sh [Debug|Release] [--skip-deps] [--skip-build]' <<<"$help_output"

if run_setup invalid >"$TEMP_DIR/invalid.out" 2>"$TEMP_DIR/invalid.err"; then
    echo "An invalid setup argument unexpectedly succeeded." >&2
    exit 1
fi
grep -Fq 'Unknown argument: invalid' "$TEMP_DIR/invalid.err"

: >"$COMMAND_LOG"
(cd "$TEMP_DIR" && PATH="$FAKE_BIN" "$TEST_REPOSITORY/setup.sh" Debug --skip-deps)
grep -Fq "cmake:$TEST_REPOSITORY:--preset wsl-x64-debug" "$COMMAND_LOG"
grep -Fq "cmake:$TEST_REPOSITORY:--build --preset wsl-debug-build" "$COMMAND_LOG"
if grep -Fq 'sudo:' "$COMMAND_LOG"; then
    echo "--skip-deps invoked the package installer." >&2
    exit 1
fi

: >"$COMMAND_LOG"
run_setup --skip-deps Release --skip-build
grep -Fq "cmake:$TEST_REPOSITORY:--preset wsl-x64-release" "$COMMAND_LOG"
if grep -Fq -- '--build' "$COMMAND_LOG"; then
    echo "--skip-build invoked the build preset." >&2
    exit 1
fi

touch "$FAKE_BIN/apt-get"
chmod +x "$FAKE_BIN/apt-get"
: >"$COMMAND_LOG"
run_setup Release
grep -Fq 'sudo:apt-get update' "$COMMAND_LOG"
grep -Fq 'sudo:apt-get install -y build-essential cmake ninja-build' "$COMMAND_LOG"
rm "$FAKE_BIN/apt-get"

touch "$FAKE_BIN/dnf"
chmod +x "$FAKE_BIN/dnf"
: >"$COMMAND_LOG"
run_setup Debug --skip-build
grep -Fq 'sudo:dnf install -y gcc gcc-c++ make cmake ninja-build' "$COMMAND_LOG"
rm "$FAKE_BIN/dnf"

if run_setup --skip-build >"$TEMP_DIR/unsupported.out" 2>"$TEMP_DIR/unsupported.err"; then
    echo "Setup unexpectedly accepted an unsupported package-manager environment." >&2
    exit 1
fi
grep -Fq 'No supported package manager found.' "$TEMP_DIR/unsupported.err"

rm "$TEST_REPOSITORY/scripts/linux/PerastageLinuxBootstrap.sh"
if "$TEST_REPOSITORY/setup.sh" --help >"$TEMP_DIR/missing.out" 2>"$TEMP_DIR/missing.err"; then
    echo "The launcher unexpectedly succeeded without its implementation." >&2
    exit 1
fi
grep -Fq 'Perastage Linux setup implementation was not found:' "$TEMP_DIR/missing.err"

echo "Linux setup entry-point checks passed."
