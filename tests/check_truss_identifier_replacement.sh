#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
rg -q 'truss_instance::ReplaceType\(' gui/mainwindow_truss_replace.cpp
if rg -q 'void ApplyTrussReplacement|target = replacement' gui/mainwindow_truss_replace.cpp; then
  echo 'Truss replacement must delegate instance preservation to Core.' >&2
  exit 1
fi
echo 'Truss identifier replacement boundary check passed.'
