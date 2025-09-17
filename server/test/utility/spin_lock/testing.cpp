//
// Created by jeremiah on 7/20/22.
//
#include <utility_general_functions.h>
#include <general_values.h>
#include <fstream>
#include <collection_names.h>
#include <reports_objects.h>
#include "gtest/gtest.h"

#include "coroutine_spin_lock.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SpinLockTests : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    static ThreadPoolSuspend lockAroundCoroutine(
            CoroutineSpinlock& spin_lock,
            const std::chrono::milliseconds& sleep_time,
            const std::string& start,
            const std::string& end,
            tbb::concurrent_vector<std::string>& concurrent_vector
            ) {
        SCOPED_SPIN_LOCK(spin_lock);
        concurrent_vector.emplace_back(start);
        std::this_thread::sleep_for(sleep_time);
        concurrent_vector.emplace_back(end);
        co_return;
    }

    static void lockAroundFunction(
            CoroutineSpinlock& spin_lock,
            const std::chrono::milliseconds& sleep_time,
            const std::string& start,
            const std::string& end,
            tbb::concurrent_vector<std::string>& concurrent_vector
            ) {
        SpinlockWrapper lock(spin_lock);
        concurrent_vector.emplace_back(start);
        std::this_thread::sleep_for(sleep_time);
        concurrent_vector.emplace_back(end);
    }
};

//Start normal thread first, make sure it blocks coroutine thread.
TEST_F(SpinLockTests, normalDelaysCoroutine) {

    CoroutineSpinlock spin_lock;

    const std::string function_start = "f_s";
    const std::string function_end = "f_e";
    const std::string coroutine_start = "c_s";
    const std::string coroutine_end = "c_e";

    tbb::concurrent_vector<std::string> concurrent_vector;

    const std::chrono::milliseconds normal_sleep_time{100};
    const std::chrono::milliseconds coroutine_sleep_time{5};

    std::jthread normal_thread([&]{
        lockAroundFunction(
                spin_lock,
                normal_sleep_time,
                function_start,
                function_end,
                concurrent_vector
                );
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    std::jthread coroutine_thread([&]{
        auto handle = lockAroundCoroutine(
                spin_lock,
                coroutine_sleep_time,
                coroutine_start,
                coroutine_end,
                concurrent_vector
                ).handle;
        while(!handle.done()) {
            handle.resume();
        }
        handle.destroy();
    });

    normal_thread.join();
    coroutine_thread.join();

    EXPECT_EQ(concurrent_vector.size(), 4);
    if(concurrent_vector.size() == 4) {
        EXPECT_EQ(concurrent_vector[0], function_start);
        EXPECT_EQ(concurrent_vector[1], function_end);
        EXPECT_EQ(concurrent_vector[2], coroutine_start);
        EXPECT_EQ(concurrent_vector[3], coroutine_end);
    }
}

//Start coroutine thread first, make sure it blocks normal thread.
TEST_F(SpinLockTests, coroutineDelaysNormal) {

    CoroutineSpinlock spin_lock;

    const std::string function_start = "f_s";
    const std::string function_end = "f_e";
    const std::string coroutine_start = "c_s";
    const std::string coroutine_end = "c_e";

    tbb::concurrent_vector<std::string> concurrent_vector;

    const std::chrono::milliseconds coroutine_sleep_time{100};
    const std::chrono::milliseconds normal_sleep_time{5};

    std::jthread coroutine_thread([&]{
        auto handle = lockAroundCoroutine(
                spin_lock,
                coroutine_sleep_time,
                coroutine_start,
                coroutine_end,
                concurrent_vector
                ).handle;
        while(!handle.done()) {
            handle.resume();
        }
        handle.destroy();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    std::jthread normal_thread([&]{
        lockAroundFunction(
                spin_lock,
                normal_sleep_time,
                function_start,
                function_end,
                concurrent_vector
                );
    });

    normal_thread.join();
    coroutine_thread.join();

    EXPECT_EQ(concurrent_vector.size(), 4);
    if(concurrent_vector.size() == 4) {
        EXPECT_EQ(concurrent_vector[0], coroutine_start);
        EXPECT_EQ(concurrent_vector[1], coroutine_end);
        EXPECT_EQ(concurrent_vector[2], function_start);
        EXPECT_EQ(concurrent_vector[3], function_end);
    }
}