include(FetchContent)

set(UNILINK_BENCH_UNILINK_SOURCE_DIR
    ""
    CACHE PATH "Optional local unilink source directory for benchmark development")

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
    GIT_REPOSITORY https://github.com/jwsung91/unilink.git
    GIT_TAG main # TODO: replace with a stable release tag
  )
endif()

FetchContent_MakeAvailable(unilink)
