//
// Created by jeremiah on 7/8/22.
//

#pragma once

#include <coroutine>
#include <chrono>
#include <vector>

/** Can read src/utility/async_server/_documentation.md for how this relates to the chat stream.
 * It is mentioned under header 'CoroutineThreadPool'. **/

//This is a coroutine type meant to be used with the CoroutineThreadPool class.
// NOTE: It does not have .destroy() called in its destructor, so if using this
// coroutine somewhere else it is important to either use the RUN_COROUTINE macro
// or call the function manually from the handle when finished with the coroutine.
struct ThreadPoolSuspend {

    struct promise_type {

        struct ReturnValue {

            //For simplicity only keeping track of a single child_coroutine. If more are used
            // then when stop_pool() is called from a thread pool it may not clean them all.
            //The actual object will be alive in the 'lowest' coroutine level.
            std::vector<std::coroutine_handle<promise_type>>* child_handles = nullptr;

            ReturnValue() = default;

            explicit ReturnValue(
                    std::vector<std::coroutine_handle<promise_type>>* _child_handles
            ) : child_handles(_child_handles)
                {}
        };

        ReturnValue value;
        std::exception_ptr exception_;

        ThreadPoolSuspend get_return_object() {
            return ThreadPoolSuspend{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        //Return suspend_always for initial_suspend so that when the handle is retrieved it will not run
        // (this is a cold start, if suspend_never was returned it would be a hot start).
        std::suspend_always initial_suspend() {
            value = ReturnValue();
            return {};
        }

        //Return suspend_always for the final_suspend so that .destroy can (and must) always be called.
        std::suspend_always final_suspend() noexcept {
            return {};
        }

        std::suspend_always yield_value(const ReturnValue& passed_val) {
            value = passed_val;
            return {};
        }

        void unhandled_exception() {
            exception_ = std::current_exception();
        }

        void return_void() noexcept {}
    };

    //This functions just like a raw pointer or a string_view. It is just a pointer to the object
    // and if it goes out of bounds it will not hurt the coroutine itself.
    std::coroutine_handle<promise_type> handle;

    explicit ThreadPoolSuspend(std::coroutine_handle<promise_type> _handle) : handle(_handle) {}

    ThreadPoolSuspend() = default;

    //NOTE: There are some issues with destroying the coroutine in the destructor. See answer here for details
    //https://stackoverflow.com/questions/62981634/calling-destroy-on-a-coroutine-handle-causes-segfault
    ~ThreadPoolSuspend() = default;

    ThreadPoolSuspend(ThreadPoolSuspend&& move) = default;

    ThreadPoolSuspend(const ThreadPoolSuspend&) = delete;

    ThreadPoolSuspend& operator=(const ThreadPoolSuspend&) = delete;
};
