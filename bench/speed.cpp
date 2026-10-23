// Timing runs. Not a test - it just prints numbers. Build Release.

#include "threadpool.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

// trial division on purpose - the task has to actually keep a core busy
static long countPrimes(long from, long to) {
    long found = 0;
    for (long n = from; n < to; n++) {
        if (n < 2) {
            continue;
        }
        bool prime = true;
        for (long d = 2; d * d <= n; d++) {
            if (n % d == 0) {
                prime = false;
                break;
            }
        }
        if (prime) {
            found++;
        }
    }
    return found;
}

static double seconds(std::chrono::steady_clock::time_point a,
                      std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double>(b - a).count();
}

int main() {
    const long limit  = 2000000;  // count primes below this
    const int  chunks = 256;      // split into this many tasks

    // more chunks than threads: primes get slower as numbers grow, so an
    // even split would leave one thread working alone at the end
    std::cout << "counting primes below " << limit << ", split into " << chunks
              << " tasks\n\n";

    // no pool
    auto t0 = std::chrono::steady_clock::now();
    long expected = countPrimes(0, limit);
    auto t1 = std::chrono::steady_clock::now();
    double baseline = seconds(t0, t1);

    std::cout << "found " << expected << " primes\n\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "threads      time(s)   speedup\n";
    std::cout << "-------------------------------\n";
    std::cout << "none (loop)  " << std::setw(7) << baseline << "    1.00x\n";

    // 1, 2, 4, 8 threads
    const int threadCounts[] = {1, 2, 4, 8};

    for (int n : threadCounts) {
        Threadpool pool(n, 128);

        auto a = std::chrono::steady_clock::now();

        std::vector<std::future<long>> parts;
        parts.reserve(chunks);
        const long step = limit / chunks;

        for (int c = 0; c < chunks; c++) {
            long from = c * step;
            long to   = (c == chunks - 1) ? limit : from + step;
            parts.push_back(pool.executeTask(countPrimes, from, to));
        }

        long total = 0;
        for (auto& p : parts) {
            total += p.get();
        }

        auto b = std::chrono::steady_clock::now();
        double took = seconds(a, b);

        std::cout << std::setw(2) << n << "           " << std::setw(7) << took
                  << "    " << std::setprecision(2) << (baseline / took) << "x"
                  << std::setprecision(3);

        if (total != expected) {
            std::cout << "   WRONG RESULT";
        }
        std::cout << "\n";
    }

    // empty tasks measure the pool itself: lock, queue, function, future
    {
        const int many = 100000;
        Threadpool pool(4, 4096);

        auto a = std::chrono::steady_clock::now();
        for (int i = 0; i < many; i++) {
            pool.executeTask([]() {
            });
        }
        pool.shutdown();
        auto b = std::chrono::steady_clock::now();

        double took = seconds(a, b);
        double perTaskUs = (took * 1e6) / many;

        std::cout << "\nempty tasks: " << many << " in " << took << "s\n";
        std::cout << "cost of one task: " << std::setprecision(2) << perTaskUs
                  << " us\n";
        std::cout << "so a task is only worth queueing if it runs for more\n"
                  << "than roughly " << std::setprecision(0) << (perTaskUs * 10)
                  << " us\n";
        std::cout << "times a worker had to wait: " << pool.workerWaits() << "\n";
    }

    return 0;
}
