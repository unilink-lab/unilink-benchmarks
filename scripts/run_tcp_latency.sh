#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="${ROOT_DIR}/build/bin"
PORT="${PORT:-9000}"
HOST="${HOST:-127.0.0.1}"
PAYLOAD_SIZE="${PAYLOAD_SIZE:-1024}"
ITERATIONS="${ITERATIONS:-100000}"
WARMUP_ITERATIONS="${WARMUP_ITERATIONS:-0}"

"${BIN_DIR}/bench_tcp_echo_server" --port "${PORT}" &
SERVER_PID=$!

cleanup() {
  kill "${SERVER_PID}" >/dev/null 2>&1 || true
  wait "${SERVER_PID}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sleep 0.3
if ! kill -0 "${SERVER_PID}" >/dev/null 2>&1; then
  wait "${SERVER_PID}"
  exit 1
fi

"${BIN_DIR}/bench_tcp_latency_client" \
  --host "${HOST}" \
  --port "${PORT}" \
  --payload-size "${PAYLOAD_SIZE}" \
  --iterations "${ITERATIONS}" \
  --warmup-iterations "${WARMUP_ITERATIONS}" \
  "$@"
