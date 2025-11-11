#include "tiny_thread_pool.h"

/**
 * @brief Constructor: Initializes pool state.
 */
ThreadPool::ThreadPool() : _threadCount(1), _stop(false) {}

/**
 * @brief Destructor: Stops the pool.
 */
ThreadPool::~ThreadPool() { Stop(); }

/**
 * @brief Initializes the number of threads.
 */
bool ThreadPool::Init(size_t num) {
    std::unique_lock<std::mutex> lock(_queueMutex);

    if (!_workerThreads.empty()) {
        return false; // Already initialized
    }

    _threadCount = num;
    return true;
}

/**
 * @brief Stops all worker threads.
 */
void ThreadPool::Stop() {
    {
        std::unique_lock<std::mutex> lock(_queueMutex);

        _stop = true; // Set stop flag

        _taskCondition.notify_all(); // Wake up all waiting threads
    }

    // Wait for all threads to join
    for (size_t i = 0; i < _workerThreads.size(); i++) {
        if (_workerThreads[i]->joinable()) {
            _workerThreads[i]->join();
        }
        delete _workerThreads[i];
        _workerThreads[i] = NULL;
    }

    std::unique_lock<std::mutex> lock(_queueMutex);
    _workerThreads.clear();
}

/**
 * @brief Creates and starts the worker threads.
 */
bool ThreadPool::Start() {
    std::unique_lock<std::mutex> lock(_queueMutex);

    if (!_workerThreads.empty()) {
        return false; // Already started
    }

    for (size_t i = 0; i < _threadCount; i++) {
        _workerThreads.push_back(new std::thread(&ThreadPool::Run, this));
    }
    return true;
}

/**
 * @brief Worker thread's main function to get and execute tasks.
 */
bool ThreadPool::Get(PoolTaskPtr &task) {
    std::unique_lock<std::mutex> lock(_queueMutex);

    // Wait until there's a task or the pool is stopped
    _taskCondition.wait(lock, [this] {
        return _stop || !_taskQueue.empty();
    });

    if (_stop) {
        return false; // Pool is stopping
    }

    if (!_taskQueue.empty()) {
        task = std::move(_taskQueue.front());
        _taskQueue.pop();
        return true;
    }

    return false; // Should not be reached if logic is correct
}

/**
 * @brief The main loop for each worker thread.
 */
void ThreadPool::Run() {
    while (!IsTerminate()) {
        PoolTaskPtr task;
        bool ok = Get(task); // Blocks until a task is available or pool stops

        if (ok) {
            ++_activeTaskCount; // Mark task as active
            try {
                // Check for expiration
                if (task->_expirationTime != 0 && task->_expirationTime < THREAD_POOL_NOW_MS) {
                    // Task expired, do nothing
                } else {
                    task->_task(); // Execute the task
                }
            } catch (...) {
                // Catch any exceptions from the task
            }

            --_activeTaskCount; // Mark task as finished

            // Notify WaitForAllDone if all work is complete
            if (_activeTaskCount == 0) {
                std::unique_lock<std::mutex> lock(_queueMutex);
                if (_taskQueue.empty()) {
                    _taskCondition.notify_all();
                }
            }
        }
    }
}

/**
 * @brief Waits for all tasks to be completed.
 */
bool ThreadPool::WaitForAllDone(int millsecond) {
    std::unique_lock<std::mutex> lock(_queueMutex);

    // Lambda function to check if all work is done
    auto allDone = [this] {
        return _taskQueue.empty() && _activeTaskCount == 0;
    };

    if (allDone()) {
        return true; // Already done
    }

    if (millsecond < 0) {
        // Wait indefinitely
        _taskCondition.wait(lock, allDone);
        return true;
    } else {
        // Wait with timeout
        return _taskCondition.wait_for(lock, std::chrono::milliseconds(millsecond),
                                       allDone);
    }
}

// --- Platform-specific time functions (unchanged) ---

int gettimeofday(struct timeval &tv) {
#if WIN32
    time_t clock;
    struct tm tm;
    SYSTEMTIME wtm;
    GetLocalTime(&wtm);
    tm.tm_year = wtm.wYear - 1900;
    tm.tm_mon = wtm.wMonth - 1;
    tm.tm_mday = wtm.wDay;
    tm.tm_hour = wtm.wHour;
    tm.tm_min = wtm.wMinute;
    tm.tm_sec = wtm.wSecond;
    tm.tm_isdst = -1;
    clock = mktime(&tm);
    tv.tv_sec = (long)clock; // Cast to long for safety
    tv.tv_usec = wtm.wMilliseconds * 1000;

    return 0;
#else
    // Assuming a definition for '::gettimeofday' exists
    return ::gettimeofday(&tv, 0);
#endif
}

void getNow(timeval *tv) {
// The original code had platform-specific logic (IOS, LINUX) that
// depended on undefined variables (_buf_idx, _t, etc.).
// Simplified to use the portable gettimeofday wrapper.
#if 0 // Disabled original logic due to missing context
    int idx = _buf_idx;
    *tv = _t[idx];
    if (fabs(_cpu_cycle - 0) < 0.0001 && _use_tsc) {
        addTimeOffset(*tv, idx);
    } else {
        TC_Common::gettimeofday(*tv);
    }
#else
    gettimeofday(*tv); // Use the wrapper defined above
#endif
}

int64_t getNowMs() {
    struct timeval tv;
    getNow(&tv);

    return tv.tv_sec * (int64_t)1000 + tv.tv_usec / 1000;
}