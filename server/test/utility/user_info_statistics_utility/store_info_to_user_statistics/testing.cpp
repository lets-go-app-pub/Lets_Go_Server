//
// Created by jeremiah on 6/23/22.
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
#include <user_account_statistics_keys.h>
#include "gtest/gtest.h"

#include "clear_database_for_testing.h"
#include "move_user_account_statistics_document_test.h"
#include "store_info_to_user_statistics.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class StoreInfoToUserStatistics : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(StoreInfoToUserStatistics, storeInfoToUserStatistics_baseCaseProperlyUpdates) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_statistics_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];

    UserAccountStatisticsDoc generated_account_statistics_doc(generated_account_oid);

    ASSERT_NE(generated_account_statistics_doc.current_object_oid.to_string(), "000000000000000000000000");

    std::string installation_id = generateUUID();
    std::string device_name = gen_random_alpha_numeric_string(rand() % 50 + 3);
    int api_num = 12;
    int lets_go_version = 15;
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    auto push_update_doc = document{}
        << user_account_statistics_keys::LOGIN_TIMES << open_document
            << user_account_statistics_keys::login_times::INSTALLATION_ID << installation_id
            << user_account_statistics_keys::login_times::DEVICE_NAME << device_name
            << user_account_statistics_keys::login_times::API_NUMBER << api_num
            << user_account_statistics_keys::login_times::LETS_GO_VERSION << lets_go_version
            << user_account_statistics_keys::login_times::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
        << close_document
    << finalize;

    storeInfoToUserStatistics(
            mongo_cpp_client,
            accounts_db,
            generated_account_oid,
            push_update_doc,
            current_timestamp
            );

    generated_account_statistics_doc.login_times.emplace_back(
            installation_id,
            device_name,
            api_num,
            lets_go_version,
            bsoncxx::types::b_date{current_timestamp}
    );

    UserAccountStatisticsDoc extracted_account_statistics_doc(generated_account_oid);

    EXPECT_EQ(extracted_account_statistics_doc, generated_account_statistics_doc);

}

TEST_F(StoreInfoToUserStatistics, storeInfoToUserStatistics_documentTooLarge_noTransaction) {
    //Make sure the document is moved from the collection for storage.
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];

    mongocxx::collection user_accounts_statistics_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];
    mongocxx::collection user_account_statistics_documents_completed_collection = info_for_statistics_db[collection_names::USER_ACCOUNT_STATISTICS_DOCUMENTS_COMPLETED_COLLECTION_NAME];

    UserAccountStatisticsDoc nearly_full_account_statistics_doc(generated_account_oid);

    ASSERT_NE(nearly_full_account_statistics_doc.current_object_oid.to_string(), "000000000000000000000000");

    std::string bios = std::string(1024*128, 'a');
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bsoncxx::builder::stream::document test_builder;

    nearly_full_account_statistics_doc.convertToDocument(test_builder);

    size_t initial_doc_size = test_builder.view().length();

    nearly_full_account_statistics_doc.bios.emplace_back(
            bios,
            bsoncxx::types::b_date{current_timestamp}
    );

    test_builder.clear();
    nearly_full_account_statistics_doc.convertToDocument(test_builder);

    size_t size_difference = test_builder.view().length() - initial_doc_size;

    bool continue_loop = true;

    //Bring the document to the point where one more bio being pushed to the array will
    // make the document larger than the 16Mb limit for mongoDB docs.
    while(continue_loop) {
        test_builder.clear();
        nearly_full_account_statistics_doc.convertToDocument(test_builder);

        if(test_builder.view().length() + size_difference > 1024*1024*16) {
            continue_loop = false;
        } else {
            nearly_full_account_statistics_doc.bios.emplace_back(
                    bios,
                    bsoncxx::types::b_date{current_timestamp}
                    );
        }
    }

    nearly_full_account_statistics_doc.setIntoCollection();

    auto push_update_doc = document{}
        << user_account_statistics_keys::BIOS << open_document
            << user_account_statistics_keys::bios::BIOS << bios
            << user_account_statistics_keys::bios::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
        << close_document
    << finalize;

    storeInfoToUserStatistics(
            mongo_cpp_client,
            accounts_db,
            generated_account_oid,
            push_update_doc,
            current_timestamp
            );

    UserAccountStatisticsDoc generated_account_statistics_doc;
    generated_account_statistics_doc.current_object_oid = generated_account_oid;
    generated_account_statistics_doc.bios.emplace_back(
        bios,
        bsoncxx::types::b_date{current_timestamp}
    );

    UserAccountStatisticsDoc extracted_account_statistics_doc(generated_account_oid);

    EXPECT_EQ(extracted_account_statistics_doc, generated_account_statistics_doc);

    UserAccountStatisticsDocumentsCompletedDoc generated_completed_doc(
            nearly_full_account_statistics_doc,
            generated_account_oid,
            current_timestamp
    );

    auto found_doc = user_account_statistics_documents_completed_collection.find_one(document{} << finalize);

    ASSERT_TRUE(found_doc);

    UserAccountStatisticsDocumentsCompletedDoc extracted_completed_doc;
    extracted_completed_doc.convertDocumentToClass(*found_doc);

    generated_completed_doc.current_object_oid = extracted_completed_doc.current_object_oid;

    EXPECT_EQ(extracted_completed_doc, generated_completed_doc);
}

TEST_F(StoreInfoToUserStatistics, storeInfoToUserStatistics_documentTooLarge_insideTransaction) {
    //Make sure the document is moved from the collection for storage.
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];

    mongocxx::collection user_accounts_statistics_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];
    mongocxx::collection user_account_statistics_documents_completed_collection = info_for_statistics_db[collection_names::USER_ACCOUNT_STATISTICS_DOCUMENTS_COMPLETED_COLLECTION_NAME];

    UserAccountStatisticsDoc nearly_full_account_statistics_doc(generated_account_oid);

    ASSERT_NE(nearly_full_account_statistics_doc.current_object_oid.to_string(), "000000000000000000000000");

    std::string bios = std::string(1024*128, 'a');
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bsoncxx::builder::stream::document test_builder;

    nearly_full_account_statistics_doc.convertToDocument(test_builder);

    size_t initial_doc_size = test_builder.view().length();

    nearly_full_account_statistics_doc.bios.emplace_back(
            bios,
            bsoncxx::types::b_date{current_timestamp}
            );

    test_builder.clear();
    nearly_full_account_statistics_doc.convertToDocument(test_builder);

    size_t size_difference = test_builder.view().length() - initial_doc_size;

    bool continue_loop = true;

    //Bring the document to the point where one more bio being pushed to the array will
    // make the document larger than the 16Mb limit for mongoDB docs.
    while(continue_loop) {
        test_builder.clear();
        nearly_full_account_statistics_doc.convertToDocument(test_builder);

        if(test_builder.view().length() + size_difference > 1024*1024*16) {
            continue_loop = false;
        } else {
            nearly_full_account_statistics_doc.bios.emplace_back(
                    bios,
                    bsoncxx::types::b_date{current_timestamp}
                    );
        }
    }

    nearly_full_account_statistics_doc.setIntoCollection();

    auto push_update_doc = document{}
        << user_account_statistics_keys::BIOS << open_document
            << user_account_statistics_keys::bios::BIOS << bios
            << user_account_statistics_keys::bios::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
        << close_document
    << finalize;


    mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* session) {
        storeInfoToUserStatistics(
                mongo_cpp_client,
                accounts_db,
                generated_account_oid,
                push_update_doc,
                current_timestamp,
                session
                );
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
        ASSERT_EQ(std::string(e.what()), "");
    }

    UserAccountStatisticsDoc generated_account_statistics_doc;
    generated_account_statistics_doc.current_object_oid = generated_account_oid;
    generated_account_statistics_doc.bios.emplace_back(
            bios,
            bsoncxx::types::b_date{current_timestamp}
            );

    UserAccountStatisticsDoc extracted_account_statistics_doc(generated_account_oid);

    EXPECT_EQ(extracted_account_statistics_doc, generated_account_statistics_doc);

    UserAccountStatisticsDocumentsCompletedDoc generated_completed_doc(
            nearly_full_account_statistics_doc,
            generated_account_oid,
            current_timestamp
            );

    auto found_doc = user_account_statistics_documents_completed_collection.find_one(document{} << finalize);

    ASSERT_TRUE(found_doc);

    UserAccountStatisticsDocumentsCompletedDoc extracted_completed_doc;
    extracted_completed_doc.convertDocumentToClass(*found_doc);

    generated_completed_doc.current_object_oid = extracted_completed_doc.current_object_oid;

    EXPECT_EQ(extracted_completed_doc, generated_completed_doc);

}