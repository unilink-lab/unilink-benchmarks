#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"${ROOT_DIR}/scripts/run_tcp_latency.sh"
"${ROOT_DIR}/scripts/run_udp_latency.sh"
"${ROOT_DIR}/scripts/run_uds_latency.sh"
