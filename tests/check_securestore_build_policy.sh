#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

python3 - <<'PY'
import json
from pathlib import Path
manifest = json.loads(Path('vcpkg.json').read_text())
baseline = manifest.get('builtin-baseline')
assert baseline == '0878b5224d4a4968940ee296a2e7fae2d3b62983', 'vcpkg.json must pin the expected builtin-baseline'
deps = manifest['dependencies']
wx = next((dep for dep in deps if isinstance(dep, dict) and dep.get('name') == 'wxwidgets'), None)
assert wx and 'secretstore' in wx.get('features', []), 'vcpkg.json must request wxwidgets[secretstore]'
gettext = next((dep for dep in deps if isinstance(dep, dict) and dep.get('name') == 'gettext'), None)
assert gettext, 'vcpkg.json must declare gettext'
assert gettext.get('host') is True, 'gettext must be a host dependency'
assert gettext.get('platform') == 'windows', 'gettext host tools must be Windows-only in the manifest'
assert 'tools' in gettext.get('features', []), 'gettext must request the tools feature'
PY

windows_roots=(setup_windows.ps1 .github/workflows/windows-installer.yml docs/developer/build.md docs/user/installation-windows.md docs/user/troubleshooting.md docs/developer/localization.md)
if rg -n 'vcpkg(?:\.exe)?\s+install\s+"?gettext\[tools\]' "${windows_roots[@]}"; then
  echo 'Windows setup must not run a package-argument gettext install from the manifest root.' >&2
  exit 1
fi

rg -q 'PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE.*ON|PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON' \
  .github/workflows/windows-installer.yml \
  .github/workflows/linux-installer.yml \
  .github/workflows/arch-package.yml \
  .github/workflows/macos-installer.yml \
  .github/workflows/macos-15-manual-installer.yml \
  setup_windows.ps1 \
  packaging/arch/PKGBUILD
rg -q 'bootstrap-vcpkg\.bat' setup_windows.ps1
rg -q 'checkout --detach' setup_windows.ps1 .github/workflows/windows-installer.yml
rg -q -- '--x-install-root' setup_windows.ps1 .github/workflows/windows-installer.yml
rg -q 'VCPKG_INSTALLED_DIR' setup_windows.ps1 .github/workflows/windows-installer.yml
rg -q 'securestore-v2' .github/workflows/windows-installer.yml .github/workflows/linux-installer.yml .github/workflows/arch-package.yml .github/workflows/macos-installer.yml .github/workflows/macos-15-manual-installer.yml
rg -q '0878b5224d4a4968940ee296a2e7fae2d3b62983' vcpkg.json setup_windows.ps1 .github/workflows/windows-installer.yml .github/workflows/linux-installer.yml .github/workflows/arch-package.yml .github/workflows/macos-installer.yml .github/workflows/macos-15-manual-installer.yml
rg -q 'ctest --test-dir .* -L release-gate' .github/workflows/windows-installer.yml .github/workflows/linux-installer.yml .github/workflows/macos-installer.yml .github/workflows/macos-15-manual-installer.yml
rg -q 'gdtf_share_security_test' .github/workflows/windows-installer.yml tests/CMakeLists.txt
rg -q 'credential_store_native_roundtrip_test' .github/workflows/windows-installer.yml tests/CMakeLists.txt
rg -q 'libsecret-1-dev' .github/workflows/linux-installer.yml
rg -q "'libsecret'" packaging/arch/PKGBUILD
rg -q -- '--x-manifest-root' .github/workflows/macos-installer.yml .github/workflows/macos-15-manual-installer.yml

echo 'OK: secure-store build, manifest, workflow, and release-gate policies are enforced.'
