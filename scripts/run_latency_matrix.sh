#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/bench_common.sh"

PAYLOAD_SIZES="${PAYLOAD_SIZES:-64 256 1024 4096 16384 65536}"
TRANSPORTS="${TRANSPORTS:-tcp udp uds}"
REPEATS="${REPEATS:-3}"
ITERATIONS="${ITERATIONS:-10000}"
WARMUP_ITERATIONS="${WARMUP_ITERATIONS:-1000}"
OUTPUT="${OUTPUT:-${ROOT_DIR}/build/latency_matrix.csv}"
SUMMARY="${SUMMARY:-${ROOT_DIR}/build/latency_matrix_summary.md}"

mkdir -p "$(dirname "${OUTPUT}")"
rm -f "${OUTPUT}" "${SUMMARY}" "${OUTPUT}.meta"
write_metadata "${OUTPUT}.meta"

run_id=0
for payload in ${PAYLOAD_SIZES}; do
  for repeat in $(seq 1 "${REPEATS}"); do
    run_id=$((run_id + 1))
    for transport in ${TRANSPORTS}; do
      echo "latency run: transport=${transport} payload=${payload} repeat=${repeat}/${REPEATS}"
      case "${transport}" in
        tcp)
          PORT=$((9000 + run_id)) PAYLOAD_SIZE="${payload}" ITERATIONS="${ITERATIONS}" \
            WARMUP_ITERATIONS="${WARMUP_ITERATIONS}" \
            "${ROOT_DIR}/scripts/run_tcp_latency.sh" --csv-output "${OUTPUT}"
          ;;
        udp)
          PORT=$((10000 + run_id)) PAYLOAD_SIZE="${payload}" ITERATIONS="${ITERATIONS}" \
            WARMUP_ITERATIONS="${WARMUP_ITERATIONS}" \
            "${ROOT_DIR}/scripts/run_udp_latency.sh" --csv-output "${OUTPUT}"
          ;;
        uds)
          SOCKET_PATH="/tmp/unilink_bench_${run_id}.sock" PAYLOAD_SIZE="${payload}" ITERATIONS="${ITERATIONS}" \
            WARMUP_ITERATIONS="${WARMUP_ITERATIONS}" \
            "${ROOT_DIR}/scripts/run_uds_latency.sh" --csv-output "${OUTPUT}"
          ;;
        *)
          echo "unknown transport: ${transport}" >&2
          exit 1
          ;;
      esac
    done
  done
done

"${ROOT_DIR}/scripts/summarize_latency_csv.py" "${OUTPUT}" | tee "${SUMMARY}"
echo "csv: ${OUTPUT}"
echo "summary: ${SUMMARY}"
echo "metadata: ${OUTPUT}.meta"
