cmake_minimum_required(VERSION 3.12)

project(unilink_benchmarks LANGUAGES CXX)

option(UNILINK_BENCH_USE_FETCHCONTENT "Fetch unilink using FetchContent" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(UNILINK_BENCH_USE_FETCHCONTENT)
  include(cmake/FetchUnilink.cmake)
else()
  find_package(unilink CONFIG REQUIRED)
endif()

set(UNILINK_BENCH_UNILINK_SOURCE_KIND "installed-package")
set(UNILINK_BENCH_UNILINK_SOURCE_PATH "")
set(UNILINK_BENCH_UNILINK_REQUESTED_REF "")
set(UNILINK_BENCH_UNILINK_COMMIT "")

if(UNILINK_BENCH_USE_FETCHCONTENT)
  set(UNILINK_BENCH_UNILINK_SOURCE_KIND "fetchcontent")
  set(UNILINK_BENCH_UNILINK_REQUESTED_REF
      "${UNILINK_BENCH_UNILINK_GIT_TAG}"
  )
  if(UNILINK_BENCH_UNILINK_SOURCE_DIR)
    set(UNILINK_BENCH_UNILINK_SOURCE_KIND "local-source")
    set(UNILINK_BENCH_UNILINK_REQUESTED_REF "local-source")
    set(UNILINK_BENCH_UNILINK_SOURCE_PATH
        "${UNILINK_BENCH_UNILINK_SOURCE_DIR}"
    )
  elseif(DEFINED unilink_SOURCE_DIR)
    set(UNILINK_BENCH_UNILINK_SOURCE_PATH "${unilink_SOURCE_DIR}")
  endif()
else()
  if(DEFINED unilink_DIR)
    set(UNILINK_BENCH_UNILINK_SOURCE_PATH "${unilink_DIR}")
  endif()
endif()

if(UNILINK_BENCH_UNILINK_SOURCE_PATH)
  execute_process(
    COMMAND git -C "${UNILINK_BENCH_UNILINK_SOURCE_PATH}" rev-parse HEAD
    OUTPUT_VARIABLE UNILINK_BENCH_UNILINK_COMMIT
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

file(
  WRITE "${CMAKE_BINARY_DIR}/unilink_bench_metadata.csv"
  "key,value\n"
  "unilink_source_kind,${UNILINK_BENCH_UNILINK_SOURCE_KIND}\n"
  "unilink_requested_ref,${UNILINK_BENCH_UNILINK_REQUESTED_REF}\n"
  "unilink_source_path,${UNILINK_BENCH_UNILINK_SOURCE_PATH}\n"
  "unilink_commit,${UNILINK_BENCH_UNILINK_COMMIT}\n"
)

add_library(unilink_bench_common INTERFACE)
target_include_directories(unilink_bench_common
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
target_link_libraries(unilink_bench_common INTERFACE unilink::unilink)

function(add_unilink_benchmark target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE unilink_bench_common)
endfunction()

add_unilink_benchmark(bench_tcp_echo_server benchmarks/tcp/tcp_echo_server.cpp)
add_unilink_benchmark(bench_tcp_latency_client benchmarks/tcp/tcp_latency_client.cpp)
add_unilink_benchmark(bench_udp_echo_server benchmarks/udp/udp_echo_server.cpp)
add_unilink_benchmark(bench_udp_latency_client benchmarks/udp/udp_latency_client.cpp)
add_unilink_benchmark(bench_udp_payload_smoke benchmarks/udp/udp_payload_smoke.cpp)
add_unilink_benchmark(bench_uds_echo_server benchmarks/uds/uds_echo_server.cpp)
add_unilink_benchmark(bench_uds_latency_client benchmarks/uds/uds_latency_client.cpp)
add_unilink_benchmark(bench_strategy_matrix benchmarks/strategy/strategy_matrix.cpp)
