//
// Created by jeremiah on 8/29/22.
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
#include "event_request_message_is_valid.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "generate_randoms.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GenerateQueryForPipelineTesting : public ::testing::Test {
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

    void checkForSingleMatch() {
        bsoncxx::builder::stream::document query_doc;

        generateInitialQueryForPipeline(
                user_account_values,
                query_doc,
                false
        );

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

    void checkForNoMatches() {
        bsoncxx::builder::stream::document query_doc;

        generateInitialQueryForPipeline(
                user_account_values,
                query_doc,
                false
        );

        mongocxx::pipeline pipeline;

        pipeline.match(query_doc.view());

        auto cursor_result = user_accounts_collection.aggregate(pipeline);

        size_t num_results = std::distance(cursor_result.begin(), cursor_result.end());

        EXPECT_EQ(num_results, 0);
    }
};

TEST_F(GenerateQueryForPipelineTesting, successfulMatch) {
    checkForSingleMatch();
}

TEST_F(GenerateQueryForPipelineTesting, statusNotActive) {

    match_account_doc.status = UserAccountStatus::STATUS_SUSPENDED;
    match_account_doc.setIntoCollection();

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, matchingNotActivated) {
    match_account_doc.matching_activated = false;
    match_account_doc.setIntoCollection();

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, matchAgeNotInUserAgeRange_tooOld) {
    match_account_doc.age = user_account_doc.age_range.max + 1;
    match_account_doc.setIntoCollection();

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, matchAgeNotInUserAgeRange_tooYoung) {
    match_account_doc.age = user_account_doc.age_range.min - 1;
    match_account_doc.setIntoCollection();

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, userAgeNotInMatchAgeRange_tooOld) {
    match_account_doc.age_range.min = user_account_doc.age + 1;
    match_account_doc.age_range.max = match_account_doc.age_range.min + 1;
    match_account_doc.setIntoCollection();

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, userAgeNotInMatchAgeRange_tooYoung) {
    match_account_doc.age_range.max = user_account_doc.age - 1;
    match_account_doc.age_range.min = match_account_doc.age_range.max - 1;
    match_account_doc.setIntoCollection();

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, userGenderNotInMatchGenderRange) {
    match_account_doc.genders_range.clear();
    match_account_doc.genders_range.emplace_back(user_account_doc.gender + 'a');
    match_account_doc.setIntoCollection();

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, matchGenderNotInUserGenderRange) {
    user_gender_range_builder.clear();

    user_gender_range_builder.append(match_account_doc.gender + 'a');

    user_account_values.genders_to_match_with_bson_array = user_gender_range_builder.view();

    user_account_values.user_matches_with_everyone = false;

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, userMatchesWithEveryone) {
    user_gender_range_builder.clear();

    user_gender_range_builder.append(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);

    user_account_values.genders_to_match_with_bson_array = user_gender_range_builder.view();
    user_account_values.user_matches_with_everyone = true;

    checkForSingleMatch();
}

TEST_F(GenerateQueryForPipelineTesting, matchMatchesWithEveryone) {
    match_account_doc.genders_range.clear();
    match_account_doc.genders_range.emplace_back(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
    match_account_doc.setIntoCollection();

    checkForSingleMatch();
}

TEST_F(GenerateQueryForPipelineTesting, userOidIn_matchOtherUsersBlocked) {
    match_account_doc.other_users_blocked.clear();
    match_account_doc.other_users_blocked.emplace_back(
            user_account_oid.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{100}}
            );
    match_account_doc.setIntoCollection();

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, userOidIn_matchPreviouslyMatchedAccounts_recently) {
    match_account_doc.previously_matched_accounts.clear();
    match_account_doc.previously_matched_accounts.emplace_back(
            user_account_oid,
            bsoncxx::types::b_date{user_account_values.current_timestamp - matching_algorithm::TIME_BETWEEN_SAME_ACCOUNT_MATCHES / 2},
            1
    );
    match_account_doc.setIntoCollection();

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, userOidIn_matchPreviouslyMatchedAccounts_notRecently) {
    match_account_doc.previously_matched_accounts.clear();
    match_account_doc.previously_matched_accounts.emplace_back(
            user_account_oid,
            bsoncxx::types::b_date{user_account_values.current_timestamp - matching_algorithm::TIME_BETWEEN_SAME_ACCOUNT_MATCHES},
            1
    );
    match_account_doc.setIntoCollection();

    checkForSingleMatch();
}

TEST_F(GenerateQueryForPipelineTesting, userNotWithinMatchMaxDistance) {

    //One degree of latitude equals approximately 69 miles.
    //Latitude was set equal above, so adding one should make the users ~69 miles apart.
    match_account_doc.max_distance = 60;
    match_account_doc.location.latitude = user_account_doc.location.latitude + 1;
    match_account_doc.setIntoCollection();

    user_account_values.max_distance = 100;

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, matchNotWithinUserMaxDistance) {

    //One degree of latitude equals approximately 69 miles.
    //Latitude was set equal above, so adding one should make the users ~69 miles apart.
    match_account_doc.max_distance = 100;
    match_account_doc.location.latitude = user_account_doc.location.latitude + 1;
    match_account_doc.setIntoCollection();

    user_account_values.max_distance = 60;

    checkForNoMatches();
}

TEST_F(GenerateQueryForPipelineTesting, matchOid_inRestrictedAccounts) {
    user_account_values.restricted_accounts.insert(match_account_oid);

    checkForNoMatches();
}
