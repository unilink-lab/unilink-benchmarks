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
add_unilink_benchmark(bench_uds_echo_server benchmarks/uds/uds_echo_server.cpp)
add_unilink_benchmark(bench_uds_latency_client benchmarks/uds/uds_latency_client.cpp)
