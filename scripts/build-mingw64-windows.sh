#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PARALLEL_JOBS="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc 2>/dev/null || printf '2')}"

cmake --preset windows-mingw64 --fresh
cmake --build --preset windows-mingw64 --parallel "${PARALLEL_JOBS}"
