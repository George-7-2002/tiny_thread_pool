#include "tiny_thread_pool.h"
#include <iostream>
#include <chrono>
#include <atomic>
#include <vector>
#include <numeric> // For std::iota

/**
 * @brief Lightweight task for Test 1
 * * Only performs an atomic increment, used to test maximum throughput.
 */
void light_task(std::atomic<long>& counter) {
    counter++;
}

/**
 * @brief CPU-intensive task for Test 2
 * * @param n Calculate the sum up to n
 * @return long The sum from 0 to n-1
 */
long cpu_intensive_task(int n) {
    long sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += i;
    }
    return sum;
}

// C++11 style timer
class Timer {
public:
    Timer() : m_start(std::chrono::high_resolution_clock::now()) {}

    void reset() {
        m_start = std::chrono::high_resolution_clock::now();
    }

    // Returns milliseconds (ms)
    double elapsed_ms() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count() / 1000.0;
    }

    // Returns seconds (s)
    double elapsed_s() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count() / 1e9;
    }

private:
    std::chrono::high_resolution_clock::time_point m_start;
};


int main() {
    // -----------------------------------------------------------------
    // Test 1: High Throughput (Lightweight Tasks)
    // -----------------------------------------------------------------
    std::cout << "--- Test 1: High Throughput (Lightweight Tasks) ---" << std::endl;

    const int num_threads = 8;
    const long num_tasks = 5000000; // 5 million tasks
    std::atomic<long> task_counter(0);

    ThreadPool pool_light;
    pool_light.Init(num_threads);
    pool_light.Start();

    std::cout << "Submitting " << num_tasks << " lightweight tasks to " << num_threads << " threads..." << std::endl;

    Timer timer_light;

    // Submit all tasks
    for (long i = 0; i < num_tasks; ++i) {
        // Use std::ref to pass the atomic counter by reference
        pool_light.Exec(light_task, std::ref(task_counter));
    }

    // Wait for all tasks to complete
    pool_light.WaitForAllDone();
    double time_taken_s = timer_light.elapsed_s();

    // Stop the thread pool
    pool_light.Stop();

    std::cout << "--- Results (Test 1) ---" << std::endl;
    std::cout << "Verification: " << (task_counter.load() == num_tasks ? "SUCCESS" : "FAILURE") << std::endl;
    std::cout << "  - Expected tasks: " << num_tasks << std::endl;
    std::cout << "  - Executed tasks: " << task_counter.load() << std::endl;
    std::cout << "Total time: " << time_taken_s << " seconds" << std::endl;
    std::cout << "Throughput: " << static_cast<long>(num_tasks / time_taken_s) << " tasks/second" << std::endl;
    std::cout << std::endl;


    // -----------------------------------------------------------------
    // Test 2: CPU-Bound (Heavyweight Tasks)
    // -----------------------------------------------------------------
    std::cout << "--- Test 2: CPU-Bound (Heavyweight Tasks) ---" << std::endl;

    // Number of tasks is usually less than Test 1, but each task is heavier
    const int num_threads_cpu = 8;
    const int num_tasks_cpu = 1000;
    const int work_factor = 50000; // Amount of calculation per task

    ThreadPool pool_cpu;
    pool_cpu.Init(num_threads_cpu);
    pool_cpu.Start();

    std::cout << "Submitting " << num_tasks_cpu << " CPU-bound tasks..." << std::endl;

    // Store futures to get return values
    std::vector<std::future<long>> futures;

    Timer timer_cpu;

    for (int i = 0; i < num_tasks_cpu; ++i) {
        futures.push_back(pool_cpu.Exec(cpu_intensive_task, work_factor));
    }

    // Wait for and collect all results
    // Note: .get() will block until the future for that task has a result
    // This step completes both waiting and result verification
    long long total_sum = 0;
    for (auto& f : futures) {
        total_sum += f.get();
    }

    double time_taken_cpu_s = timer_cpu.elapsed_s();

    // Stop the thread pool
    pool_cpu.Stop();

    // Verify results
    long expected_sum_per_task = 0;
    for(int i = 0; i < work_factor; ++i) expected_sum_per_task += i;
    long long expected_total_sum = (long long)expected_sum_per_task * num_tasks_cpu;

    std::cout << "--- Results (Test 2) ---" << std::endl;
    std::cout << "Verification: " << (total_sum == expected_total_sum ? "SUCCESS" : "FAILURE") << std::endl;
    std::cout << "  - Expected sum: " << expected_total_sum << std::endl;
    std::cout << "  - Calculated sum: " << total_sum << std::endl;
    std::cout << "Total time: " << time_taken_cpu_s << " seconds" << std::endl;

    return 0;
}
