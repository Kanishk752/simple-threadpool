#include "threadpool.h"

Threadpool::Threadpool(std::size_t threads, std::size_t queueLimit)
    : maxtasks(queueLimit == 0 ? 1 : queueLimit),
      stop(false),
      tasksDoneCount(0),
      workerWaitCount(0)
{
    if (threads == 0) {
        threads = 1;
    }

    workers.reserve(threads);

    for (std::size_t i = 0; i < threads; i++) {
        workers.emplace_back([this]() {
            workerLoop();
        });
    }
}

Threadpool::~Threadpool() {
    shutdown();
}

void Threadpool::workerLoop() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> ul(mtx);

            if (tasks.empty() && !stop) {
                workerWaitCount.fetch_add(1);

                notEmpty.wait(ul, [this]() {
                    return !tasks.empty() || stop;
                });
            }

            // empty AND stop - otherwise queued tasks get thrown away
            if (tasks.empty() && stop) {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }

        notFull.notify_one();

        task();     // lock released, or the pool would run one task at a time

        tasksDoneCount.fetch_add(1);
    }
}

void Threadpool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> ul(mtx);

        notFull.wait(ul, [this]() {
            return tasks.size() < maxtasks || stop;
        });

        if (stop) {
            throw std::runtime_error("Threadpool is stopped");
        }

        tasks.push(std::move(task));
    }

    notEmpty.notify_one();
}

void Threadpool::shutdown() {
    {
        std::lock_guard<std::mutex> lg(mtx);

        if (stop) {
            return;
        }

        stop = true;
    }

    // both: workers so they can drain and exit, producers so they stop
    // waiting for space
    notEmpty.notify_all();
    notFull.notify_all();

    for (std::thread& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }
}
