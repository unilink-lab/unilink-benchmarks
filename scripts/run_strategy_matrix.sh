#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="${ROOT_DIR}/build/bin"
PAYLOAD_SIZE="${PAYLOAD_SIZE:-1024}"
DURATION="${DURATION:-3}"

"${BIN_DIR}/bench_strategy_matrix" \
  --payload-size "${PAYLOAD_SIZE}" \
  --duration "${DURATION}" \
  "$@"
