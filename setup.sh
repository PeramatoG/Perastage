#!/usr/bin/env bash

set -euo pipefail

REPOSITORY_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMPLEMENTATION_SCRIPT="$REPOSITORY_ROOT/scripts/linux/PerastageLinuxBootstrap.sh"

if [[ ! -f "$IMPLEMENTATION_SCRIPT" ]]; then
    echo "Perastage Linux setup implementation was not found: $IMPLEMENTATION_SCRIPT" >&2
    exit 1
fi

exec "$IMPLEMENTATION_SCRIPT" "$@"
