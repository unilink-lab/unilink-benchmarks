#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/bench_common.sh"
BIN_DIR="${ROOT_DIR}/build/bin"
PORT="${PORT:-9001}"
HOST="${HOST:-127.0.0.1}"
PAYLOAD_SIZE="${PAYLOAD_SIZE:-1024}"
ITERATIONS="${ITERATIONS:-100000}"
WARMUP_ITERATIONS="${WARMUP_ITERATIONS:-0}"
SERVER_START_DELAY="${SERVER_START_DELAY:-0.2}"

"${BIN_DIR}/bench_udp_echo_server" --port "${PORT}" &
SERVER_PID=$!

cleanup() {
  kill "${SERVER_PID}" >/dev/null 2>&1 || true
  wait "${SERVER_PID}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

if ! wait_for_process "${SERVER_PID}" 5; then
  wait "${SERVER_PID}"
  exit 1
fi

sleep "${SERVER_START_DELAY}"

"${BIN_DIR}/bench_udp_latency_client" \
  --host "${HOST}" \
  --port "${PORT}" \
  --payload-size "${PAYLOAD_SIZE}" \
  --iterations "${ITERATIONS}" \
  --warmup-iterations "${WARMUP_ITERATIONS}" \
  "$@"
