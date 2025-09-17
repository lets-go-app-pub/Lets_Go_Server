//
// Created by jeremiah on 8/30/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include "../generate_matching_users.h"
#include "user_account_keys.h"
#include "algorithm_pipeline_field_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GenerateAddFieldsInitializeAndCleanTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc match_account_doc;

    UserAccountValues user_account_values;

    bsoncxx::builder::basic::array user_gender_range_builder;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        ASSERT_TRUE(
                generateMatchingUsers(
                        user_account_doc,
                        match_account_doc,
                        user_account_oid,
                        match_account_oid,
                        user_account_values,
                        user_gender_range_builder
                )
        );

        const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{250};
        const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{300};
        const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{650};
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void compareResults(
            const bsoncxx::document::view& categories_doc,
            const long total_time,
            const std::vector<TestTimeframe>& generated_time_frames
            ) {

        bsoncxx::builder::stream::document query_doc;

        mongocxx::pipeline pipeline;

        query_doc
                << "_id" << match_account_oid;

        pipeline.match(query_doc.view());

        pipeline.add_fields(categories_doc);

        generateAddFieldsInitializeAndCleanTimeFramesPipelineStage(
                &pipeline,
                user_account_values
        );

        auto cursor_result = user_accounts_collection.aggregate(pipeline);

        int num_results = 0;
        for(const auto& x : cursor_result) {

            int num_activities = 0;
            bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
            for(const auto& activity : activities_array) {
                bsoncxx::document::view activity_doc = activity.get_document();

                EXPECT_EQ(
                    total_time,
                    activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH].get_int64().value
                );

                bsoncxx::array::view time_frames_array = activity_doc[user_account_keys::categories::TIMEFRAMES].get_array();

                std::vector<TestTimeframe> extracted_time_frames;
                for(const auto& time_frame : time_frames_array) {
                    bsoncxx::document::view time_frame_doc = time_frame.get_document();

                    extracted_time_frames.emplace_back(
                            time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value,
                            time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value
                            );
                }

                EXPECT_EQ(generated_time_frames.size(), extracted_time_frames.size());
                if(generated_time_frames.size() == extracted_time_frames.size()) {
                    for(int i = 0; i < (int)generated_time_frames.size(); ++i) {
                        EXPECT_EQ(generated_time_frames[i], extracted_time_frames[i]);
                    }
                }

                num_activities++;
            }

            num_results++;
            EXPECT_EQ(num_activities, 1);
        }

        EXPECT_EQ(num_results, 1);
    }

};

TEST_F(GenerateAddFieldsInitializeAndCleanTesting, emptyTimeFramesArray) {

    bsoncxx::builder::stream::document categories_doc{};

    categories_doc
            << user_account_keys::CATEGORIES << open_array
                << open_document
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 0
                    << user_account_keys::categories::TIMEFRAMES << open_array
                    << close_array
                << close_document
            << close_array;

    std::vector<TestTimeframe> generated_time_frames;

    generated_time_frames.emplace_back(
            user_account_values.earliest_time_frame_start_timestamp.count() + 1,
            1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc.view(),
            (user_account_values.end_of_time_frame_timestamp - user_account_values.earliest_time_frame_start_timestamp).count(),
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsInitializeAndCleanTesting, startTimeBelowEarliest_stopTimeBelowEarliest) {
    bsoncxx::builder::stream::document categories_doc{};

    categories_doc
            << user_account_keys::CATEGORIES << open_array
                << open_document
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 0
                    << user_account_keys::categories::TIMEFRAMES << open_array
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() - 100
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() - 50
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                    << close_array
                << close_document
            << close_array;

    std::vector<TestTimeframe> generated_time_frames;

    generated_time_frames.emplace_back(
            user_account_values.earliest_time_frame_start_timestamp.count() + 1,
            1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc.view(),
            (user_account_values.end_of_time_frame_timestamp - user_account_values.earliest_time_frame_start_timestamp).count(),
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsInitializeAndCleanTesting, startTimeBelowEarliest_stopTimeAboveEarliest) {

    const long stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 50;

    bsoncxx::builder::stream::document categories_doc{};

    categories_doc
            << user_account_keys::CATEGORIES << open_array
                << open_document
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 0
                    << user_account_keys::categories::TIMEFRAMES << open_array
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() - 100
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                    << close_array
                << close_document
            << close_array;

    std::vector<TestTimeframe> generated_time_frames;

    generated_time_frames.emplace_back(
            user_account_values.earliest_time_frame_start_timestamp.count() + 1,
            1
    );

    generated_time_frames.emplace_back(
            stop_time,
            -1
    );

    compareResults(
            categories_doc.view(),
            stop_time - user_account_values.earliest_time_frame_start_timestamp.count(),
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsInitializeAndCleanTesting, startTimeAboveEarliest_stopTimeAboveEarliest) {
    const long start_time = user_account_values.earliest_time_frame_start_timestamp.count() + 50;
    const long stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 100;

    bsoncxx::builder::stream::document categories_doc{};

    categories_doc
            << user_account_keys::CATEGORIES << open_array
                << open_document
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 0
                    << user_account_keys::categories::TIMEFRAMES << open_array
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                    << close_array
                << close_document
            << close_array;

    std::vector<TestTimeframe> generated_time_frames;

    generated_time_frames.emplace_back(
            start_time,
            1
    );

    generated_time_frames.emplace_back(
            stop_time,
            -1
    );

    compareResults(
            categories_doc.view(),
            stop_time - start_time,
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsInitializeAndCleanTesting, multipleTimeFrames) {
    const long start_time_one = user_account_values.earliest_time_frame_start_timestamp.count() + 25;
    const long stop_time_one = user_account_values.earliest_time_frame_start_timestamp.count() + 50;

    const long start_time_two = user_account_values.earliest_time_frame_start_timestamp.count() + 100;
    const long stop_time_two = user_account_values.earliest_time_frame_start_timestamp.count() + 150;

    const long start_time_three = user_account_values.earliest_time_frame_start_timestamp.count() + 200;
    const long stop_time_three = user_account_values.earliest_time_frame_start_timestamp.count() + 300;

    bsoncxx::builder::stream::document categories_doc{};

    categories_doc
            << user_account_keys::CATEGORIES << open_array
                << open_document
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 0
                    << user_account_keys::categories::TIMEFRAMES << open_array
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << start_time_one
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << stop_time_one
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << start_time_two
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << stop_time_two
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << start_time_three
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << stop_time_three
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                    << close_array
                << close_document
            << close_array;

    std::vector<TestTimeframe> generated_time_frames;

    generated_time_frames.emplace_back(
            start_time_one,
            1
    );

    generated_time_frames.emplace_back(
            stop_time_one,
            -1
    );

    generated_time_frames.emplace_back(
            start_time_two,
            1
    );

    generated_time_frames.emplace_back(
            stop_time_two,
            -1
    );

    generated_time_frames.emplace_back(
            start_time_three,
            1
    );

    generated_time_frames.emplace_back(
            stop_time_three,
            -1
    );

    compareResults(
            categories_doc.view(),
            (stop_time_three - start_time_three) + (stop_time_two - start_time_two) + (stop_time_one - start_time_one),
            generated_time_frames
    );
}
