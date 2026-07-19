#pragma once

#ifdef WIRESTEAD_BENCH_USE_LEGACY_UNILINK_API
#include <unilink/unilink.hpp>
namespace wirestead = unilink;
#else
#include <wirestead/wirestead.hpp>
#endif
