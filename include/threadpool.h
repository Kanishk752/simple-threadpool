#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

// Worker threads pulling tasks off a shared queue.
// The queue is size limited, so executeTask() blocks when it is full.
class Threadpool {
public:
    Threadpool(std::size_t threads, std::size_t queueLimit);
    ~Threadpool();

    Threadpool(const Threadpool&) = delete;
    Threadpool& operator=(const Threadpool&) = delete;

    // Template, so it has to be defined here. Wraps the callable into a
    // function<void()> and hands it to enqueue().
    // Note: args must be copyable (bind stores them).
    template<class F, class... Args>
    auto executeTask(F&& func, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
    {
        using returnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

        auto boundTask = std::bind(std::forward<F>(func), std::forward<Args>(args)...);

        // shared_ptr because packaged_task is move-only but std::function
        // needs something copyable
        auto task = std::make_shared<std::packaged_task<returnType()>>(
            std::move(boundTask));

        std::future<returnType> result = task->get_future();

        enqueue([task]() {
            (*task)();
        });

        return result;
    }

    // Finishes queued tasks, then joins. Safe to call twice.
    void shutdown();

    std::size_t numThreads() const {
        return workers.size();
    }

    std::size_t tasksDone() const {
        return tasksDoneCount.load();
    }

    std::size_t workerWaits() const {
        return workerWaitCount.load();
    }

private:
    void workerLoop();
    void enqueue(std::function<void()> task);

    std::mutex mtx;
    std::condition_variable notEmpty;   // workers wait here
    std::condition_variable notFull;    // producers wait here

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::size_t maxtasks;
    bool stop;      // guarded by mtx

    std::atomic<std::size_t> tasksDoneCount;
    std::atomic<std::size_t> workerWaitCount;
};

#endif
