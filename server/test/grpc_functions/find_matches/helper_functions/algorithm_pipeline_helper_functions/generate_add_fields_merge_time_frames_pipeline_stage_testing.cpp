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

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GenerateAddFieldsMergeTimeFramesTesting : public ::testing::Test {
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

        const_cast<std::chrono::milliseconds &>(user_account_values.current_timestamp) = std::chrono::milliseconds{250};
        const_cast<std::chrono::milliseconds &>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{300};
        const_cast<std::chrono::milliseconds &>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{650};

        user_account_values.user_activities.front().totalTime = std::chrono::milliseconds{
                user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1
        };

        //set up user account values to have anytime time frames
        user_account_values.user_activities.front().timeFrames.clear();

        user_account_values.user_activities.front().timeFrames.emplace_back(
                user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{1},
                1
        );

        user_account_values.user_activities.front().timeFrames.emplace_back(
                user_account_values.end_of_time_frame_timestamp,
                -1
        );

        user_account_values.user_categories.front().timeFrames.clear();

        user_account_values.user_categories.front().timeFrames.emplace_back(
                user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{1},
                1
        );

        user_account_values.user_categories.front().timeFrames.emplace_back(
                user_account_values.end_of_time_frame_timestamp,
                -1
        );
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void compareResults(
            const bsoncxx::document::view& categories_doc,
            const std::vector<TestTimeframe>& generated_time_frames
    ) {

        bsoncxx::builder::stream::document query_doc;

        mongocxx::pipeline pipeline;

        query_doc
                << "_id" << match_account_oid;

        pipeline.match(query_doc.view());

        pipeline.add_fields(categories_doc);

        generateAddFieldsMergeTimeFramesPipelineStage(
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

TEST_F(GenerateAddFieldsMergeTimeFramesTesting, defaultTimeFramesUsed) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                << user_account_keys::categories::INDEX_VALUE << 0
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
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
            user_account_values.earliest_time_frame_start_timestamp.count() + 1,
            1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc,
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsMergeTimeFramesTesting, matchingByCategoriesOnly) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::CATEGORY_TYPE
                << user_account_keys::categories::INDEX_VALUE << 0
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
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
            user_account_values.earliest_time_frame_start_timestamp.count() + 1,
            1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc,
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsMergeTimeFramesTesting, noMatchForActivity) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                << user_account_keys::categories::INDEX_VALUE << 22
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
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
            user_account_values.earliest_time_frame_start_timestamp.count() + 1,
            1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc,
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsMergeTimeFramesTesting, userTimeFrameArraySize_largerThan_matchTimeFrameArraySize) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const long first_start_time = user_account_values.earliest_time_frame_start_timestamp.count() + 50;
    const long first_stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 100;
    const long second_start_time = user_account_values.earliest_time_frame_start_timestamp.count() + 150;
    const long second_stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 200;

    user_account_values.user_activities.front().totalTime = std::chrono::milliseconds{
        (second_stop_time - second_start_time) + (first_stop_time - first_start_time)
    };

    user_account_values.user_activities.front().timeFrames.clear();

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{first_start_time},
            1
    );

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{first_stop_time},
            -1
    );

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{second_start_time},
            1
    );

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{second_stop_time},
            -1
    );

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                << user_account_keys::categories::INDEX_VALUE << 0
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
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
            first_start_time,
            1
    );

    generated_time_frames.emplace_back(
            first_stop_time,
            -1
    );

    generated_time_frames.emplace_back(
            second_start_time,
            1
    );

    generated_time_frames.emplace_back(
            second_stop_time,
            -1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc,
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsMergeTimeFramesTesting, matchTimeFrameArraySize_largerThan_userTimeFrameArraySize) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const long first_start_time = user_account_values.earliest_time_frame_start_timestamp.count() + 50;
    const long first_stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 100;
    const long second_start_time = user_account_values.earliest_time_frame_start_timestamp.count() + 150;
    const long second_stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 200;

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                << user_account_keys::categories::INDEX_VALUE << 0
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << first_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << first_stop_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << second_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << second_stop_time
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
            first_start_time,
            1
    );

    generated_time_frames.emplace_back(
            first_stop_time,
            -1
    );

    generated_time_frames.emplace_back(
            second_start_time,
            1
    );

    generated_time_frames.emplace_back(
            second_stop_time,
            -1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc,
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsMergeTimeFramesTesting, emptyActivityTimeFrame) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    user_account_values.user_activities.clear();

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                << user_account_keys::categories::INDEX_VALUE << 0
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
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
            user_account_values.earliest_time_frame_start_timestamp.count() + 1,
            1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc,
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsMergeTimeFramesTesting, activityStartStopValuesInvalid) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    //Each time frame should come in pair and include a 1 and a -1. This has two 1s.
    user_account_values.user_activities.front().timeFrames.clear();

    user_account_values.user_activities.front().timeFrames.emplace_back(
            user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{1},
            1
    );

    user_account_values.user_activities.front().timeFrames.emplace_back(
            user_account_values.end_of_time_frame_timestamp,
            1
    );

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                << user_account_keys::categories::INDEX_VALUE << 0
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
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
            user_account_values.earliest_time_frame_start_timestamp.count() + 1,
            1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc,
            generated_time_frames
    );
}

TEST_F(GenerateAddFieldsMergeTimeFramesTesting, activityInvalidTimeframesSize) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    //Each time frame should come in pair and include a 1 and a -1. This does not include the -1.
    user_account_values.user_activities.front().timeFrames.clear();
    user_account_values.user_activities.front().timeFrames.emplace_back(
            user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{1},
            1
    );

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                << user_account_keys::categories::INDEX_VALUE << 0
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
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
            user_account_values.earliest_time_frame_start_timestamp.count() + 1,
            1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    generated_time_frames.emplace_back(
            user_account_values.end_of_time_frame_timestamp.count(),
            -1
    );

    compareResults(
            categories_doc,
            generated_time_frames
    );
}
