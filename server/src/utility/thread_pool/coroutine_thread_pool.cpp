//
// Created by jeremiah on 7/8/22.
//

#include <iostream>
#include <optional>
#include <store_mongoDB_error_and_exception.h>

#include "coroutine_thread_pool.h"

void CoroutineThreadPool::submit(const std::function<void()>& job) {
    {
        std::scoped_lock<std::mutex> lock(thread_loop_mutex);
        jobs.push(std::make_unique<SingleJob>(job));
    }
    thread_loop_condition_variable.notify_one();
}

void CoroutineThreadPool::submit(std::function<void()>&& job) {
    {
        std::scoped_lock<std::mutex> lock(thread_loop_mutex);
        jobs.push(std::make_unique<SingleJob>(std::move(job)));
    }
    thread_loop_condition_variable.notify_one();
}

void CoroutineThreadPool::submit_coroutine(std::coroutine_handle<ThreadPoolSuspend::promise_type> handle) {
    if (handle) {
        {
            std::scoped_lock<std::mutex> lock(thread_loop_mutex);
            jobs.push(std::make_unique<SingleJob>(handle));
        }
        thread_loop_condition_variable.notify_one();
    }
}

void CoroutineThreadPool::stop_pool() {
    {
        std::scoped_lock<std::mutex> thread_lock(thread_loop_mutex);
        should_terminate = true;
    }
    thread_loop_condition_variable.notify_all();
    for (std::jthread& active_thread : threads) {
        active_thread.join();
    }
    threads.clear();

    while (!jobs.empty()) {
        std::unique_ptr<SingleJob> job = std::move(jobs.front());
        jobs.pop();
        stop_job(job);
    }

    std::string operations_str = "CoroutineThreadPool number of operations for each thread.\n[";
    for (int x : operations) {
        operations_str += std::to_string(x) + ",";
    }
    if(operations.size() > 0) {
        operations_str.pop_back();
    }
    operations_str += "]\n";
    std::cout << operations_str;
}

void CoroutineThreadPool::stop_job(std::unique_ptr<SingleJob>& job) {
    if (job->is_coroutine && job->handle) {
        //clean up child coroutines
        for (auto& child_handle : job->child_coroutine_handles) {
            child_handle.destroy();
        }
        job->child_coroutine_handles.clear();

        job->handle.destroy();
        job->handle = nullptr;
    }

    job->standard_task = nullptr;
    job->handle = nullptr;
}

void CoroutineThreadPool::initialize_thread(const unsigned int threads_passed) {

    //smallest thread pool size is 1.
    int num_threads = 1;
    if(threads_passed > 1) {
        num_threads = threads_passed;
    }

    threads.resize(num_threads);
    operations.resize(num_threads, 0);
    for (unsigned int i = 0; i < threads.size(); i++) {
        threads.at(i) = std::jthread(&CoroutineThreadPool::thread_loop, this, i);
    }
}

//Primary loop for each thread.
//NOTE: This object (and this function specifically) are responsible for
// cleaning up the coroutines by calling .destroy() from the handle. final_suspend()
// returns suspend_always so handle.destroy() should always be valid after handle.done()
// is checked to be true.
void CoroutineThreadPool::thread_loop(int index) {
    while (true) {
        std::unique_ptr<SingleJob> job = nullptr;
        {
            std::unique_lock<std::mutex> lock(thread_loop_mutex);
            thread_loop_condition_variable.wait(lock, [this] {
                return !jobs.empty() || should_terminate;
            });
            if (should_terminate) {
                return;
            }
            job = std::move(jobs.front());
            jobs.pop();
        }
        operations[index]++;
        if (job->is_coroutine && job->handle) {

            job->handle.resume();
            job->child_coroutine_handles.clear(); //these are now invalid

            if (!job->handle.done()) {

                auto& promise = job->handle.promise();

                if (promise.value.child_handles) {
                    //the handles are simply pointers, no need to move them
                    for (const auto& child_handle : *promise.value.child_handles) {
                        job->child_coroutine_handles.emplace_back(child_handle);
                    }
                }

                {
                    std::scoped_lock<std::mutex> lock(thread_loop_mutex);
                    jobs.push(std::move(job));
                }
                thread_loop_condition_variable.notify_one();
            } else {
                if(job->handle.promise().exception_) {
                    std::optional<std::string> exception_string;
                    try {
                        std::rethrow_exception(job->handle.promise().exception_);
                    } catch (const std::exception& e) {
                        exception_string = e.what();
                    }

                    try {
                        const std::string error_string = "CoroutineThreadPool caught an unhandled exception.";
                        storeMongoDBErrorAndException(__LINE__, __FILE__, exception_string, error_string);
                    } catch (const std::exception& e) {
                        std::cout << "CoroutineThreadPool caught an unhandled exception and is unable to store it.\n"
                            << "store_error_exception: " << e.what() << "\n"
                            << "previous_exception: " << *exception_string << "\n";
                    }

                }

                job->handle.destroy();
                job->handle = nullptr;
            }
        } else if (!job->is_coroutine) {
            job->standard_task();
        }
        job = nullptr;
    }
}