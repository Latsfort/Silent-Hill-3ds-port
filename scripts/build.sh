#!/usr/bin/env bash
# Run inside the devkitPro MSYS2 shell.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ -z "${DEVKITPRO:-}" ]]; then
  echo "DEVKITPRO is not set. Open the 'devkitPro' MSYS2 shell first."
  exit 1
fi

make clean
make -j"$(nproc 2>/dev/null || echo 2)"
echo
echo "Built: $ROOT/silent-hill-3ds.3dsx"
echo "Copy to sd:/3ds/silent-hill-3ds/ then launch from Homebrew Launcher."
