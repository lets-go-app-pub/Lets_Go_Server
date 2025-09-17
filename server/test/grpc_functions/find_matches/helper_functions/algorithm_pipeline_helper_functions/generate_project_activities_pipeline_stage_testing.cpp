//
// Created by jeremiah on 8/30/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
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

class GenerateProjectActivitiesPipelineStageTesting : public ::testing::Test {
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
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

};

TEST_F(GenerateProjectActivitiesPipelineStageTesting, activitiesArrayNotEmpty) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.add_fields(
            document{}
                << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHING_ACTIVITIES_FIELD << open_array
                    << open_document
                        << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                        << user_account_keys::categories::INDEX_VALUE << 0
                        << user_account_keys::categories::TIMEFRAMES << open_array
                        << close_array
                    << close_document
                << close_array
            << finalize
            );

    generateProjectCategoriesPipelineStage(
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
            EXPECT_EQ(AccountCategoryType::ACTIVITY_TYPE, AccountCategoryType(activity_doc[user_account_keys::categories::TYPE].get_int32().value));
            EXPECT_EQ(0, activity_doc[user_account_keys::categories::INDEX_VALUE].get_int32().value);
            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateProjectActivitiesPipelineStageTesting, categoriesMatchingTurnedOff) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.add_fields(
            document{}
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHING_ACTIVITIES_FIELD << open_array
                    << close_array
            << finalize
    );

    generateProjectCategoriesPipelineStage(
            &pipeline,
            user_account_values
    );

    auto cursor_result = user_accounts_collection.aggregate(pipeline);

    int num_results = 0;
    for(const auto& x : cursor_result) {
        bsoncxx::array::view activities_array = x[user_account_keys::CATEGORIES].get_array().value;
        size_t num_activities = std::distance(activities_array.begin(), activities_array.end());
        EXPECT_EQ(num_activities, 0);

        num_results++;
    }
    EXPECT_EQ(num_results, 1);
}

TEST_F(GenerateProjectActivitiesPipelineStageTesting, noActivities_matchingCategory) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    bsoncxx::builder::stream::document query_doc;

    mongocxx::pipeline pipeline;

    query_doc
            << "_id" << match_account_oid;

    pipeline.match(query_doc.view());

    pipeline.add_fields(
            document{}
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHING_ACTIVITIES_FIELD << open_array
                    << close_array
            << finalize
    );

    generateProjectCategoriesPipelineStage(
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
            EXPECT_EQ(AccountCategoryType::CATEGORY_TYPE, AccountCategoryType(activity_doc[user_account_keys::categories::TYPE].get_int32().value));
            EXPECT_EQ(0, activity_doc[user_account_keys::categories::INDEX_VALUE].get_int32().value);
            num_activities++;
        }

        num_results++;
        EXPECT_EQ(num_activities, 1);
    }

    EXPECT_EQ(num_results, 1);
}