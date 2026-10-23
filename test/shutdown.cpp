// Shutdown - where this kind of code usually breaks, either by hanging or by
// dropping tasks that were still queued.

#include "threadpool.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static int failures = 0;

static void check(bool ok, const std::string& what) {
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n";

    if (!ok) {
        failures++;
    }
}

int main() {
    std::cout << "shutdown tests\n";

    // big queue, few workers, so there is a real backlog when stop is set
    {
        const int total = 2000;

        Threadpool pool(4, 512);
        std::atomic<int> ran{0};

        for (int i = 0; i < total; i++) {
            pool.executeTask([&ran]() {
                ran.fetch_add(1);
            });
        }

        pool.shutdown();

        check(ran.load() == total,
              "no task lost: " + std::to_string(ran.load()) + "/" +
                  std::to_string(total));
    }

    {
        Threadpool pool(2, 64);
        std::atomic<int> ran{0};

        for (int i = 0; i < 10; i++) {
            pool.executeTask([&ran]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                ran.fetch_add(1);
            });
        }

        pool.shutdown();

        check(ran.load() == 10, "slow tasks finished before shutdown returned");
    }

    // the destructor calls shutdown() too, so it has to be safe twice
    {
        Threadpool pool(2, 16);

        pool.executeTask([]() {
        }).get();

        pool.shutdown();
        pool.shutdown();

        check(true, "shutdown twice does not hang or crash");
    }

    {
        std::atomic<int> ran{0};

        {
            Threadpool pool(4, 256);

            for (int i = 0; i < 500; i++) {
                pool.executeTask([&ran]() {
                    ran.fetch_add(1);
                });
            }
        }

        check(ran.load() == 500, "destructor drained the queue");
    }

    // would hang if the workers were waiting without being notified
    {
        Threadpool pool(4, 16);

        pool.shutdown();

        check(true, "unused pool shuts down");
    }

    // only one caller should do the work - nobody may join a thread twice
    {
        Threadpool pool(4, 256);
        std::atomic<int> ran{0};

        for (int i = 0; i < 500; i++) {
            pool.executeTask([&ran]() {
                ran.fetch_add(1);
            });
        }

        std::vector<std::thread> callers;

        for (int i = 0; i < 4; i++) {
            callers.emplace_back([&pool]() {
                pool.shutdown();
            });
        }

        for (std::thread& t : callers) {
            t.join();
        }

        check(ran.load() == 500, "shutdown called from 4 threads at once");
    }

    // every submit must be either accepted and run, or rejected - never
    // accepted and then quietly dropped
    {
        Threadpool pool(4, 32);

        std::atomic<int> accepted{0};
        std::atomic<int> rejected{0};
        std::atomic<int> ran{0};

        std::thread submitter([&]() {
            for (int i = 0; i < 3000; i++) {
                try {
                    pool.executeTask([&ran]() {
                        ran.fetch_add(1);
                    });

                    accepted.fetch_add(1);
                } catch (const std::runtime_error&) {
                    rejected.fetch_add(1);
                }
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        pool.shutdown();
        submitter.join();

        check(accepted.load() + rejected.load() == 3000,
              "every submit was either accepted or rejected");

        check(ran.load() == accepted.load(),
              "every accepted task ran (" + std::to_string(ran.load()) + "/" +
                  std::to_string(accepted.load()) + ")");
    }

    std::cout << (failures == 0 ? "shutdown: all passed\n"
                                : "shutdown: FAILURES\n");

    return failures == 0 ? 0 : 1;
}
