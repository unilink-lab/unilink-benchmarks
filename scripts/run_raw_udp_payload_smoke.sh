#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export UNILINK_BENCH_BUILD_DIR="${UNILINK_BENCH_BUILD_DIR:-${ROOT_DIR}/build}"

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-9501}"
PAYLOAD_SIZES="${PAYLOAD_SIZES:-1024 4096 16384}"
ITERATIONS="${ITERATIONS:-1000}"
WARMUP_ITERATIONS="${WARMUP_ITERATIONS:-100}"
TIMEOUT_MS="${TIMEOUT_MS:-1000}"
OUTPUT="${OUTPUT:-${ROOT_DIR}/build/raw_udp_payload_smoke.csv}"
SUMMARY="${SUMMARY:-${ROOT_DIR}/build/raw_udp_payload_smoke_summary.md}"

mkdir -p "$(dirname "${OUTPUT}")" "$(dirname "${SUMMARY}")"
rm -f "${OUTPUT}" "${SUMMARY}"

"${UNILINK_BENCH_BUILD_DIR}/bin/bench_raw_udp_payload_smoke" \
  --host "${HOST}" \
  --port "${PORT}" \
  --payload-sizes "${PAYLOAD_SIZES}" \
  --iterations "${ITERATIONS}" \
  --warmup-iterations "${WARMUP_ITERATIONS}" \
  --timeout-ms "${TIMEOUT_MS}" \
  --csv-output "${OUTPUT}"

python3 "${ROOT_DIR}/scripts/summarize_raw_udp_payload_smoke.py" \
  --input "${OUTPUT}" \
  --output "${SUMMARY}"

echo "csv: ${OUTPUT}"
echo "summary: ${SUMMARY}"
