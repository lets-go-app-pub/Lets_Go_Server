//
// Created by jeremiah on 5/16/21.
//
#pragma once

#include <vector>
#include <mutex>
#include <shared_mutex>
#include <coroutine_spin_lock.h>

template<typename T>
class ThreadSafeWritesWaitingToProcessVector {
    //Spin lock seems to outperform a mutex slightly in simple situations (based on small_stress_test).
    CoroutineSpinlock spin_lock;
    std::vector<T> vector;
public:

    void concat_vector_to_front_without_lock(std::vector<T>& passed_vector) {
        if(!passed_vector.empty()) {
            vector.insert(vector.begin(), std::make_move_iterator(passed_vector.begin()), std::make_move_iterator(passed_vector.end()));
        }
    }

    void concat_vector_to_index_one_without_lock(std::vector<T>& passed_vector) {
        if(!passed_vector.empty()) {
            vector.insert(vector.begin()+!vector.empty(), std::make_move_iterator(passed_vector.begin()), std::make_move_iterator(passed_vector.end()));
        }
    }

    size_t push_and_size_no_coroutine(T&& val, const std::function<void()>& run_while_locked) {
        SpinlockWrapper lock(spin_lock);
        if(run_while_locked)
            run_while_locked();
        vector.emplace_back(std::move(val));
        return vector.size();
    }

    //NOTE: Passing a value that contains a lambda is a bit tricky, if the function calls
    // a SUSPEND, any references to temporaries will reference garbage. And rvalues don't
    // seem to be stored in general. It can be done (like it is here) using pass by value.
    // However, this means that a copy must be made every time. There is also the chance that
    // the compiler could optimize out the pass by value. Instead, push_and_size_no_lock
    // is used most (all) of the time.
    size_t push_and_size_no_lock(T&& val) {
        vector.emplace_back(std::move(val));
        return vector.size();
    }

    ThreadPoolSuspend pop_front(bool& successful) {
        SCOPED_SPIN_LOCK(spin_lock);
        if(vector.empty()) {
            successful = false;
            co_return;
        }
        vector.erase(vector.begin());
        successful = true;
        co_return;
    }

    ThreadPoolSuspend pop_front_and_empty(bool& empty) {
        SCOPED_SPIN_LOCK(spin_lock);
        if(vector.empty()) {
            empty = true;
            co_return;
        }
        vector.erase(vector.begin());
        empty = vector.empty();
        co_return;
    }

    ThreadPoolSuspend pop_front_and_get_next(bool& empty, std::shared_ptr<T>& val) {
        SCOPED_SPIN_LOCK(spin_lock);
        if(vector.empty()) {
            empty = true;
            co_return;
        }
        vector.erase(vector.begin());
        empty = vector.empty();
        if(!empty) {
            val = std::make_shared<T>(vector.front());
        } else {
            val = nullptr;
        };
        co_return;
    }

    ThreadPoolSuspend front_if_not_empty(std::shared_ptr<T>& val) {
        SCOPED_SPIN_LOCK(spin_lock);
        val = vector.empty() ? nullptr : std::make_shared<T>(vector.front());
        co_return;
    }

    /** Only use for testing, not locked. **/
    auto size_for_testing() {
        return vector.size();
    }

    CoroutineSpinlock& getSpinLock() {
        return spin_lock;
    }

#ifdef LG_TESTING
private:

    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, concat_vector_to_front_without_lock);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, concat_vector_to_index_one_without_lock);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, push_and_size_no_coroutine);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, push_and_size_no_lock);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, pop_front);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_empty_lastElement);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_empty_middleCase);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_get_next_empty);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_get_next_lastElement);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_get_next_middleCase);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, front_if_not_empty_emptyVector);
    FRIEND_TEST(ThreadSafeWritesWaitingToProcessVectorTests, front_if_not_empty_notEmpty);

    FRIEND_TEST(IterateAndSendMessagesToTargetUsersTests, joinMessageInFront);
    FRIEND_TEST(IterateAndSendMessagesToTargetUsersTests, leaveMessageInFront);
    FRIEND_TEST(IterateAndSendMessagesToTargetUsersTests, multipleMessageInFront);

    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, noMessagesToRequest);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, requestNormalMessage);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, requestkDifferentUserJoinedMessage);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, inviteForCurrentUserIsPassedThrough);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, inviteForOtherUserDoesNotPassThrough);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, deleteForAllIsPassedThrough);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, deleteForCurrentUserIsPassedThrough);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, deleteForOtherUserDoesNotPassThrough);

#endif
};