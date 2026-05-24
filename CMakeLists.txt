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

include(CheckCXXSourceCompiles)
find_package(Threads REQUIRED)

get_target_property(
  UNILINK_BENCH_UNILINK_INCLUDE_DIRS
  unilink::unilink
  INTERFACE_INCLUDE_DIRECTORIES
)
set(CMAKE_REQUIRED_INCLUDES ${UNILINK_BENCH_UNILINK_INCLUDE_DIRS})
check_cxx_source_compiles(
  "
  #include <unilink/unilink.hpp>
  #include <utility>

  int main() {
    using Stats = decltype(std::declval<unilink::TcpClient&>().stats());
    (void)sizeof(decltype(std::declval<Stats&>().messages_accepted));
    (void)sizeof(decltype(std::declval<Stats&>().dropped_messages));
    return 0;
  }
  "
  UNILINK_BENCH_HAS_RUNTIME_STATS
)
unset(CMAKE_REQUIRED_INCLUDES)

if(UNILINK_BENCH_HAS_RUNTIME_STATS)
  set(UNILINK_BENCH_RUNTIME_STATS_SUPPORTED 1)
else()
  set(UNILINK_BENCH_RUNTIME_STATS_SUPPORTED 0)
endif()

file(
  WRITE "${CMAKE_BINARY_DIR}/unilink_bench_metadata.csv"
  "key,value\n"
  "unilink_source_kind,${UNILINK_BENCH_UNILINK_SOURCE_KIND}\n"
  "unilink_requested_ref,${UNILINK_BENCH_UNILINK_REQUESTED_REF}\n"
  "unilink_source_path,${UNILINK_BENCH_UNILINK_SOURCE_PATH}\n"
  "unilink_commit,${UNILINK_BENCH_UNILINK_COMMIT}\n"
  "runtime_stats_supported,${UNILINK_BENCH_RUNTIME_STATS_SUPPORTED}\n"
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

add_executable(bench_raw_udp_payload_smoke benchmarks/udp/raw_udp_payload_smoke.cpp)
target_include_directories(bench_raw_udp_payload_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
target_link_libraries(bench_raw_udp_payload_smoke PRIVATE Threads::Threads)
if(UNILINK_BENCH_HAS_RUNTIME_STATS)
  target_compile_definitions(bench_udp_payload_smoke PRIVATE UNILINK_BENCH_HAS_RUNTIME_STATS=1)
  target_compile_definitions(bench_strategy_matrix PRIVATE UNILINK_BENCH_HAS_RUNTIME_STATS=1)
endif()
