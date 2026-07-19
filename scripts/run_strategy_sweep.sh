#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/bench_common.sh"
export WIRESTEAD_BENCH_BUILD_DIR="${WIRESTEAD_BENCH_BUILD_DIR:-${UNILINK_BENCH_BUILD_DIR:-${ROOT_DIR}/build}}"
export UNILINK_BENCH_BUILD_DIR="${WIRESTEAD_BENCH_BUILD_DIR}"

PAYLOAD_SIZES="${PAYLOAD_SIZES:-64 256 1024 4096 16384 65536}"
REPEATS="${REPEATS:-3}"
DURATION="${DURATION:-3}"
OUTPUT="${OUTPUT:-${ROOT_DIR}/build/strategy_sweep.csv}"
SUMMARY="${SUMMARY:-${ROOT_DIR}/build/strategy_sweep_summary.md}"

mkdir -p "$(dirname "${OUTPUT}")"
rm -f "${OUTPUT}" "${SUMMARY}" "${OUTPUT}.meta"
write_metadata "${OUTPUT}.meta"

run_id=0
for payload in ${PAYLOAD_SIZES}; do
  for repeat in $(seq 1 "${REPEATS}"); do
    run_id=$((run_id + 1))
    echo "strategy run: payload=${payload} repeat=${repeat}/${REPEATS}"
    PAYLOAD_SIZE="${payload}" DURATION="${DURATION}" "${ROOT_DIR}/scripts/run_strategy_matrix.sh" \
      --tcp-port "$((9100 + run_id))" \
      --udp-server-port "$((9200 + run_id))" \
      --udp-client-port "$((9300 + run_id))" \
      --uds-path "/tmp/wirestead_strategy_${run_id}.sock" \
      --csv-output "${OUTPUT}"
  done
done

"${ROOT_DIR}/scripts/summarize_strategy_csv.py" "${OUTPUT}" | tee "${SUMMARY}"
echo "csv: ${OUTPUT}"
echo "summary: ${SUMMARY}"
echo "metadata: ${OUTPUT}.meta"
