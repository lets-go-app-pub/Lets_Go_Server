//
// Created by jeremiah on 5/31/22.
//

#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>
#include <generate_multiple_random_accounts.h>
#include <account_objects.h>
#include <utility_general_functions.h>
#include <move_user_account_statistics_document.h>
#include <info_for_statistics_objects.h>
#include "gtest/gtest.h"

#include "clear_database_for_testing.h"
#include "move_user_account_statistics_document_test.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class UserInfoStatisticsUtility : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(UserInfoStatisticsUtility, insertDocumentToStatisticsNoTransaction) {

    auto generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_statistics_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];

    UserAccountStatisticsDoc user_account_statistics(generated_account_oid);

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    moveUserAccountStatisticsDocument(
            mongo_cpp_client,
            generated_account_oid,
            user_accounts_statistics_collection,
            currentTimestamp,
            true,
            nullptr
    );

    checkMoveUserAccountStatisticsDocumentResult(
            generated_account_oid,
            currentTimestamp,
            user_account_statistics,
            true
    );

}

TEST_F(UserInfoStatisticsUtility, insertDocumentToStatisticsTransaction) {

    auto generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_statistics_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];

    UserAccountStatisticsDoc user_account_statistics(generated_account_oid);

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session) {

        moveUserAccountStatisticsDocument(
                mongo_cpp_client,
                generated_account_oid,
                user_accounts_statistics_collection,
                currentTimestamp,
                true,
                session
                );
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
        ASSERT_EQ(std::string(e.what()), "dummy_string");
    }

    checkMoveUserAccountStatisticsDocumentResult(
            generated_account_oid,
            currentTimestamp,
            user_account_statistics,
            true
            );
}

TEST_F(UserInfoStatisticsUtility, insertDocumentToStatisticsNoCreate) {

    auto generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_statistics_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];

    UserAccountStatisticsDoc user_account_statistics(generated_account_oid);

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    moveUserAccountStatisticsDocument(
            mongo_cpp_client,
            generated_account_oid,
            user_accounts_statistics_collection,
            currentTimestamp,
            false,
            nullptr
            );

    checkMoveUserAccountStatisticsDocumentResult(
            generated_account_oid,
            currentTimestamp,
            user_account_statistics,
            false
            );
}