# tiny_thread_pool
A lightweight, fast, and easy-to-use C++17 thread pool with support for task futures and timeouts.
# C++11 Thread Pool

A lightweight, fast, and easy-to-use C++11 thread pool with support for task futures and timeouts.

This implementation is self-contained in `tc_thread_pool.h` and `tc_thread_pool.cc`, making it easy to integrate into any C++11 compatible project.

## Features

* **Modern C++17:** Built entirely with standard C++11~C++17 components (`std::scoped_lock<T>()`,`std::thread`, `std::mutex`, `std::condition_variable`, `std::future`, `std::atomic`).
* **`std::future` Based:** The `Exec` method returns a `std::future`, allowing you to easily retrieve return values or catch exceptions from asynchronous tasks.
* **Task Timeouts:** Supports submitting tasks with an expiration time. Expired tasks are discarded if not processed in time, preventing stale work.
* **Graceful Shutdown:** Provides `Stop()` and `WaitForAllDone()` methods for clean and safe termination, ensuring all tasks are completed before exit.
* **Easy Integration:** Self-contained in one `.h` and one `.cc` file.
* **High Performance:** Designed for low-latency task submission and high throughput, suitable for both I/O-bound and CPU-bound workloads.

## Requirements

* A C++17 compliant compiler (e.g., g++ 4.8+, Clang 3.3+, MSVC 2013+).
* For Linux/macOS, linking against the `pthread` library is required (`-lpthread`).

## How to Build
# Building with CMake (Recommended)
mkdir build
cd build
cmake ..
cmake --build .
make
./my_app

# on linux:
g++ -o my_app main.cpp tc_thread_pool.cc -std=c++11 -lpthread -O2
./my_app

