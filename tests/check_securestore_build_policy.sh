#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

python3 - <<'PY'
import json
from pathlib import Path
manifest = json.loads(Path('vcpkg.json').read_text())
assert manifest.get('builtin-baseline'), 'vcpkg.json must pin builtin-baseline'
wx = next((dep for dep in manifest['dependencies'] if isinstance(dep, dict) and dep.get('name') == 'wxwidgets'), None)
assert wx and 'secretstore' in wx.get('features', []), 'vcpkg.json must request wxwidgets[secretstore]'
PY

rg -q 'PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE.*ON|PERASTAGE_REQUIRE_SECURE_CREDENTIAL_STORE=ON' \
  .github/workflows/windows-installer.yml \
  .github/workflows/linux-installer.yml \
  .github/workflows/arch-package.yml \
  .github/workflows/macos-installer.yml \
  .github/workflows/macos-15-manual-installer.yml \
  setup_windows.ps1 \
  packaging/arch/PKGBUILD
rg -q 'libsecret-1-dev' .github/workflows/linux-installer.yml
rg -q "'libsecret'" packaging/arch/PKGBUILD
rg -q 'securestore-v1' .github/workflows/windows-installer.yml .github/workflows/linux-installer.yml .github/workflows/arch-package.yml .github/workflows/macos-installer.yml .github/workflows/macos-15-manual-installer.yml
rg -q '0878b5224d4a4968940ee296a2e7fae2d3b62983' vcpkg.json setup_windows.ps1 .github/workflows/windows-installer.yml .github/workflows/linux-installer.yml .github/workflows/arch-package.yml .github/workflows/macos-installer.yml .github/workflows/macos-15-manual-installer.yml
rg -q -- '--x-manifest-root' setup_windows.ps1 .github/workflows/windows-installer.yml .github/workflows/linux-installer.yml .github/workflows/arch-package.yml .github/workflows/macos-installer.yml .github/workflows/macos-15-manual-installer.yml

echo 'OK: official build paths require wxSecretStore and request wxwidgets[secretstore].'
