#!/usr/bin/env bash

wait_for_process() {
  local pid="$1"
  local seconds="${2:-5}"
  local attempts=$((seconds * 10))

  for _ in $(seq 1 "${attempts}"); do
    if kill -0 "${pid}" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
  done

  return 1
}

wait_for_tcp() {
  local host="$1"
  local port="$2"
  local seconds="${3:-5}"
  local attempts=$((seconds * 10))

  for _ in $(seq 1 "${attempts}"); do
    if (echo >/dev/tcp/"${host}"/"${port}") >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.1
  done

  return 1
}

wait_for_uds() {
  local path="$1"
  local seconds="${2:-5}"
  local attempts=$((seconds * 10))

  for _ in $(seq 1 "${attempts}"); do
    if [[ -S "${path}" ]]; then
      return 0
    fi
    sleep 0.1
  done

  return 1
}

write_metadata() {
  local path="$1"
  {
    echo "timestamp_utc,$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "uname,$(uname -a)"
    echo "cmake,$(cmake --version | head -n 1)"
    echo "cxx,$(${CXX:-c++} --version | head -n 1)"
  } >"${path}"
}
