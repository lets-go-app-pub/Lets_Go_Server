
// Created by jeremiah on 7/6/22.
//

#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <coroutine>
#include <unordered_set>
#include <sstream>

#ifdef LG_TESTING
#include <gtest/gtest_prod.h>
#endif

#include "coroutine_type.h"

/** Can read src/utility/async_server/_documentation.md for how this relates to the chat stream. **/

//Accepts coroutines of type ThreadPoolSuspend.
//Also accepts tasks of form std::function<void()>.
class CoroutineThreadPool {
private:

    struct SingleJob {
        const bool is_coroutine;
        std::function<void()> standard_task = nullptr;
        std::coroutine_handle<ThreadPoolSuspend::promise_type> handle = nullptr;
        //Will store handles to any outstanding child co-routines in order of deepest->shallowest. It will NOT
        // include the highest level parent handle (stored inside the handle variable above).
        std::vector<std::coroutine_handle<ThreadPoolSuspend::promise_type>> child_coroutine_handles;

        SingleJob() = delete;

        SingleJob(SingleJob& copy) = delete;

        SingleJob(SingleJob&& move) = delete;

        ~SingleJob() = default;

        SingleJob& operator=(const SingleJob& rhs) = delete;

        explicit SingleJob(
                std::function<void()> _standard_task
                ) : is_coroutine(false),
                standard_task(std::move(_standard_task)) {}

        explicit SingleJob(
                std::coroutine_handle<ThreadPoolSuspend::promise_type> _handle
                ) : is_coroutine(true),
                handle(_handle) {}

    };
public:

    CoroutineThreadPool() {
        initialize_thread(std::thread::hardware_concurrency());
    }

    CoroutineThreadPool(CoroutineThreadPool& copy) = delete;

    CoroutineThreadPool(CoroutineThreadPool&& move) = delete;

    CoroutineThreadPool& operator=(const CoroutineThreadPool& rhs) = delete;

    ~CoroutineThreadPool() {
        stop_pool();
    }

    explicit CoroutineThreadPool(const unsigned int num_threads) {
        initialize_thread(num_threads);
    }

    void submit(const std::function<void()>& job);

    void submit(std::function<void()>&& job);

    void submit_coroutine(std::coroutine_handle<ThreadPoolSuspend::promise_type> handle);

    void stop_pool();

    //NOTE: This is meant for testing purposes. Use sparingly (or preferably not at all)
    // in production code.
    size_t num_jobs_outstanding() {
        std::scoped_lock<std::mutex> thread_lock(thread_loop_mutex);
        return jobs.size();
    }

private:

    static void stop_job(std::unique_ptr<SingleJob>& job);

    //NOTE: An extra thread will be created to watch coroutine delay times.
    void initialize_thread(unsigned int num_threads);

    //Primary loop for each thread.
    //NOTE: This object (and this function specifically) are responsible for
    // cleaning up the coroutines by calling .destroy() from the handle. final_suspend()
    // returns suspend_always so handle.destroy() should always be valid after handle.done()
    // is checked to be true.
    void thread_loop(int index);

    std::atomic_bool should_terminate = false;

    //NOTE: These mutex should never be locked outside this container. This way the threads
    // can always make progress somewhere (assuming no other deadlock bugs).
    std::mutex thread_loop_mutex;

    std::condition_variable thread_loop_condition_variable;

    //The final thread in this vector is the scheduling thread.
    std::vector<std::jthread> threads;
    std::vector<int> operations;

    std::queue<std::unique_ptr<SingleJob>> jobs;

#ifdef LG_TESTING
    FRIEND_TEST(ThreadPoolTests, cleanupChildCoroutines);
#endif
};

//Run a child coroutine from inside a parent coroutine. Handles that the child coroutine may co_yield.
// Child coroutine must return the same type as the calling coroutine. Exceptions will be propagated upwards.
#define RUN_COROUTINE(func_name, ...) \
{ \
    auto handle = func_name(__VA_ARGS__).handle; \
    handle.resume(); \
    while(!handle.done()) { \
        handle.promise().value.child_handles->emplace_back(handle); \
        co_yield handle.promise().value; \
        handle.resume(); \
    } \
    std::exception_ptr exception_ptr = handle.promise().exception_; \
    handle.destroy(); \
    if(exception_ptr) { \
        std::rethrow_exception(exception_ptr); \
    } \
}

#define SUSPEND() std::vector<std::coroutine_handle<ThreadPoolSuspend::promise_type>> child_handles;\
                  co_yield ThreadPoolSuspend::promise_type::ReturnValue(&child_handles);
