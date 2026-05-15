# unilink-benchmarks

Standalone performance benchmarks for unilink.

This repository measures latency, throughput, and concurrency behavior across supported unilink transports.

## What is unilink?

unilink is a modern C++ communication library that provides a unified interface for serial, network, and local IPC transports.

Core library:
https://github.com/jwsung91/unilink

## Scope

This repository is separate from the core library to keep the main `unilink` release and package flow lightweight.

The first benchmark suite focuses on local echo latency tests for:

- TCP
- UDP
- Unix Domain Socket

Serial benchmarks may be added later as optional hardware-dependent benchmarks.

TCP and UDS latency benchmarks use a small length header internally so each round trip measures one complete payload
even though those transports are byte streams.

## Build

### Option 1: Use installed unilink package

```bash
cmake -S . -B build -DUNILINK_BENCH_USE_FETCHCONTENT=OFF
cmake --build build
```

### Option 2: Fetch unilink from source

```bash
cmake -S . -B build -DUNILINK_BENCH_USE_FETCHCONTENT=ON
cmake --build build
```

For local development against a checked-out `unilink` source tree:

```bash
cmake -S . -B build -DUNILINK_BENCH_USE_FETCHCONTENT=ON \
  -DUNILINK_BENCH_UNILINK_SOURCE_DIR=/path/to/unilink
cmake --build build
```

## Run

Binaries are generated under:

```text
build/bin
```

Example:

```bash
./build/bin/bench_tcp_echo_server --port 9000
./build/bin/bench_tcp_latency_client --host 127.0.0.1 --port 9000 --payload-size 1024 --iterations 100000
```

Or use scripts:

```bash
./scripts/run_tcp_latency.sh
./scripts/run_udp_latency.sh
./scripts/run_uds_latency.sh
./scripts/run_all_local.sh
```

Latency clients also support warmup iterations and CSV output:

```bash
./build/bin/bench_tcp_latency_client \
  --host 127.0.0.1 \
  --port 9000 \
  --payload-size 1024 \
  --iterations 100000 \
  --warmup-iterations 1000 \
  --csv-output results.csv
```

## Backpressure Strategy

unilink supports two backpressure strategies:

- `Reliable`: waits for queue pressure to clear and prioritizes delivery.
- `BestEffort`: avoids blocking and may drop data under pressure.

Use the strategy benchmark to compare accepted throughput, received throughput, delivery rate, and failed sends:

```bash
./scripts/run_strategy_matrix.sh
```

Or run it directly:

```bash
./build/bin/bench_strategy_matrix --payload-size 1024 --duration 3 --csv-output strategy.csv
```

## Metrics

Each benchmark reports:

- payload size
- iterations
- elapsed time
- messages/sec
- MiB/sec
- latency min / avg / p50 / p95 / p99 / max

The strategy benchmark reports:

- accepted MiB/sec
- received MiB/sec
- delivery rate
- failed sends

## Notes

Benchmark results may vary depending on:

- OS
- CPU
- scheduler behavior
- system load
- socket buffer size
- loopback network behavior
- compiler and build type

Use Release builds for meaningful measurements:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## License

Apache-2.0
