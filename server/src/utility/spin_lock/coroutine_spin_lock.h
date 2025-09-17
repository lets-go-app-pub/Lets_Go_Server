//
// Created by jeremiah on 7/11/22.
//

#pragma once

#include <atomic>
#include <memory>
#include <general_values.h>

#include "coroutine_thread_pool.h"
#include "coroutine_type.h"

//Functions as a long for a coroutine or a standard thread. Meant to be used with
// CoroutineSpinlockWrapper class (non-coroutine) or SCOPED_SPIN_LOCK macro
// (coroutine).
//NOTE: This locking method is not fair.
class CoroutineSpinlock {
public:
    ThreadPoolSuspend lock() {
        for(
                int i = 0; flag_.load(std::memory_order_relaxed)
                || flag_.exchange(1, std::memory_order_acquire); ++i
                ) {
            if(i == general_values::SPINLOCK_NUMBER_TIMES_TO_LOOP_BEFORE_SLEEP) {
                SUSPEND();
                i = 0;
            }
        }

        co_return;
    }

    void non_coroutine_lock() {
        for(
                int i = 0; flag_.load(std::memory_order_relaxed)
                || flag_.exchange(1, std::memory_order_acquire); ++i
                ) {
            if(i == general_values::SPINLOCK_NUMBER_TIMES_TO_LOOP_BEFORE_SLEEP) {
                static const timespec ns = { 0,1 }; //1 nanosecond
                nanosleep(&ns, nullptr);
                i = 0;
            }
        }
    }

    void unlock() {
        flag_.store(0, std::memory_order_release);
    }

    CoroutineSpinlock(const CoroutineSpinlock& wrapper) = delete;

    CoroutineSpinlock(CoroutineSpinlock&& wrapper) = delete;

    bool operator=(CoroutineSpinlock& rhs) = delete;

    CoroutineSpinlock() = default;

    ~CoroutineSpinlock() = default;
private:

    std::atomic<unsigned int> flag_ = 0;
};

//Used for a non-coroutine thread (will block) it can be called raw with the CoroutineSpinlock passed
// to the constructor.
class SpinlockWrapper {
public:

    explicit SpinlockWrapper(
            CoroutineSpinlock& passed_lock
            ) : lock(&passed_lock) {
        lock->non_coroutine_lock();
    }

    SpinlockWrapper() = delete;

    SpinlockWrapper(const SpinlockWrapper& wrapper) = delete;

    SpinlockWrapper(SpinlockWrapper&& wrapper) = delete;

    bool operator=(SpinlockWrapper& rhs) = delete;

    ~SpinlockWrapper() {
        lock->unlock();
        lock = nullptr;
    }

private:
    CoroutineSpinlock* lock;
};

//This class is not meant to be used raw. It is used as part of the macro SCOPED_SPIN_LOCK
// which requires a CoroutineSpinlock passed to it.
class CoroutineSpinlockWrapper {
public:

    explicit CoroutineSpinlockWrapper(
            CoroutineSpinlock& passed_lock
            ) : lock(&passed_lock) {}

    CoroutineSpinlockWrapper() = delete;

    CoroutineSpinlockWrapper(const CoroutineSpinlockWrapper& wrapper) = delete;

    CoroutineSpinlockWrapper(CoroutineSpinlockWrapper&& wrapper) = delete;

    bool operator=(CoroutineSpinlockWrapper& rhs) = delete;

    ~CoroutineSpinlockWrapper() {
        lock->unlock();
        lock = nullptr;
    }

private:
    CoroutineSpinlock* lock;
};

#define SCOPED_SPIN_LOCK(coroutine_spin_lock) \
    CoroutineSpinlockWrapper scoped_spin_lock_wrapper((coroutine_spin_lock)); \
    RUN_COROUTINE((coroutine_spin_lock).lock);
