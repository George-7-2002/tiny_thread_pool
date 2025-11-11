#ifndef _TC_THREAD_POOL_H_
#define _TC_THREAD_POOL_H_

#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <atomic> // Added for std::atomic
#include <thread>  // Added for std::thread

#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif
using namespace std;

// --- Utility Time Functions ---
void getNow(timeval *tv);
int64_t getNowMs();

// Renamed macro for clarity
#define THREAD_POOL_NOW_MS getNowMs()

/////////////////////////////////////////////////
/**
 * @file tc_thread_pool.h
 * @brief C++11 Thread Pool Implementation
 * * (Usage instructions from original comments...)
 */

class ThreadPool {
protected:
    /**
     * @brief Internal struct to hold a task and its metadata.
     */
    struct PoolTask {
        PoolTask(uint64_t expireTime) : _expirationTime(expireTime) {}

        std::function<void()> _task; // The actual callable task
        int64_t _expirationTime = 0; // Absolute expiration time (0 = no expiration)
    };
    typedef shared_ptr<PoolTask> PoolTaskPtr;

public:
    /**
     * @brief Constructor.
     */
    ThreadPool();

    /**
     * @brief Destructor. Calls Stop() to ensure all threads are joined.
     */
    virtual ~ThreadPool();

    /**
     * @brief Initializes the thread pool with a fixed number of threads.
     * @note Must be called before Start().
     *
     * @param num Number of worker threads to create.
     * @return true on success, false if pool is already started.
     */
    bool Init(size_t num);

    /**
     * @brief Gets the number of worker threads.
     *
     * @return size_t The number of threads.
     */
    size_t GetThreadNum() {
        std::scoped_lock<std::mutex> lock(_queueMutex);
        return _workerThreads.size();
    }

    /**
     * @brief Gets the number of pending tasks in the queue.
     *
     * @return size_t The number of tasks waiting to be executed.
     */
    size_t GetJobNum() {
        std::scoped_lock<std::mutex> lock(_queueMutex);
        return _taskQueue.size();
    }

    /**
     * @brief Stops the thread pool.
     * @note Waits for all running tasks to complete and joins all threads.
     */
    void Stop();

    /**
     * @brief Starts the worker threads.
     * @return true on success, false if threads are already running.
     */
    bool Start();

    /**
     * @brief Submits a task to the thread pool (no timeout).
     *
     * @tparam F Type of the callable (function, lambda, etc.).
     * @tparam Args Types of the arguments to the callable.
     * @param f The callable object.
     * @param args Arguments to pass to the callable.
     * @return A std::future object to get the task's return value.
     */
    template <class F, class... Args>
    auto Exec(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
        return Exec(0, f, args...); // 0 timeout
    }

    /**
     * @brief Submits a task to the thread pool with an optional timeout.
     *
     * @tparam F Type of the callable (function, lambda, etc.).
     * @tparam Args Types of the arguments to the callable.
     * @param timeoutMs Timeout in milliseconds. 0 means no timeout.
     * If the task doesn't start before this, it will be discarded.
     * @param f The callable object.
     * @param args Arguments to pass to the callable.
     * @return A std::future object to get the task's return value.
     */
    template <class F, class... Args>
    auto Exec(int64_t timeoutMs, F &&f, Args &&...args)
    -> std::future<decltype(f(args...))> {

        int64_t expireTime =
                (timeoutMs == 0 ? 0 : THREAD_POOL_NOW_MS + timeoutMs);

        // Deduce the return type
        using RetType = decltype(f(args...));

        // Package the task to get a future
        auto task = std::make_shared<std::packaged_task<RetType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        //Wrap it in our internal task struct
        PoolTaskPtr fPtr = std::make_shared<PoolTask>(expireTime);
        fPtr->_task = [task]() { // The function to run is to call the packaged_task
            (*task)();
        };

        // Add task to the queue
        {
            std::scoped_lock<std::mutex> lock(_queueMutex);
            if (_stop) { // Don't add tasks if the pool is stopping
                throw std::runtime_error("Exec on stopped ThreadPool");
            }
            _taskQueue.push(fPtr);
        }

        // Notify one waiting thread
        _taskCondition.notify_one();

        return task->get_future();
    }

    /**
     * @brief Waits until all tasks are completed (both in queue and running).
     *
     * @param millsecond Timeout in milliseconds.
     * -1: Wait indefinitely.
     * >0: Wait for a specific duration.
     * @return           true if all tasks are done,
     * false if the timeout occurred.
     */
    bool WaitForAllDone(int millsecond = -1);

protected:
    /**
     * @brief Gets a task from the queue. Blocks if the queue is empty.
     *
     * @param task (out) The smart pointer to receive the task.
     * @return true if a task was successfully retrieved,
     * false if the pool is stopping and no task was retrieved.
     */
    bool Get(PoolTaskPtr &task);

    /**
     * @brief Checks if the thread pool is marked for termination.
     */
    bool IsTerminate() { return _stop; }

    /**
     * @brief The main loop for each worker thread.
     * Continuously fetches and executes tasks.
     */
    void Run();

protected:
    /**
     * @brief The queue of pending tasks.
     */
    queue<PoolTaskPtr> _taskQueue;

    /**
     * @brief Vector holding all worker threads.
     */
    std::vector<std::thread *> _workerThreads;

    /**
     * @brief Mutex to protect the task queue and worker thread vector.
     */
    std::mutex _queueMutex;

    /**
     * @brief Condition variable to signal threads about new tasks or termination.
     */
    std::condition_variable _taskCondition;

    /**
     * @brief The configured number of threads.
     */
    size_t _threadCount;

    /**
     * @brief Flag to signal all threads to stop.
     */
    bool _stop;

    /**
     * @brief Atomic counter for tasks currently being executed.
     */
    std::atomic<int> _activeTaskCount{0};
};

#endif // _TC_THREAD_POOL_H_