// Everyday behaviour: results and exceptions come back, every task runs.

#include "threadpool.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

static int failures = 0;

static void check(bool ok, const std::string& what) {
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n";

    if (!ok) {
        failures++;
    }
}

struct MyError : std::exception {
    const char* what() const noexcept override {
        return "my error";
    }
};

static int doubleIt(int x) {
    return x * 2;
}

int main() {
    std::cout << "basic tests\n";

    {
        Threadpool pool(4, 32);

        auto f = pool.executeTask([](int a, int b) {
            return a + b;
        }, 2, 3);

        check(f.get() == 5, "int result");
    }

    {
        Threadpool pool(4, 32);

        auto f = pool.executeTask([](const std::string& s, int n) {
            return s + std::to_string(n);
        }, std::string("abc"), 7);

        check(f.get() == "abc7", "string result");
    }

    {
        Threadpool pool(2, 32);
        std::atomic<int> hit{0};

        auto f = pool.executeTask([&hit]() {
            hit.fetch_add(1);
        });

        f.get();

        check(hit.load() == 1, "void task ran");
    }

    {
        Threadpool pool(2, 32);

        auto f = pool.executeTask([]() -> int {
            throw std::runtime_error("boom");
        });

        bool caught = false;

        try {
            f.get();
        } catch (const std::runtime_error& e) {
            caught = (std::string(e.what()) == "boom");
        }

        check(caught, "exception reaches the caller");
    }

    // several threads submitting at the same time
    {
        const int producers = 4;
        const int each      = 5000;

        Threadpool pool(8, 128);
        std::atomic<int> ran{0};

        std::vector<std::thread> submitters;

        for (int p = 0; p < producers; p++) {
            submitters.emplace_back([&pool, &ran, each]() {
                for (int i = 0; i < each; i++) {
                    pool.executeTask([&ran]() {
                        ran.fetch_add(1);
                    });
                }
            });
        }

        for (std::thread& t : submitters) {
            t.join();
        }

        pool.shutdown();

        check(ran.load() == producers * each,
              "all " + std::to_string(producers * each) + " tasks ran (got " +
                  std::to_string(ran.load()) + ")");
    }

    // no future should end up holding another task's answer
    {
        Threadpool pool(8, 64);
        std::vector<std::future<int>> futures;

        for (int i = 0; i < 500; i++) {
            futures.push_back(pool.executeTask([](int x) {
                return x * x;
            }, i));
        }

        bool allRight = true;

        for (int i = 0; i < 500; i++) {
            if (futures[i].get() != i * i) {
                allRight = false;
            }
        }

        check(allRight, "each future holds its own task's result");
    }

    {
        Threadpool pool(2, 16);

        auto f = pool.executeTask([]() {
            return std::make_unique<int>(99);
        });

        std::unique_ptr<int> got = f.get();

        check(got && *got == 99, "move-only return type");
    }

    // the type should survive, not just the fact that something threw
    {
        Threadpool pool(2, 16);

        auto f = pool.executeTask([]() -> int {
            throw MyError{};
        });

        bool rightType = false;

        try {
            f.get();
        } catch (const MyError&) {
            rightType = true;
        } catch (...) {
            rightType = false;
        }

        check(rightType, "custom exception type is preserved");
    }

    {
        Threadpool pool(2, 16);

        auto f = pool.executeTask(doubleIt, 21);

        check(f.get() == 42, "function pointer as the task");
    }

    {
        Threadpool pool(4, 64);
        std::atomic<int> ran{0};

        for (int i = 0; i < 100; i++) {
            pool.executeTask([&ran]() {
                ran.fetch_add(1);
            });
        }

        pool.shutdown();

        check(ran.load() == 100, "tasks run even when the future is discarded");
    }

    {
        Threadpool pool(4, 64);

        for (int i = 0; i < 1000; i++) {
            pool.executeTask([]() {
            });
        }

        pool.shutdown();

        check(pool.tasksDone() == 1000,
              "tasksDone() == 1000 (got " + std::to_string(pool.tasksDone()) + ")");
    }

    {
        Threadpool pool(0, 0);

        auto f = pool.executeTask([]() {
            return 42;
        });

        check(pool.numThreads() == 1 && f.get() == 42,
              "0 threads / 0 queue is clamped to 1 each");
    }

    // 64 threads on 8 cores
    {
        Threadpool pool(64, 128);
        std::atomic<int> ran{0};

        for (int i = 0; i < 5000; i++) {
            pool.executeTask([&ran]() {
                ran.fetch_add(1);
            });
        }

        pool.shutdown();

        check(ran.load() == 5000, "64 threads, 5000 tasks");
    }

    std::cout << (failures == 0 ? "basic: all passed\n" : "basic: FAILURES\n");

    return failures == 0 ? 0 : 1;
}
