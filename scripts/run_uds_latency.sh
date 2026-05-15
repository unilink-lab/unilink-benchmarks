#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="${ROOT_DIR}/build/bin"
SOCKET_PATH="${SOCKET_PATH:-/tmp/unilink_bench.sock}"
PAYLOAD_SIZE="${PAYLOAD_SIZE:-1024}"
ITERATIONS="${ITERATIONS:-100000}"

rm -f "${SOCKET_PATH}"
"${BIN_DIR}/bench_uds_echo_server" --path "${SOCKET_PATH}" &
SERVER_PID=$!

cleanup() {
  kill "${SERVER_PID}" >/dev/null 2>&1 || true
  wait "${SERVER_PID}" >/dev/null 2>&1 || true
  rm -f "${SOCKET_PATH}"
}
trap cleanup EXIT

sleep 0.3
if ! kill -0 "${SERVER_PID}" >/dev/null 2>&1; then
  wait "${SERVER_PID}"
  exit 1
fi

"${BIN_DIR}/bench_uds_latency_client" \
  --path "${SOCKET_PATH}" \
  --payload-size "${PAYLOAD_SIZE}" \
  --iterations "${ITERATIONS}"
