// The queue limit and the blocking that goes with it.

#include "threadpool.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

static int failures = 0;

static void check(bool ok, const std::string& what) {
    std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n";

    if (!ok) {
        failures++;
    }
}

int main() {
    std::cout << "queue limit tests\n";

    // one slow worker, queue of 2 - submitting 5 tasks has to block
    {
        Threadpool pool(1, 2);
        std::atomic<int> done{0};

        auto start = std::chrono::steady_clock::now();

        for (int i = 0; i < 5; i++) {
            pool.executeTask([&done]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                done.fetch_add(1);
            });
        }

        auto afterSubmit = std::chrono::steady_clock::now();

        double submitMs =
            std::chrono::duration<double, std::milli>(afterSubmit - start).count();

        // without the limit all five would go in under a millisecond
        check(submitMs > 60.0,
              "producer blocked while the queue was full (" +
                  std::to_string(static_cast<int>(submitMs)) + "ms to submit 5)");

        pool.shutdown();

        check(done.load() == 5, "all 5 tasks finished anyway");
    }

    // The queue size is not readable from outside, so measure submitted minus
    // finished. That can never be more than the limit plus what the workers
    // are running.
    {
        const std::size_t limit   = 4;
        const std::size_t threads = 2;

        Threadpool pool(threads, limit);

        long submitted = 0;
        long worst     = 0;

        for (int i = 0; i < 2000; i++) {
            pool.executeTask([]() {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            });

            submitted++;

            long inFlight = submitted - static_cast<long>(pool.tasksDone());

            if (inFlight > worst) {
                worst = inFlight;
            }
        }

        pool.shutdown();

        const long allowed = static_cast<long>(limit + threads);

        check(worst <= allowed,
              "queue never exceeded its limit (worst in flight " +
                  std::to_string(worst) + ", allowed " +
                  std::to_string(allowed) + ")");

        check(pool.tasksDone() == 2000,
              "all 2000 tasks finished through a queue of 4");
    }

    {
        Threadpool pool(2, 8);

        pool.executeTask([]() {
        }).get();

        pool.shutdown();

        bool threw = false;

        try {
            pool.executeTask([]() {
            });
        } catch (const std::runtime_error&) {
            threw = true;
        }

        check(threw, "submit after shutdown throws");
    }

    // a producer waiting for space must not be left there by shutdown
    {
        Threadpool pool(1, 1);

        pool.executeTask([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        });

        pool.executeTask([]() {
        });

        std::atomic<bool> producerFinished{false};

        std::thread producer([&pool, &producerFinished]() {
            try {
                pool.executeTask([]() {
                });
            } catch (const std::runtime_error&) {
                // expected if shutdown happened while waiting
            }

            producerFinished.store(true);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        pool.shutdown();
        producer.join();

        check(producerFinished.load(), "shutdown releases a blocked producer");
    }

    // works while the queue has room; would deadlock if it filled up
    {
        Threadpool pool(4, 64);

        auto f = pool.executeTask([&pool]() {
            auto inner = pool.executeTask([]() {
                return 5;
            });

            return inner.get();
        });

        check(f.get() == 5, "a task can submit to its own pool");
    }

    std::cout << (failures == 0 ? "queue limit: all passed\n"
                                : "queue limit: FAILURES\n");

    return failures == 0 ? 0 : 1;
}
