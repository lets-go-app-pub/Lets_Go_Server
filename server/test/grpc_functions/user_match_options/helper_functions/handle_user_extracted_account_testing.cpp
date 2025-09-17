//
// Created by jeremiah on 9/9/22.
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

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "helper_functions/build_update_doc_for_response_type.h"
#include "helper_functions/user_match_options_helper_functions.h"
#include "errors_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class HandleUserExtractedAccountTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid{};
    bsoncxx::oid saved_statistics_oid{};

    UserAccountDoc user_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::optional<bsoncxx::document::value> optional_user_account_doc;
    bsoncxx::document::view user_account_doc_view;
    std::optional<bsoncxx::document::view> user_extracted_array_doc;
    std::optional<bsoncxx::oid> statistics_oid;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunctionAndCompareBasicValues(
            ResponseType response_type,
            bool expected_user_extracted_array_doc = true
            ) {
        mongocxx::client_session::with_transaction_cb transactionCallback = [&](
                mongocxx::client_session* callback_session
        ) {

            bool return_value = handleUserExtractedAccount(
                    current_timestamp,
                    user_accounts_collection,
                    callback_session,
                    user_account_oid,
                    match_account_oid,
                    response_type,
                    optional_user_account_doc,
                    user_account_doc_view,
                    user_extracted_array_doc,
                    statistics_oid
            );

            ASSERT_TRUE(return_value);

        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        try {
            session.with_transaction(transactionCallback);
        } catch (const std::exception& e) {
            EXPECT_EQ(std::string(e.what()), "dummy_string");
            return;
        }

        EXPECT_TRUE(optional_user_account_doc);
        EXPECT_EQ(user_extracted_array_doc.operator bool(), expected_user_extracted_array_doc);
        EXPECT_EQ(statistics_oid.operator bool(), expected_user_extracted_array_doc);

        EXPECT_FALSE(user_account_doc_view.empty());

        if(user_extracted_array_doc) {
            EXPECT_FALSE(user_extracted_array_doc->empty());
        }

        if(statistics_oid) {
            EXPECT_EQ(saved_statistics_oid.to_string(), statistics_oid->to_string());
        }
    }

    MatchingElement addMatchingElement(
            std::vector<MatchingElement>& accounts_list
            ) {
        MatchingElement matching_element;
        matching_element.generateRandomValues();

        matching_element.oid = match_account_oid;
        matching_element.saved_statistics_oid = saved_statistics_oid;

        accounts_list.emplace_back(
                matching_element
        );
        user_account_doc.setIntoCollection();

        return matching_element;
    }

    void compareUserDocumentValues() {
        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

};

TEST_F(HandleUserExtractedAccountTesting, nullSession) {

    bool return_value = handleUserExtractedAccount(
            current_timestamp,
            user_accounts_collection,
            nullptr,
            user_account_oid,
            match_account_oid,
            ResponseType::USER_MATCH_OPTION_NO,
            optional_user_account_doc,
            user_account_doc_view,
            user_extracted_array_doc,
            statistics_oid
    );

    EXPECT_FALSE(return_value);

    compareUserDocumentValues();
}

TEST_F(HandleUserExtractedAccountTesting, responseType_yes) {

    addMatchingElement(user_account_doc.has_been_extracted_accounts_list);

    runFunctionAndCompareBasicValues(ResponseType::USER_MATCH_OPTION_YES);

    //make sure values were properly projected out
    EXPECT_TRUE(user_account_doc_view[user_account_keys::AGE]);
    EXPECT_TRUE(user_account_doc_view[user_account_keys::GENDER]);
    EXPECT_TRUE(user_account_doc_view[user_account_keys::AGE_RANGE]);
    EXPECT_TRUE(user_account_doc_view[user_account_keys::GENDERS_RANGE]);

    user_account_doc.has_been_extracted_accounts_list.pop_back();
    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{current_timestamp},
            1
            );

    compareUserDocumentValues();
}

TEST_F(HandleUserExtractedAccountTesting, zero_extractedElementsFound) {
    runFunctionAndCompareBasicValues(
            ResponseType::USER_MATCH_OPTION_NO,
            false
            );

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{current_timestamp},
            1
    );

    compareUserDocumentValues();
}

TEST_F(HandleUserExtractedAccountTesting, one_extractedElementFound) {
    addMatchingElement(user_account_doc.has_been_extracted_accounts_list);

    runFunctionAndCompareBasicValues(ResponseType::USER_MATCH_OPTION_REPORT);

    user_account_doc.has_been_extracted_accounts_list.pop_back();
    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{current_timestamp},
            1
    );

    compareUserDocumentValues();
}

TEST_F(HandleUserExtractedAccountTesting, one_algorithmMatchedElementFound) {
    addMatchingElement(user_account_doc.has_been_extracted_accounts_list);

    addMatchingElement(user_account_doc.algorithm_matched_accounts_list);

    runFunctionAndCompareBasicValues(ResponseType::USER_MATCH_OPTION_NO);

    user_account_doc.algorithm_matched_accounts_list.pop_back();
    user_account_doc.has_been_extracted_accounts_list.pop_back();
    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{current_timestamp},
            1
    );

    compareUserDocumentValues();
}

TEST_F(HandleUserExtractedAccountTesting, one_otherUserElementFound) {
    addMatchingElement(user_account_doc.has_been_extracted_accounts_list);

    addMatchingElement(user_account_doc.other_users_matched_accounts_list);

    runFunctionAndCompareBasicValues(ResponseType::USER_MATCH_OPTION_NO);

    user_account_doc.other_users_matched_accounts_list.pop_back();
    user_account_doc.has_been_extracted_accounts_list.pop_back();
    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{current_timestamp},
            1
    );

    compareUserDocumentValues();
}

TEST_F(HandleUserExtractedAccountTesting, one_previouslyMatchedElement) {
    addMatchingElement(user_account_doc.has_been_extracted_accounts_list);

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{1000}},
            1
    );
    user_account_doc.setIntoCollection();

    runFunctionAndCompareBasicValues(ResponseType::USER_MATCH_OPTION_NO);

    user_account_doc.has_been_extracted_accounts_list.pop_back();
    user_account_doc.previously_matched_accounts.back().timestamp = bsoncxx::types::b_date{current_timestamp};
    user_account_doc.previously_matched_accounts.back().number_times_matched++;

    compareUserDocumentValues();
}

TEST_F(HandleUserExtractedAccountTesting, multiple_previouslyMatchedElements) {

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{1000}},
            1
    );

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{500}},
            1
    );

    user_account_doc.setIntoCollection();

    runFunctionAndCompareBasicValues(
            ResponseType::USER_MATCH_OPTION_NO,
            false
    );

    user_account_doc.previously_matched_accounts.pop_back();
    user_account_doc.previously_matched_accounts.pop_back();

    compareUserDocumentValues();
}
