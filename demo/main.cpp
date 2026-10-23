// Basic usage of the pool.

#include "threadpool.h"

#include <iostream>
#include <string>
#include <vector>

int main() {
    unsigned cores = std::thread::hardware_concurrency();

    if (cores == 0) {
        cores = 4;
    }

    Threadpool pool(cores, 50);
    std::cout << "pool started with " << pool.numThreads() << " threads\n\n";

    auto sum = pool.executeTask([](int a, int b) {
        return a + b;
    }, 10, 20);

    auto prod = pool.executeTask([](int a, int b) {
        return a * b;
    }, 10, 20);

    auto name = pool.executeTask([]() {
        return std::string("kanishk");
    });

    std::cout << "10 + 20      = " << sum.get() << "\n";
    std::cout << "10 * 20      = " << prod.get() << "\n";
    std::cout << "string task  = " << name.get() << "\n\n";

    // the exception travels through the future
    auto bad = pool.executeTask([]() -> int {
        throw std::runtime_error("something went wrong inside the task");
    });

    try {
        bad.get();
    } catch (const std::exception& e) {
        std::cout << "caught from task: " << e.what() << "\n\n";
    }

    // split a vector into slices, one task each, add the slices up here
    std::vector<int> data(1000000, 1);

    const std::size_t parts = pool.numThreads();
    const std::size_t step  = data.size() / parts;

    std::vector<std::future<long>> partials;

    for (std::size_t i = 0; i < parts; i++) {
        std::size_t from = i * step;
        std::size_t to   = (i == parts - 1) ? data.size() : from + step;

        partials.push_back(pool.executeTask([&data, from, to]() {
            long total = 0;

            for (std::size_t k = from; k < to; k++) {
                total += data[k];
            }

            return total;
        }));
    }

    long total = 0;

    for (auto& p : partials) {
        total += p.get();
    }

    std::cout << "sum of " << data.size() << " ones = " << total << "\n";
    std::cout << "tasks finished so far          = " << pool.tasksDone() << "\n";

    return 0;
}
