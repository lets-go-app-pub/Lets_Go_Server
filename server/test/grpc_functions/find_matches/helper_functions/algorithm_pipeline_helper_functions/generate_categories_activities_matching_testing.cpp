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

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GenerateCategoriesActivitiesMatchingTesting : public ::testing::Test {
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

    void checkForSingleMatch(
            AccountCategoryType account_category_type
            ) {
        bsoncxx::builder::stream::document query_doc;

        query_doc
                << "_id" << open_document
                << "$ne" << user_account_oid
                << close_document;

        bool return_val = generateCategoriesActivitiesMatching(
                query_doc,
                user_account_values,
                user_accounts_collection
        );

        EXPECT_TRUE(return_val);

        bsoncxx::document::view query_view = query_doc.view();

        AccountCategoryType extracted_category_type = AccountCategoryType(
                query_view[user_account_keys::CATEGORIES].get_document().view()["$elemMatch"].get_document().view()[user_account_keys::categories::TYPE].get_int32().value
        );

        EXPECT_EQ(extracted_category_type, account_category_type);

        mongocxx::pipeline pipeline;

        pipeline.match(query_doc.view());

        auto cursor_result = user_accounts_collection.aggregate(pipeline);

        int num_results = 0;
        for(const auto& x : cursor_result) {
            EXPECT_EQ(x["_id"].get_oid().value, match_account_oid);
            num_results++;
        }

        EXPECT_EQ(num_results, 1);
    }

    void checkForNoMatches(
            AccountCategoryType account_category_type
            ) {
        bsoncxx::builder::stream::document query_doc;

        query_doc
                << "_id" << open_document
                    << "$ne" << user_account_oid
                << close_document;

        bool return_val = generateCategoriesActivitiesMatching(
                query_doc,
                user_account_values,
                user_accounts_collection
        );

        EXPECT_TRUE(return_val);

        bsoncxx::document::view query_view = query_doc.view();

        AccountCategoryType extracted_category_type = AccountCategoryType(
                query_view[user_account_keys::CATEGORIES].get_document().view()["$elemMatch"].get_document().view()[user_account_keys::categories::TYPE].get_int32().value
        );

        EXPECT_EQ(extracted_category_type, account_category_type);

        mongocxx::pipeline pipeline;

        pipeline.match(query_doc.view());

        auto cursor_result = user_accounts_collection.aggregate(pipeline);

        size_t num_results = std::distance(cursor_result.begin(), cursor_result.end());

        EXPECT_EQ(num_results, 0);
    }
};

TEST_F(GenerateCategoriesActivitiesMatchingTesting, matchByCategoriesAndActivities_noActivityMatches) {

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    match_account_doc.categories.clear();
    match_account_doc.categories.emplace_back(
            AccountCategoryType::ACTIVITY_TYPE, 1
            );
    match_account_doc.categories.emplace_back(
            AccountCategoryType::CATEGORY_TYPE, 0
    );
    match_account_doc.setIntoCollection();

    checkForSingleMatch(AccountCategoryType::CATEGORY_TYPE);
}

TEST_F(GenerateCategoriesActivitiesMatchingTesting, categoriesAndActivies_oneActivityMatches) {

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    checkForSingleMatch(AccountCategoryType::ACTIVITY_TYPE);
}

TEST_F(GenerateCategoriesActivitiesMatchingTesting, matchByActiviesOnly_oneActivityMatches) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    checkForSingleMatch(AccountCategoryType::ACTIVITY_TYPE);
}

TEST_F(GenerateCategoriesActivitiesMatchingTesting, matchByActiviesOnly_noActivitiesMatch) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    match_account_doc.categories.clear();
    match_account_doc.categories.emplace_back(
            AccountCategoryType::ACTIVITY_TYPE, 1
    );
    match_account_doc.categories.emplace_back(
            AccountCategoryType::CATEGORY_TYPE, 0
    );
    match_account_doc.setIntoCollection();

    checkForNoMatches(AccountCategoryType::ACTIVITY_TYPE);
}

TEST_F(GenerateCategoriesActivitiesMatchingTesting, invalidValueOf_algorithmSearchOptions) {
    user_account_values.algorithm_search_options = AlgorithmSearchOptions(-1);

    match_account_doc.categories.clear();
    match_account_doc.categories.emplace_back(
            AccountCategoryType::ACTIVITY_TYPE, 1
    );
    match_account_doc.categories.emplace_back(
            AccountCategoryType::CATEGORY_TYPE, 0
    );
    match_account_doc.setIntoCollection();

    checkForSingleMatch(AccountCategoryType::CATEGORY_TYPE);
}
