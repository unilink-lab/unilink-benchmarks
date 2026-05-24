#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "${ROOT_DIR}/scripts/bench_common.sh"
export UNILINK_BENCH_BUILD_DIR="${UNILINK_BENCH_BUILD_DIR:-${ROOT_DIR}/build}"

PAYLOAD_SIZES="${PAYLOAD_SIZES:-64 256 1024 4096 16384 65536}"
TRANSPORTS="${TRANSPORTS:-tcp udp uds}"
REPEATS="${REPEATS:-3}"
ITERATIONS="${ITERATIONS:-10000}"
WARMUP_ITERATIONS="${WARMUP_ITERATIONS:-1000}"
UDP_MAX_PAYLOAD_SIZE="${UDP_MAX_PAYLOAD_SIZE:-1024}"
OUTLIER_THRESHOLDS_US="${OUTLIER_THRESHOLDS_US:-5000 10000 50000}"
OUTPUT="${OUTPUT:-${ROOT_DIR}/build/latency_matrix.csv}"
SUMMARY="${SUMMARY:-${ROOT_DIR}/build/latency_matrix_summary.md}"
export OUTLIER_THRESHOLDS_US

mkdir -p "$(dirname "${OUTPUT}")"
rm -f "${OUTPUT}" "${SUMMARY}" "${OUTPUT}.meta"
write_metadata "${OUTPUT}.meta"
{
  echo "payload_sizes,${PAYLOAD_SIZES}"
  echo "transports,${TRANSPORTS}"
  echo "repeats,${REPEATS}"
  echo "iterations,${ITERATIONS}"
  echo "warmup_iterations,${WARMUP_ITERATIONS}"
  echo "udp_max_payload_size,${UDP_MAX_PAYLOAD_SIZE}"
  echo "outlier_thresholds_us,${OUTLIER_THRESHOLDS_US}"
} >>"${OUTPUT}.meta"

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
          if [[ "${UDP_MAX_PAYLOAD_SIZE}" != "0" && "${payload}" -gt "${UDP_MAX_PAYLOAD_SIZE}" ]]; then
            echo "latency skip: transport=udp payload=${payload} exceeds UDP_MAX_PAYLOAD_SIZE=${UDP_MAX_PAYLOAD_SIZE}"
            continue
          fi
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
