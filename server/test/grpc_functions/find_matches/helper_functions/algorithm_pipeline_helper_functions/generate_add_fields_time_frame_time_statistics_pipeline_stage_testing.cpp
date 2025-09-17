//
// Created by jeremiah on 8/31/22.
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
#include "utility_general_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GenerateAddFieldsTimeFrameTimeStatisticsTesting : public ::testing::Test {
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

        user_account_values.user_categories.front().totalTime = std::chrono::milliseconds{
                user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1
        };

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

    static void compareResults(
            const bsoncxx::document::view& activity_doc,
            const int expected_activity_index_value,
            const AccountCategoryType expected_activity_type,
            const long expected_total_match_time,
            const long expected_total_user_time,
            const long expected_overlap_time,
            const long expected_match_expiration_time,
            const bool between_times_array_empty = true
    ) {

        auto index = activity_doc[user_account_keys::categories::INDEX_VALUE].get_int32().value;
        AccountCategoryType type{activity_doc[user_account_keys::categories::TYPE].get_int32().value};
        long total_match_time = activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH].get_int64().value;
        long total_user_time = activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR].get_int64().value;
        long overlap_time = activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR].get_int64().value;
        long match_expiration_time = activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR].get_int64().value;

        EXPECT_EQ(expected_activity_index_value, index);
        EXPECT_EQ(expected_activity_type, type);

        EXPECT_EQ(total_match_time, expected_total_match_time);
        EXPECT_EQ(total_user_time, expected_total_user_time);
        EXPECT_EQ(overlap_time, expected_overlap_time);

        if(between_times_array_empty) {
            bsoncxx::array::view between_times_array = activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR].get_array().value;
            EXPECT_TRUE(between_times_array.empty());
        }

        EXPECT_EQ(match_expiration_time, expected_match_expiration_time);
    }

};

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, anytimeTimeFrames_activityOnly) {

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    //This is simply set to guarantee it isn't used as the final value.
    user_account_values.user_categories.front().totalTime = std::chrono::milliseconds {-1};

    const AccountCategoryType activity_type = AccountCategoryType::ACTIVITY_TYPE;
    const int activity_index_value = 0;

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << activity_type
                << user_account_keys::categories::INDEX_VALUE << activity_index_value
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1}
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                << close_array
            << close_document
        << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
            << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                    user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                    user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                    user_account_values.end_of_time_frame_timestamp.count()
            );

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, anytimeTimeFrames_categoryOnly) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    //This is simply set to guarantee it isn't used as the final value.
    user_account_values.user_activities.front().totalTime = std::chrono::milliseconds {-1};

    const AccountCategoryType activity_type = AccountCategoryType::CATEGORY_TYPE;
    const int activity_index_value = 0;

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << activity_type
                << user_account_keys::categories::INDEX_VALUE << activity_index_value
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1}
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                << close_array
            << close_document
        << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
            << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                    user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                    user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                    user_account_values.end_of_time_frame_timestamp.count()
            );

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, anytimeTimeFrames_categoryAndActivity) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    const int activity_index_value = 0;
    const int category_index_value = 0;

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::CATEGORY_TYPE
                << user_account_keys::categories::INDEX_VALUE << category_index_value
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1}
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                << close_array
            << close_document
            << open_document
                << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                << user_account_keys::categories::INDEX_VALUE << activity_index_value
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1}
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                << close_array
            << close_document
        << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
            << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            if(num_activities == 0) {
                compareResults(
                        activity_doc,
                        category_index_value,
                        AccountCategoryType::CATEGORY_TYPE,
                        user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                        user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                        user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                        user_account_values.end_of_time_frame_timestamp.count()
                );
            } else {
                compareResults(
                        activity_doc,
                        activity_index_value,
                        AccountCategoryType::ACTIVITY_TYPE,
                        user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                        user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                        user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count() - 1,
                        user_account_values.end_of_time_frame_timestamp.count()
                );
            }

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 2);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, noBetweenTimes_with_noOverlappingTimes) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const AccountCategoryType activity_type = AccountCategoryType::ACTIVITY_TYPE;
    const int activity_index_value = 0;

    //make sure end of time is AFTER max between times values
    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{100};
    const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{150};
    const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{user_account_values.earliest_time_frame_start_timestamp.count() + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count() * 2};

    //no 'between times', no 'overlapping times'
    const long match_start_time = user_account_values.earliest_time_frame_start_timestamp.count();
    const long match_stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 100;
    const long user_start_time = user_account_values.end_of_time_frame_timestamp.count() - 100;
    const long user_stop_time = user_account_values.end_of_time_frame_timestamp.count();

    user_account_values.user_activities.front().timeFrames.clear();

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{user_start_time},
            1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{user_stop_time},
            -1
    );

    user_account_values.user_activities.front().totalTime = std::chrono::milliseconds{
            user_stop_time - user_start_time
    };

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << activity_type
                << user_account_keys::categories::INDEX_VALUE << activity_index_value
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{match_stop_time - match_start_time}
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << match_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << match_stop_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_stop_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                << close_array
            << close_document
        << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
            << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    match_stop_time - match_start_time,
                    user_stop_time - user_start_time,
                    0,
                    user_account_values.end_of_time_frame_timestamp.count()
            );

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, betweenTimes_with_noOverlappingTimes) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const AccountCategoryType activity_type = AccountCategoryType::ACTIVITY_TYPE;
    const int activity_index_value = 0;

    //make sure end of time is AFTER max between times values
    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{100};
    const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{150};
    const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{user_account_values.earliest_time_frame_start_timestamp.count() + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count() * 2};

    //'between times', no 'overlapping times'
    const long match_start_time = user_account_values.earliest_time_frame_start_timestamp.count();
    const long match_stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 100;
    const long user_start_time = match_stop_time + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count() - 1;
    const long user_stop_time = user_start_time + 100;

    user_account_values.user_activities.front().timeFrames.clear();

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{user_start_time},
            1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{user_stop_time},
            -1
    );

    user_account_values.user_activities.front().totalTime = std::chrono::milliseconds{
            user_stop_time - user_start_time
    };

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << activity_type
                << user_account_keys::categories::INDEX_VALUE << activity_index_value
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{match_stop_time - match_start_time}
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << match_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << match_stop_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_stop_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                << close_array
            << close_document
        << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
            << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    match_stop_time - match_start_time,
                    user_stop_time - user_start_time,
                    0,
                    match_stop_time,
                    false
            );

            bsoncxx::array::view between_times_array = activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR].get_array().value;

            int num_between_time_elements = 0;
            for(const auto& ele : between_times_array) {
                EXPECT_EQ(user_start_time - match_stop_time, ele.get_int64().value);
                num_between_time_elements++;
            }
            EXPECT_EQ(num_between_time_elements, 1);

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, noBetweenTimes_with_overlappingTimes) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const AccountCategoryType activity_type = AccountCategoryType::ACTIVITY_TYPE;
    const int activity_index_value = 0;

    //no 'between times', 'overlapping times'
    const long match_start_time = user_account_values.earliest_time_frame_start_timestamp.count();
    const long match_stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 100;
    const long user_start_time = match_start_time + 60;
    const long user_stop_time = user_start_time + 120;

    user_account_values.user_activities.front().timeFrames.clear();

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{user_start_time},
            1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{user_stop_time},
            -1
    );

    user_account_values.user_activities.front().totalTime = std::chrono::milliseconds{
            user_stop_time - user_start_time
    };

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << activity_type
                << user_account_keys::categories::INDEX_VALUE << activity_index_value
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{match_stop_time - match_start_time}
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << match_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << match_stop_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_stop_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                << close_array
            << close_document
        << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
            << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    match_stop_time - match_start_time,
                    user_stop_time - user_start_time,
                    match_stop_time - user_start_time,
                    match_stop_time
            );

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, betweenTimes_with_overlappingTimes) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const AccountCategoryType activity_type = AccountCategoryType::ACTIVITY_TYPE;
    const int activity_index_value = 0;

    //make sure end of time is AFTER max between times values
    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{100};
    const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{150};
    const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{user_account_values.earliest_time_frame_start_timestamp.count() + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count() * 2};

    //'between times' and 'overlapping times'
    const long overlapping_match_start_time = user_account_values.earliest_time_frame_start_timestamp.count();
    const long overlapping_match_stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 100;
    const long overlapping_user_start_time = overlapping_match_start_time + 60;
    const long overlapping_user_stop_time = overlapping_user_start_time + 120;

    const long between_match_start_time = overlapping_user_stop_time + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count();
    const long between_match_stop_time = between_match_start_time + 100;
    const long between_user_start_time = between_match_stop_time + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count()/10;
    const long between_user_stop_time = between_user_start_time + 100;

    user_account_values.user_activities.front().timeFrames.clear();

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{overlapping_user_start_time},
            1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{overlapping_user_stop_time},
            -1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{between_user_start_time},
            1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{between_user_stop_time},
            -1
    );

    user_account_values.user_activities.front().totalTime = std::chrono::milliseconds{
            (overlapping_user_stop_time - overlapping_user_start_time) + (between_user_stop_time - between_user_start_time)
    };

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
            << user_account_keys::CATEGORIES << open_array
                << open_document
                    << user_account_keys::categories::TYPE << activity_type
                    << user_account_keys::categories::INDEX_VALUE << activity_index_value
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{(overlapping_match_stop_time - overlapping_match_start_time) + (between_match_stop_time - between_match_start_time)}
                    << user_account_keys::categories::TIMEFRAMES << open_array
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << overlapping_match_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << overlapping_user_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << overlapping_match_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << overlapping_user_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << between_match_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << between_match_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << between_user_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << between_user_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                    << close_array
                << close_document
            << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
                    << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    (overlapping_match_stop_time - overlapping_match_start_time) + (between_match_stop_time - between_match_start_time),
                    (overlapping_user_stop_time - overlapping_user_start_time) + (between_user_stop_time - between_user_start_time),
                    overlapping_match_stop_time - overlapping_user_start_time,
                    between_match_stop_time,
                    false
            );

            bsoncxx::array::view between_times_array = activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR].get_array().value;

            int num_between_time_elements = 0;
            for(const auto& ele : between_times_array) {
                EXPECT_EQ(between_user_start_time - between_match_stop_time, ele.get_int64().value);
                num_between_time_elements++;
            }
            EXPECT_EQ(num_between_time_elements, 1);

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, betweenTimes_stopTimeMatchesStartTime) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const AccountCategoryType activity_type = AccountCategoryType::ACTIVITY_TYPE;
    const int activity_index_value = 0;

    //make sure end of time is AFTER max between times values
    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{100};
    const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{150};
    const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{user_account_values.earliest_time_frame_start_timestamp.count() + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count() * 2};

    //'between times', no 'overlapping times'
    const long match_start_time = user_account_values.earliest_time_frame_start_timestamp.count();
    const long match_stop_time = user_account_values.earliest_time_frame_start_timestamp.count() + 100;
    const long user_start_time = match_stop_time;
    const long user_stop_time = user_start_time + 100;

    user_account_values.user_activities.front().timeFrames.clear();

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{user_start_time},
            1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{user_stop_time},
            -1
    );

    user_account_values.user_activities.front().totalTime = std::chrono::milliseconds{
            user_stop_time - user_start_time
    };

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
        << user_account_keys::CATEGORIES << open_array
            << open_document
                << user_account_keys::categories::TYPE << activity_type
                << user_account_keys::categories::INDEX_VALUE << activity_index_value
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{match_stop_time - match_start_time}
                << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << match_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << match_stop_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_start_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                    << open_document
                        << user_account_keys::categories::timeframes::TIME << user_stop_time
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << close_document
                << close_array
            << close_document
        << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
            << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    match_stop_time - match_start_time,
                    user_stop_time - user_start_time,
                    0,
                    match_stop_time,
                    false
            );

            bsoncxx::array::view between_times_array = activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR].get_array().value;

            int num_between_time_elements = 0;
            for(const auto& ele : between_times_array) {
                EXPECT_EQ(user_start_time - match_stop_time, ele.get_int64().value);
                num_between_time_elements++;
            }
            EXPECT_EQ(num_between_time_elements, 1);

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, multipleOverlappingTimes) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const AccountCategoryType activity_type = AccountCategoryType::ACTIVITY_TYPE;
    const int activity_index_value = 0;

    //'between times' and 'overlapping times'
    const long first_match_start_time = user_account_values.earliest_time_frame_start_timestamp.count() + 2;
    const long first_match_stop_time = first_match_start_time + 20;
    const long second_match_start_time = first_match_stop_time + 40;
    const long second_match_stop_time = second_match_start_time + 50;

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
            << user_account_keys::CATEGORIES << open_array
                << open_document
                    << user_account_keys::categories::TYPE << activity_type
                    << user_account_keys::categories::INDEX_VALUE << activity_index_value
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{(first_match_stop_time - first_match_start_time) + (second_match_stop_time - second_match_start_time)}
                    << user_account_keys::categories::TIMEFRAMES << open_array
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << user_account_values.earliest_time_frame_start_timestamp.count() + 1
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document

                        << open_document
                            << user_account_keys::categories::timeframes::TIME << first_match_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << first_match_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << second_match_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << second_match_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document

                        << open_document
                            << user_account_keys::categories::timeframes::TIME << user_account_values.end_of_time_frame_timestamp.count()
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                    << close_array
                << close_document
            << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
                    << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    (first_match_stop_time - first_match_start_time) + (second_match_stop_time - second_match_start_time),
                    user_account_values.user_activities.front().totalTime.count(),
                    (first_match_stop_time - first_match_start_time) + (second_match_stop_time - second_match_start_time),
                    second_match_stop_time
            );

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, multipleBetweenTimes) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const AccountCategoryType activity_type = AccountCategoryType::ACTIVITY_TYPE;
    const int activity_index_value = 0;

    //make sure end of time is AFTER max between times values
    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{100};
    const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{150};
    const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{user_account_values.earliest_time_frame_start_timestamp.count() + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count() * 2};

    //2 sets of 'between times'
    const long first_match_start_time = user_account_values.earliest_time_frame_start_timestamp.count() + 1;
    const long first_match_stop_time = first_match_start_time + 50;
    const long first_user_start_time = first_match_stop_time + 100;
    const long first_user_stop_time = first_user_start_time + 120;

    const long second_match_start_time = first_user_stop_time + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count();
    const long second_match_stop_time = second_match_start_time + 40;
    const long second_user_start_time = second_match_stop_time;
    const long second_user_stop_time = second_user_start_time + 223;

    user_account_values.user_activities.front().timeFrames.clear();

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{first_user_start_time},
            1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{first_user_stop_time},
            -1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{second_user_start_time},
            1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{second_user_stop_time},
            -1
    );

    user_account_values.user_activities.front().totalTime = std::chrono::milliseconds{
            (second_user_stop_time - second_user_start_time) + (first_user_stop_time - first_user_start_time)
    };

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
            << user_account_keys::CATEGORIES << open_array
                << open_document
                    << user_account_keys::categories::TYPE << activity_type
                    << user_account_keys::categories::INDEX_VALUE << activity_index_value
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{(second_match_stop_time - second_match_start_time) + (first_match_stop_time - first_match_start_time)}
                    << user_account_keys::categories::TIMEFRAMES << open_array
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << first_match_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << first_match_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << first_user_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << first_user_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << second_match_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << second_match_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << second_user_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << second_user_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                    << close_array
                << close_document
            << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
                    << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    (second_match_stop_time - second_match_start_time) + (first_match_stop_time - first_match_start_time),
                    (second_user_stop_time - second_user_start_time) + (first_user_stop_time - first_user_start_time),
                    0,
                    second_match_stop_time,
                    false
            );

            bsoncxx::array::view between_times_array = activity_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR].get_array().value;

            int num_between_time_elements = 0;
            for(const auto& ele : between_times_array) {
                if(num_between_time_elements == 0) {
                    EXPECT_EQ(first_user_start_time - first_match_stop_time, ele.get_int64().value);
                } else {
                    EXPECT_EQ(second_user_start_time - second_match_stop_time, ele.get_int64().value);
                }
                num_between_time_elements++;
            }
            EXPECT_EQ(num_between_time_elements, 2);

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateAddFieldsTimeFrameTimeStatisticsTesting, eventPassed_noMatchingTimes) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    const AccountCategoryType activity_type = AccountCategoryType::ACTIVITY_TYPE;
    const int activity_index_value = 0;

    //make sure end of time is AFTER max between times values
    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{100};
    const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{150};
    const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{user_account_values.earliest_time_frame_start_timestamp.count() + matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count() * 2};

    const long first_user_start_time = user_account_values.earliest_time_frame_start_timestamp.count() + 1;
    const long first_user_stop_time = first_user_start_time + 50;

    const long event_stop_time = user_account_values.end_of_time_frame_timestamp.count() - 1;
    const long event_start_time = event_stop_time - 50;

    user_account_values.user_activities.front().timeFrames.clear();

    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{first_user_start_time},
            1
    );
    user_account_values.user_activities.front().timeFrames.emplace_back(
            std::chrono::milliseconds{first_user_stop_time},
            -1
    );

    user_account_values.user_activities.front().totalTime = std::chrono::milliseconds{
            (first_user_stop_time - first_user_start_time)
    };

    bsoncxx::builder::stream::document categories_doc{};
    categories_doc
            << user_account_keys::CATEGORIES << open_array
                << open_document
                    << user_account_keys::categories::TYPE << activity_type
                    << user_account_keys::categories::INDEX_VALUE << activity_index_value
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{event_stop_time - event_start_time}
                    << user_account_keys::categories::TIMEFRAMES << open_array
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << event_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << event_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << first_user_start_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                        << open_document
                            << user_account_keys::categories::timeframes::TIME << first_user_stop_time
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                        << close_document
                    << close_array
                << close_document
            << close_array;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.project(
            document{}
                    << user_account_keys::CATEGORIES << 1
                    << finalize
    );

    pipeline.add_fields(categories_doc.view());

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {

        std::cout << makePrettyJson(x) << '\n';

        int num_activities = 0;
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        for(const auto& activity : activities_array) {
            bsoncxx::document::view activity_doc = activity.get_document();

            compareResults(
                    activity_doc,
                    activity_index_value,
                    activity_type,
                    event_stop_time - event_start_time,
                    first_user_stop_time - first_user_start_time,
                    0,
                    event_stop_time,
                    false
            );

            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}
