#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if rg -n '#include.*(wx|gui|configmanager|layer_service)' core/layer_identity.{h,cpp}; then
  echo "Layer identity must remain independent of GUI and application state." >&2
  exit 1
fi
if rg -n '\.layer (==|!=) layer.name|uuid = "layer_default"' core mvr gui; then
  echo "Production ownership cannot use Layer names or synthetic non-UUID identities." >&2
  exit 1
fi
echo "Layer UUID identity boundary check passed."
