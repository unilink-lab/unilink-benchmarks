#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BASELINE_REF="${BASELINE_REF:-v0.7.4}"
CANDIDATE_REF="${CANDIDATE_REF:-v0.7.5}"
COMPARISON_DIR="${COMPARISON_DIR:-${ROOT_DIR}/build/comparison}"
BUILD_ROOT="${BUILD_ROOT:-${ROOT_DIR}/build/comparison-builds}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

PAYLOAD_SIZES="${PAYLOAD_SIZES:-64 256 1024 4096 16384 65536}"
REPEATS="${REPEATS:-3}"
ITERATIONS="${ITERATIONS:-10000}"
WARMUP_ITERATIONS="${WARMUP_ITERATIONS:-1000}"
DURATION="${DURATION:-3}"
UDP_MAX_PAYLOAD_SIZE="${UDP_MAX_PAYLOAD_SIZE:-1024}"

safe_ref() {
  printf '%s' "$1" | sed 's/[^A-Za-z0-9._-]/-/g'
}

run_ref() {
  local ref="$1"
  local safe
  safe="$(safe_ref "${ref}")"
  local build_dir="${BUILD_ROOT}/${safe}"
  local result_dir="${COMPARISON_DIR}/${safe}"

  echo "==> configuring ${ref}"
  cmake -S "${ROOT_DIR}" -B "${build_dir}" \
    -DUNILINK_BENCH_USE_FETCHCONTENT=ON \
    -DUNILINK_BENCH_UNILINK_GIT_TAG="${ref}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"

  echo "==> building ${ref}"
  if [[ -n "${BUILD_JOBS:-}" ]]; then
    cmake --build "${build_dir}" --parallel "${BUILD_JOBS}"
  else
    cmake --build "${build_dir}" --parallel
  fi

  echo "==> running benchmark sweeps for ${ref}"
  mkdir -p "${result_dir}"
  python3 "${ROOT_DIR}/scripts/collect_environment.py" "${result_dir}"

  UNILINK_BENCH_BUILD_DIR="${build_dir}" \
  PAYLOAD_SIZES="${PAYLOAD_SIZES}" \
  REPEATS="${REPEATS}" \
  ITERATIONS="${ITERATIONS}" \
  WARMUP_ITERATIONS="${WARMUP_ITERATIONS}" \
  UDP_MAX_PAYLOAD_SIZE="${UDP_MAX_PAYLOAD_SIZE}" \
  OUTPUT="${result_dir}/latency_matrix.csv" \
  SUMMARY="${result_dir}/latency_matrix_summary.md" \
    "${ROOT_DIR}/scripts/run_latency_matrix.sh"

  UNILINK_BENCH_BUILD_DIR="${build_dir}" \
  PAYLOAD_SIZES="${PAYLOAD_SIZES}" \
  REPEATS="${REPEATS}" \
  DURATION="${DURATION}" \
  OUTPUT="${result_dir}/strategy_sweep.csv" \
  SUMMARY="${result_dir}/strategy_sweep_summary.md" \
    "${ROOT_DIR}/scripts/run_strategy_sweep.sh"

  python3 "${ROOT_DIR}/scripts/package_results.py" \
    --result-dir "${result_dir}" \
    --output-dir "${COMPARISON_DIR}/packages" \
    --unilink-ref "${ref}" \
    --platform-suffix "comparison-local"
}

main() {
  mkdir -p "${COMPARISON_DIR}" "${BUILD_ROOT}"

  echo "comparison baseline: ${BASELINE_REF}"
  echo "comparison candidate: ${CANDIDATE_REF}"

  if ! run_ref "${BASELINE_REF}"; then
    echo "release comparison failed while benchmarking baseline ref ${BASELINE_REF}" >&2
    exit 1
  fi

  if ! run_ref "${CANDIDATE_REF}"; then
    echo "release comparison failed while benchmarking candidate ref ${CANDIDATE_REF}" >&2
    exit 1
  fi

  python3 "${ROOT_DIR}/scripts/compare_results.py" \
    --baseline "${COMPARISON_DIR}/$(safe_ref "${BASELINE_REF}")" \
    --candidate "${COMPARISON_DIR}/$(safe_ref "${CANDIDATE_REF}")" \
    --baseline-label "${BASELINE_REF}" \
    --candidate-label "${CANDIDATE_REF}" \
    --output "${COMPARISON_DIR}/compare_summary.md"

  echo "comparison summary: ${COMPARISON_DIR}/compare_summary.md"
}

main "$@"
