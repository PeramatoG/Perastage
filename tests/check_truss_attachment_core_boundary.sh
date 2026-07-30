#!/usr/bin/env bash
set -euo pipefail

files=(
  core/truss_attachment_candidates.cpp
  core/truss_attachment_candidates.h
  core/truss_screen_snap.cpp
  core/truss_screen_snap.h
)

if rg -n '#include <(GL|OpenGL)|#include "(gui|viewer2d|viewer3d|configmanager)' "${files[@]}"; then
  echo "Truss attachment core services must remain GUI and OpenGL independent." >&2
  exit 1
fi
