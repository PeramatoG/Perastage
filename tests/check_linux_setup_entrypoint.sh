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

assert_text_contains() {
    local assertion_name="$1"
    local expected_text="$2"
    local actual_text="$3"

    if [[ "$actual_text" != *"$expected_text"* ]]; then
        printf 'Assertion failed: %s\nExpected text: %s\nActual text:\n%s\n' \
            "$assertion_name" "$expected_text" "$actual_text" >&2
        exit 1
    fi
}

assert_file_contains() {
    local assertion_name="$1"
    local expected_text="$2"
    local actual_file="$3"

    if ! grep -Fq -- "$expected_text" "$actual_file"; then
        printf 'Assertion failed: %s\nExpected text: %s\nActual file (%s):\n' \
            "$assertion_name" "$expected_text" "$actual_file" >&2
        cat "$actual_file" >&2
        exit 1
    fi
}

assert_file_excludes() {
    local assertion_name="$1"
    local unexpected_text="$2"
    local actual_file="$3"

    if grep -Fq -- "$unexpected_text" "$actual_file"; then
        printf 'Assertion failed: %s\nUnexpected text: %s\nActual file (%s):\n' \
            "$assertion_name" "$unexpected_text" "$actual_file" >&2
        cat "$actual_file" >&2
        exit 1
    fi
}

create_tool_wrapper() {
    local tool_name="$1"
    local tool_path="$2"

    cat >"$FAKE_BIN/$tool_name" <<EOF
#!/bin/sh
exec "$tool_path" "\$@"
EOF
    chmod +x "$FAKE_BIN/$tool_name"
}

# Wrappers avoid Git Bash's platform-dependent symbolic-link emulation.
create_tool_wrapper bash "$(command -v bash)"
create_tool_wrapper cat "$(command -v cat)"
create_tool_wrapper dirname "$(command -v dirname)"

export PERASTAGE_TEST_REPOSITORY="$TEST_REPOSITORY"

cat >"$FAKE_BIN/cmake" <<EOF
#!/bin/sh
if [ "\$PWD" -ef "\$PERASTAGE_TEST_REPOSITORY" ]; then
    working_directory='repository-root'
else
    working_directory="unexpected:\$PWD"
fi
printf 'cmake:%s:%s\n' "\$working_directory" "\$*" >>"$COMMAND_LOG"
EOF
cat >"$FAKE_BIN/sudo" <<EOF
#!/bin/sh
printf 'sudo:%s\n' "\$*" >>"$COMMAND_LOG"
EOF
chmod +x "$FAKE_BIN/cmake" "$FAKE_BIN/sudo"

run_setup() {
    PATH="$FAKE_BIN" "$TEST_REPOSITORY/setup.sh" "$@"
}

help_output="$(run_setup --help)"
assert_text_contains "help preserves public usage" \
    './setup.sh [Debug|Release] [--skip-deps] [--skip-build]' "$help_output"

if run_setup 'invalid argument' >"$TEMP_DIR/invalid.out" 2>"$TEMP_DIR/invalid.err"; then
    echo "An invalid setup argument unexpectedly succeeded." >&2
    exit 1
fi
assert_file_contains "invalid argument is passed through unchanged" \
    'Unknown argument: invalid argument' "$TEMP_DIR/invalid.err"

: >"$COMMAND_LOG"
(cd "$TEMP_DIR" && PATH="$FAKE_BIN" "$TEST_REPOSITORY/setup.sh" Debug --skip-deps)
assert_file_contains "non-root invocation selects the Debug configure preset" \
    'cmake:repository-root:--preset wsl-x64-debug' "$COMMAND_LOG"
assert_file_contains "Debug selects the Debug build preset" \
    'cmake:repository-root:--build --preset wsl-debug-build' "$COMMAND_LOG"
assert_file_excludes "--skip-deps avoids package installation" 'sudo:' "$COMMAND_LOG"

: >"$COMMAND_LOG"
run_setup --skip-deps Release --skip-build
assert_file_contains "Release selects the Release configure preset" \
    'cmake:repository-root:--preset wsl-x64-release' "$COMMAND_LOG"
assert_file_excludes "--skip-build avoids the build preset" '--build' "$COMMAND_LOG"

touch "$FAKE_BIN/apt-get"
chmod +x "$FAKE_BIN/apt-get"
: >"$COMMAND_LOG"
run_setup Release
assert_file_contains "apt updates package metadata" 'sudo:apt-get update' "$COMMAND_LOG"
assert_file_contains "apt installs the expected dependency prefix" \
    'sudo:apt-get install -y build-essential cmake ninja-build' "$COMMAND_LOG"
rm "$FAKE_BIN/apt-get"

touch "$FAKE_BIN/dnf"
chmod +x "$FAKE_BIN/dnf"
: >"$COMMAND_LOG"
run_setup Debug --skip-build
assert_file_contains "dnf installs the expected dependency prefix" \
    'sudo:dnf install -y gcc gcc-c++ make cmake ninja-build' "$COMMAND_LOG"
rm "$FAKE_BIN/dnf"

if run_setup --skip-build >"$TEMP_DIR/unsupported.out" 2>"$TEMP_DIR/unsupported.err"; then
    echo "Setup unexpectedly accepted an unsupported package-manager environment." >&2
    exit 1
fi
assert_file_contains "unsupported package manager fails explicitly" \
    'No supported package manager found.' "$TEMP_DIR/unsupported.err"

rm "$TEST_REPOSITORY/scripts/linux/PerastageLinuxBootstrap.sh"
if "$TEST_REPOSITORY/setup.sh" --help >"$TEMP_DIR/missing.out" 2>"$TEMP_DIR/missing.err"; then
    echo "The launcher unexpectedly succeeded without its implementation." >&2
    exit 1
fi
assert_file_contains "missing implementation fails explicitly" \
    'Perastage Linux setup implementation was not found:' "$TEMP_DIR/missing.err"

echo "Linux setup entry-point checks passed."
