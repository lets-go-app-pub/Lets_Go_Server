//
// Created by jeremiah on 9/15/22.
//

#include <gtest/gtest.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <clear_database_for_testing.h>
#include <generate_multiple_random_accounts.h>
#include <account_objects.h>
#include <utility_general_functions.h>
#include <general_values.h>
#include <user_account_keys.h>

#include "grpc_function_server_template.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GetLoginDocumentTestingTemplate : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::string login_token_str;
    std::string installation_id;

    bsoncxx::builder::stream::document merge_document;
    bsoncxx::builder::stream::document projection_document;

    std::shared_ptr<std::vector<bsoncxx::document::value>> append_to_and_statement_doc = nullptr;

    bsoncxx::stdx::optional<bsoncxx::document::value> find_and_update_user_account;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        login_token_str = user_account_doc.logged_in_token;
        installation_id = user_account_doc.installation_ids.front();

        projection_document
                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1;
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    template<bool allowed_to_run_with_not_enough_info = true, bool requires_subscription = false>
    void runFunction() {
        const bsoncxx::document::value login_document = getLoginDocument<allowed_to_run_with_not_enough_info, requires_subscription>(
                login_token_str,
                installation_id,
                merge_document.view(),
                current_timestamp,
                append_to_and_statement_doc
        );

        const bool return_value = runInitialLoginOperation(
                find_and_update_user_account,
                user_accounts_collection,
                user_account_oid,
                login_document,
                projection_document
        );

        EXPECT_TRUE(return_value);
    }

    void checkFunctionSuccessful(ReturnStatus expected_logged_in_return_message) {
        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);

        EXPECT_EQ(extracted_user_account_doc.logged_in_return_message, expected_logged_in_return_message);
    }
};

TEST_F(GetLoginDocumentTestingTemplate, successfulLogin) {
    runFunction();

    checkFunctionSuccessful(ReturnStatus::SUCCESS);
}

TEST_F(GetLoginDocumentTestingTemplate, mergeWorks) {

    //A value that cannot happen.
    const int new_max_distance = -1;

    merge_document
            << user_account_keys::MAX_DISTANCE << new_max_distance;

    runFunction();

    user_account_doc.max_distance = new_max_distance;

    checkFunctionSuccessful(ReturnStatus::SUCCESS);

}

TEST_F(GetLoginDocumentTestingTemplate, accountBanned) {

    user_account_doc.status = UserAccountStatus::STATUS_BANNED;
    user_account_doc.setIntoCollection();

    runFunction();

    checkFunctionSuccessful(ReturnStatus::ACCOUNT_BANNED);
}

TEST_F(GetLoginDocumentTestingTemplate, accountSuspended) {
    user_account_doc.status = UserAccountStatus::STATUS_SUSPENDED;
    user_account_doc.setIntoCollection();

    runFunction();

    checkFunctionSuccessful(ReturnStatus::ACCOUNT_SUSPENDED);
}

TEST_F(GetLoginDocumentTestingTemplate, installationIdInvalid) {

    installation_id = gen_random_alpha_numeric_string(5);

    user_account_doc.setIntoCollection();

    runFunction();

    checkFunctionSuccessful(ReturnStatus::LOGGED_IN_ELSEWHERE);
}

TEST_F(GetLoginDocumentTestingTemplate, loginTimeExpired) {

    user_account_doc.logged_in_token_expiration = bsoncxx::types::b_date{current_timestamp};

    user_account_doc.setIntoCollection();

    runFunction();

    checkFunctionSuccessful(ReturnStatus::LOGIN_TOKEN_EXPIRED);
}

TEST_F(GetLoginDocumentTestingTemplate, loginTokenDoesNotMatch) {
    user_account_doc.logged_in_token = generateUUID();

    user_account_doc.setIntoCollection();

    runFunction();

    checkFunctionSuccessful(ReturnStatus::LOGIN_TOKEN_DID_NOT_MATCH);
}

TEST_F(GetLoginDocumentTestingTemplate, userNotSubscribed) {
    user_account_doc.subscription_status = UserSubscriptionStatus::NO_SUBSCRIPTION;
    user_account_doc.setIntoCollection();

    runFunction<false, true>();

    checkFunctionSuccessful(ReturnStatus::SUBSCRIPTION_REQUIRED);
}

TEST_F(GetLoginDocumentTestingTemplate, appendAdditionalError) {

    append_to_and_statement_doc = std::make_shared<std::vector<bsoncxx::document::value>>();

    //this should always fail, max distance will always be greater than or equal to 1
    bsoncxx::document::value userRequiredInChatRoomCondition =
            document{}
                << "$lt" << open_array
                    << "$" + user_account_keys::MAX_DISTANCE << 0
                << close_array
            << finalize;

    append_to_and_statement_doc->emplace_back(userRequiredInChatRoomCondition.view());

    runFunction();

    checkFunctionSuccessful(ReturnStatus::UNKNOWN);
}
