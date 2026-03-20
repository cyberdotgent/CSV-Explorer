#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake --preset windows-mingw64 --fresh
cmake --build --preset windows-mingw64
