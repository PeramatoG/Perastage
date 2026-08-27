#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github/workflows/ci-tests.yml"


def workflow_step(text: str, name: str) -> str:
    marker = f"      - name: {name}\n"
    start = text.find(marker)
    if start < 0:
        return ""
    end = text.find("\n      - name: ", start + len(marker))
    return text[start:] if end < 0 else text[start:end]


def require(text: str, needle: str, message: str) -> bool:
    if needle in text:
        return True
    print(message)
    return False


def main() -> int:
    text = WORKFLOW.read_text(encoding="utf-8")
    windows = text[text.index("  windows-debug:"):text.index("\n  macos-debug:")]
    prepare = workflow_step(text, "Prepare Windows Debug test tools")
    resolve_bash = workflow_step(text, "Resolve Git Bash for Windows tests")
    persist_toolchain = workflow_step(windows, "Persist Visual Studio Hostx64 x64 environment")
    restore_vcpkg = workflow_step(windows, "Restore vcpkg installed packages and binary archives")
    write_cache = workflow_step(text, "Write Windows compiler-cache initial cache")
    configure = workflow_step(text, "Configure Windows Debug tests")
    validate = workflow_step(text, "Validate Windows Debug toolchain and sccache launcher")
    ok = True

    ok &= require(prepare, "Get-Command rg.exe", "Windows Debug must resolve ripgrep in PowerShell.")
    ok &= require(prepare, "choco.exe", "Windows Debug must install ripgrep when the hosted runner lacks it.")
    ok &= require(prepare, "BurntSushi/ripgrep/releases/download", "Windows Debug must provide a direct ripgrep fallback.")
    ok &= require(prepare, "Get-FileHash", "Downloaded Windows test tools must be checksum-verified.")
    ok &= require(prepare, "failed SHA-256 verification", "Windows Debug must reject an invalid ripgrep archive.")
    ok &= require(prepare, "windows-test-tools.txt", "Windows Debug must log resolved test tools.")
    ok &= require(resolve_bash, "PERASTAGE_GIT_BASH=$bash", "Windows Debug must publish the validated Git Bash path.")
    ok &= require(resolve_bash, "--noprofile --norc -c 'printf", "Git Bash execution probe must use a non-login shell.")
    ok &= require(resolve_bash, "--noprofile --norc -c 'command -v rg && rg --version'", "Git Bash ripgrep probe must use a non-login shell.")
    ok &= require(resolve_bash, r"\\windowsapps\\bash\.exe$", "Windows Debug must reject the WindowsApps WSL Bash launcher.")
    ok &= require(resolve_bash, r"\\system32\\bash\.exe$", "Windows Debug must reject the System32 WSL Bash launcher.")
    ok &= require(persist_toolchain, "id: windows-toolchain", "Windows Debug must expose its resolved toolchain identity.")
    ok &= require(persist_toolchain, "$env:VCToolsInstallDir", "Windows cache identity must include the active MSVC toolset.")
    ok &= require(persist_toolchain, "$env:WindowsSDKVersion", "Windows cache identity must include the active Windows SDK.")
    ok &= require(persist_toolchain, "cache-identity=$cacheIdentity", "Windows Debug must publish a path-safe cache identity.")
    ok &= require(restore_vcpkg, "steps.windows-toolchain.outputs.cache-identity", "The compiled vcpkg cache key must include the resolved Windows toolchain identity.")
    ok &= require(write_cache, "$bashExecutable = $env:PERASTAGE_GIT_BASH", "Initial-cache generation must read the validated Git Bash path.")
    ok &= require(write_cache, '--bash-executable "$bashExecutable"', "Initial-cache generation must pass the validated Git Bash path to its helper.")
    ok &= require(configure, 'cmake -S . -B build-windows-debug', "Windows Debug must configure with CMake.")
    ok &= require(configure, '-C "$env:CI_LOG_DIR\\cmake-windows-debug-initial-cache.cmake"', "Windows CMake configure must consume the generated initial cache.")
    ok &= require(validate, '--expected-bash "$env:PERASTAGE_GIT_BASH"', "Toolchain validation must verify the configured Bash path.")
    ok &= require(validate, '--expected-launcher "$env:PERASTAGE_SCCACHE_EXECUTABLE"', "Toolchain validation must verify the sccache launcher.")
    if not (text.find("Resolve Git Bash for Windows tests") < text.find("Configure Windows Debug tests")):
        print("Windows Debug must resolve and probe Git Bash before CMake configure.")
        ok = False
    if not (windows.find("Persist Visual Studio Hostx64 x64 environment") < windows.find("Restore vcpkg installed packages and binary archives")):
        print("Windows Debug must resolve and persist its toolchain before restoring compiled vcpkg packages.")
        ok = False
    if "bash -lc 'printf" in resolve_bash or "bash --login" in resolve_bash:
        print("Windows Debug Git Bash probes must not use login-shell mode.")
        ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
