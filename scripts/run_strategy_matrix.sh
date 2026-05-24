#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UNILINK_BENCH_BUILD_DIR="${UNILINK_BENCH_BUILD_DIR:-${ROOT_DIR}/build}"
BIN_DIR="${UNILINK_BENCH_BUILD_DIR}/bin"
PAYLOAD_SIZE="${PAYLOAD_SIZE:-1024}"
DURATION="${DURATION:-3}"

"${BIN_DIR}/bench_strategy_matrix" \
  --payload-size "${PAYLOAD_SIZE}" \
  --duration "${DURATION}" \
  "$@"
