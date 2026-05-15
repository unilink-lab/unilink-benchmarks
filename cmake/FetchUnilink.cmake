include(FetchContent)

set(UNILINK_BENCH_UNILINK_SOURCE_DIR
    ""
    CACHE PATH "Optional local unilink source directory for benchmark development")
set(UNILINK_BENCH_UNILINK_GIT_REPOSITORY
    "https://github.com/jwsung91/unilink.git"
    CACHE STRING "unilink Git repository used by FetchContent")
set(UNILINK_BENCH_UNILINK_GIT_TAG
    "main"
    CACHE STRING "unilink Git tag, branch, or commit used by FetchContent")

set(UNILINK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(UNILINK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(UNILINK_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(UNILINK_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(UNILINK_BUILD_STATIC OFF CACHE BOOL "" FORCE)
set(UNILINK_ENABLE_PERFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
set(UNILINK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)

if(UNILINK_BENCH_UNILINK_SOURCE_DIR)
  FetchContent_Declare(unilink SOURCE_DIR ${UNILINK_BENCH_UNILINK_SOURCE_DIR})
else()
  FetchContent_Declare(
    unilink
    GIT_REPOSITORY ${UNILINK_BENCH_UNILINK_GIT_REPOSITORY}
    GIT_TAG ${UNILINK_BENCH_UNILINK_GIT_TAG}
  )
endif()

FetchContent_MakeAvailable(unilink)
