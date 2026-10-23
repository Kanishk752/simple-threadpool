# C++ Thread Pool

A C++20 thread pool with configurable workers, a bounded task queue,
future-based results, and exception propagation.

Includes a parallel log-scanning demo, automated tests, and benchmarks.

## Features

- Reuses worker threads to execute tasks.
- Blocks submission when the queue is full.
- Returns results and propagates exceptions through `std::future`.
- Finishes accepted tasks and joins workers during shutdown.

## Build, Test, and Run

Requires CMake 3.16+ and a C++20 compiler.
Tested on Windows with MinGW GCC 16.1.0 and Ninja.

Run from the project root:

```powershell
mkdir build
cd build

cmake ..
cmake --build .

ctest --output-on-failure --timeout 30

.\main.exe       # Basic demo
.\analyze.exe    # Log scanner; generates sample logs if missing
.\speed.exe      # Benchmarks
```

The build defaults to Release mode, enabling compiler optimizations.
For a Debug build, configure with `cmake .. -DCMAKE_BUILD_TYPE=Debug`.

All three test suites passed, covering task results, exceptions,
queue backpressure, and shutdown behaviour.

## Usage

```cpp
#include "threadpool.h"
#include <iostream>

int main() {
    Threadpool pool(4, 64);  // Four workers, queue capacity of 64

    auto result = pool.executeTask([](int a, int b) {
        return a + b;
    }, 10, 20);

    std::cout << result.get() << '\n';  // 30
}
```

The destructor finishes accepted tasks and joins workers automatically.

## Benchmark

Counting primes below 2,000,000, divided into 256 tasks.
Windows, MinGW GCC 16.1.0, Release build; one local run.

| Workers | Time | Speedup |
|---------|------|---------|
| Sequential | 0.246 s | 1.00x |
| 1 | 0.246 s | 1.00x |
| 2 | 0.126 s | 1.96x |
| 4 | 0.069 s | 3.57x |
| 8 | 0.044 s | 5.60x |

Results depend on hardware, workload, and system load.

## Design Notes

- Separate condition variables coordinate workers and producers.
- Tasks execute outside the queue lock.
- `std::packaged_task` stores task results and exceptions.
- Avoid workers waiting for child tasks on the same pool, which can
  deadlock. Call shutdown from an external thread.
