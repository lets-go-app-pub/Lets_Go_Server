//
// Created by jeremiah on 7/20/22.
//
#include <utility_general_functions.h>
#include <fstream>
#include <collection_names.h>
#include <reports_objects.h>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <clear_database_for_testing.h>
#include "gtest/gtest.h"

#include "thread_pool_global_variable.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ThreadPoolTests : public ::testing::Test {
protected:

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    ThreadPoolSuspend simpleCoroutine(bool& successful) {
        successful = true;
        co_return;
    }

    ThreadPoolSuspend childCoroutine(bool& child_finished) {
        for (int i = 0; i < 10e9; ++i) {
            SUSPEND();
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        child_finished = true;
        co_return;
    }

    ThreadPoolSuspend parentCoroutine(
            bool& parent_finished,
            bool& child_finished
    ) {
        RUN_COROUTINE(childCoroutine, child_finished);
        parent_finished = true;
        co_return;
    }

    ThreadPoolSuspend exceptionChildCoroutine() {
        std::string().at(1); // This generates an 'std::out_of_range'.
        co_return;
    }

    ThreadPoolSuspend exceptionCoroutine(bool& finished) {
        //making sure that RUN_COROUTINE properly propagates the exception
        RUN_COROUTINE(exceptionChildCoroutine);
        finished = true;
        co_return;
    }

    const int num_times_to_loop = 15;

    ThreadPoolSuspend suspendChildCoroutine(
            int index,
            tbb::concurrent_vector<int>& vec
            ) {
        vec.emplace_back(index);
        SUSPEND();
    }

    ThreadPoolSuspend suspendCoroutine(
            int index,
            tbb::concurrent_vector<int>& vec
            ) {
        //sleep while thread pool initializes
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        for(int i = 0; i < num_times_to_loop; i++) {
            //making sure RUN_COROUTINE properly handles child coroutines suspending
            RUN_COROUTINE(suspendChildCoroutine, index, vec);
        }

        co_return;
    }
};

TEST_F(ThreadPoolTests, submit_lValue) {
    CoroutineThreadPool testing_thread_pool;
    bool function_ran = false;

    auto lambda = [&] {
        function_ran = true;
    };

    testing_thread_pool.submit(lambda);

    //allow thread time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_TRUE(function_ran);
}

TEST_F(ThreadPoolTests, submit_rValue) {
    CoroutineThreadPool testing_thread_pool;
    bool function_ran = false;

    testing_thread_pool.submit([&] {
        function_ran = true;
    });

    //allow thread time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_TRUE(function_ran);
}

TEST_F(ThreadPoolTests, submitCoroutine) {
    CoroutineThreadPool testing_thread_pool;
    bool function_ran = false;

    testing_thread_pool.submit_coroutine(simpleCoroutine(function_ran).handle);

    //allow thread time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_TRUE(function_ran);
}

TEST_F(ThreadPoolTests, cleanupChildCoroutines) {
    CoroutineThreadPool testing_thread_pool;
    bool parent_coroutine_finished = false;
    bool child_coroutine_finished = false;

    testing_thread_pool.submit_coroutine(parentCoroutine(parent_coroutine_finished, child_coroutine_finished).handle);

    //allow thread time to begin
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_FALSE(parent_coroutine_finished);
    EXPECT_FALSE(child_coroutine_finished);

    {
        bool empty = true;
        while(empty) {
            testing_thread_pool.thread_loop_mutex.lock();
            empty = testing_thread_pool.jobs.empty();
            static const timespec ns = { 0,1 }; //1 nanosecond
            nanosleep(&ns, nullptr);
            if(empty) {
                testing_thread_pool.thread_loop_mutex.unlock();
            }
        }
        EXPECT_EQ(testing_thread_pool.jobs.size(), 1);

        if(!testing_thread_pool.jobs.empty()) {
            //the child coroutine handle should be saved to allow it to be destroyed
            auto& reference = testing_thread_pool.jobs.front();
            EXPECT_EQ(reference->child_coroutine_handles.size(), 1);
        }

        testing_thread_pool.thread_loop_mutex.unlock();
    }

    testing_thread_pool.stop_pool();

    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    //neither coroutine ever should ever reach the co_return
    EXPECT_FALSE(parent_coroutine_finished);
    EXPECT_FALSE(child_coroutine_finished);
}

TEST_F(ThreadPoolTests, catchException) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection errors_collection = errors_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    auto num_docs = errors_collection.count_documents(document{} << finalize);

    EXPECT_EQ(num_docs, 0);

    CoroutineThreadPool testing_thread_pool;
    bool function_ran = false;

    testing_thread_pool.submit_coroutine(exceptionCoroutine(function_ran).handle);

    //allow coroutine time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_FALSE(function_ran);
    EXPECT_EQ(testing_thread_pool.num_jobs_outstanding(), 0);

    num_docs = errors_collection.count_documents(document{} << finalize);

    //exception should be stored
    EXPECT_EQ(num_docs, 1);
}

TEST_F(ThreadPoolTests, suspendProperlyCycles) {
    CoroutineThreadPool testing_thread_pool(1);
    tbb::concurrent_vector<int> vec;

    const int num_coroutines_to_push = 12;

    for(int i = 0; i < num_coroutines_to_push; i++) {
        testing_thread_pool.submit_coroutine(suspendCoroutine(i, vec).handle);
    }

    //allow thread time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds{num_coroutines_to_push*10 + 150});

    EXPECT_EQ(testing_thread_pool.num_jobs_outstanding(), 0);

    EXPECT_EQ(vec.size(), num_coroutines_to_push*num_times_to_loop);
    if(testing_thread_pool.num_jobs_outstanding() == 0
        && (int)vec.size() == num_coroutines_to_push*num_times_to_loop) {
        for(int i = 0; i < num_times_to_loop; ++i) {
            for(int j = 0; j < num_coroutines_to_push; ++j) {
                EXPECT_EQ(vec[num_coroutines_to_push*i+j], j);
            }
        }
    }
}
