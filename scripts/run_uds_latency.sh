#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/bench_common.sh"
UNILINK_BENCH_BUILD_DIR="${UNILINK_BENCH_BUILD_DIR:-${ROOT_DIR}/build}"
BIN_DIR="${UNILINK_BENCH_BUILD_DIR}/bin"
SOCKET_PATH="${SOCKET_PATH:-/tmp/unilink_bench.sock}"
PAYLOAD_SIZE="${PAYLOAD_SIZE:-1024}"
ITERATIONS="${ITERATIONS:-100000}"
WARMUP_ITERATIONS="${WARMUP_ITERATIONS:-0}"

rm -f "${SOCKET_PATH}"
"${BIN_DIR}/bench_uds_echo_server" --path "${SOCKET_PATH}" &
SERVER_PID=$!

cleanup() {
  kill "${SERVER_PID}" >/dev/null 2>&1 || true
  wait "${SERVER_PID}" >/dev/null 2>&1 || true
  rm -f "${SOCKET_PATH}"
}
trap cleanup EXIT

if ! wait_for_process "${SERVER_PID}" 5; then
  wait "${SERVER_PID}"
  exit 1
fi
if ! wait_for_uds "${SOCKET_PATH}" 5; then
  echo "UDS server did not create socket ${SOCKET_PATH}" >&2
  exit 1
fi

"${BIN_DIR}/bench_uds_latency_client" \
  --path "${SOCKET_PATH}" \
  --payload-size "${PAYLOAD_SIZE}" \
  --iterations "${ITERATIONS}" \
  --warmup-iterations "${WARMUP_ITERATIONS}" \
  "$@"
