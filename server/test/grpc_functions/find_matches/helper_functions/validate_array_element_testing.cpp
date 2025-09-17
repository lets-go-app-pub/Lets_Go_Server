//
// Created by jeremiah on 9/5/22.
//

#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include "user_account_keys.h"

#include "helper_functions/find_matches_helper_functions.h"
#include "extract_data_from_bsoncxx.h"
#include "request_statistics_values.h"
#include "generate_matching_users.h"
#include "utility_find_matches_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ValidateArrayElementTesting : public ::testing::Test {
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

    MatchingElement matching_element;

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

        matching_element.generateRandomValues();

        matching_element.oid = match_account_oid;
        matching_element.expiration_time = bsoncxx::types::b_date{user_account_values.current_timestamp + std::chrono::milliseconds{24 * 60 * 60 * 1000}};
        matching_element.saved_statistics_oid = bsoncxx::oid{};
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunctionInsideTransaction(
            ValidateArrayElementReturnEnum expected_return_val,
            FindMatchesArrayNameEnum array_to_pop_from
            ) {

        mongocxx::client_session::with_transaction_cb transaction_callback = [&](
                mongocxx::client_session* session
        ) {
            ValidateArrayElementReturnEnum return_val = validateArrayElement(
                user_account_values,
                user_accounts_collection,
                session,
                array_to_pop_from
            );

            EXPECT_EQ(expected_return_val, return_val);
        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        //NOTE: this must go before the code below, this will block for the transaction to finish
        try {
            session.with_transaction(transaction_callback);
        } catch (const std::exception& e) {
            EXPECT_EQ(std::string(e.what()), "dummy_string");
        }
    }

    void compareMatchingElement(
            FindMatchesArrayNameEnum array_popped_from,
            const bsoncxx::builder::basic::array& activity_statistics
            ) {

        ASSERT_EQ(user_account_values.saved_matches.size(), 1);

        EXPECT_EQ(user_account_values.saved_matches.back().match_user_account_doc->view()["_id"].get_oid().value.to_string(), match_account_oid.to_string());

        bsoncxx::document::value generated_matching_element_doc = buildMatchedAccountDoc<false>(
                match_account_oid,
                matching_element.point_value,
                matching_element.distance,
                matching_element.expiration_time.value,
                matching_element.match_timestamp.value,
                matching_element.from_match_algorithm_list,
                matching_element.saved_statistics_oid
        );

        EXPECT_EQ(user_account_values.saved_matches.back().matching_element_doc, generated_matching_element_doc);

        EXPECT_EQ(user_account_values.saved_matches.back().activity_statistics.view(), activity_statistics.view());

        EXPECT_EQ(user_account_values.saved_matches.back().array_element_is_from, array_popped_from);
        EXPECT_EQ(user_account_values.saved_matches.back().number_swipes_after_extracted, user_account_values.number_swipes_remaining - 1);
    }

};

TEST_F(ValidateArrayElementTesting, sessionSetToNullptr) {
    ValidateArrayElementReturnEnum return_val = validateArrayElement(
            user_account_values,
            user_accounts_collection,
            nullptr,
            FindMatchesArrayNameEnum::other_users_matched_list
    );

    EXPECT_EQ(return_val, ValidateArrayElementReturnEnum::unhandleable_error);

    EXPECT_TRUE(user_account_values.algorithm_matched_account_list.empty());
    EXPECT_TRUE(user_account_values.other_users_swiped_yes_list.empty());

    EXPECT_TRUE(user_account_values.saved_matches.empty());
}

TEST_F(ValidateArrayElementTesting, from_otherUsersMatchedList) {
    matching_element.from_match_algorithm_list = false;

    bsoncxx::builder::stream::document element_builder;
    matching_element.convertToDocument(element_builder);

    user_account_values.other_users_swiped_yes_list.emplace_back(element_builder.view());

    const FindMatchesArrayNameEnum array_to_pop_from = FindMatchesArrayNameEnum::other_users_matched_list;

    runFunctionInsideTransaction(
            ValidateArrayElementReturnEnum::success,
            array_to_pop_from
    );

    EXPECT_TRUE(user_account_values.other_users_swiped_yes_list.empty());

    compareMatchingElement(
            array_to_pop_from,
            bsoncxx::builder::basic::array{}
    );
}

TEST_F(ValidateArrayElementTesting, from_algorithmMatchedList) {

    matching_element.from_match_algorithm_list = true;

    const FindMatchesArrayNameEnum array_to_pop_from = FindMatchesArrayNameEnum::algorithm_matched_list;

    bsoncxx::builder::stream::document element_builder;
    matching_element.convertToDocument(element_builder);

    bsoncxx::builder::basic::array activity_statistics;

    activity_statistics.append(
            document{}
                    << "dummy_key" << "dummy_value_one"
            << finalize
    );

    activity_statistics.append(
            document{}
                    << "dummy_key" << "dummy_value_two"
            << finalize
    );

    element_builder
        << user_account_keys::accounts_list::ACTIVITY_STATISTICS << activity_statistics;

    user_account_values.algorithm_matched_account_list.emplace_back(element_builder.view());

    runFunctionInsideTransaction(
            ValidateArrayElementReturnEnum::success,
            array_to_pop_from
    );

    EXPECT_TRUE(user_account_values.algorithm_matched_account_list.empty());

    compareMatchingElement(
            array_to_pop_from,
            activity_statistics
    );

    ASSERT_EQ(user_account_values.saved_matches.size(), 1);

}

TEST_F(ValidateArrayElementTesting, matchNoLongerValid) {

    match_account_doc.matching_activated = false;
    match_account_doc.setIntoCollection();

    matching_element.from_match_algorithm_list = false;

    bsoncxx::builder::stream::document element_builder;
    matching_element.convertToDocument(element_builder);

    user_account_values.other_users_swiped_yes_list.emplace_back(element_builder.view());

    const FindMatchesArrayNameEnum array_to_pop_from = FindMatchesArrayNameEnum::other_users_matched_list;

    runFunctionInsideTransaction(
            ValidateArrayElementReturnEnum::no_longer_valid,
            array_to_pop_from
    );

    EXPECT_TRUE(user_account_values.other_users_swiped_yes_list.empty());

    EXPECT_TRUE(user_account_values.saved_matches.empty());
}
