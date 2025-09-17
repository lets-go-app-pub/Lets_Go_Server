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
#include "helper_functions/user_match_options_helper_functions.h"
#include "info_for_statistics_objects.h"
#include "errors_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class UpdateMatchStatisticsDocumentTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    IndividualMatchStatisticsDoc individual_match_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        individual_match_doc.generateRandomValues(user_account_oid);
        individual_match_doc.setIntoCollection();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void compareFinalDocumentValues() {
        IndividualMatchStatisticsDoc extracted_individual_match_doc(individual_match_doc.current_object_oid);

        EXPECT_EQ(extracted_individual_match_doc, individual_match_doc);
    }
};

TEST_F(UpdateMatchStatisticsDocumentTesting, matchStatisticsDocOid_setToFalse) {

    std::optional<bsoncxx::oid> match_statistics_doc_oid;

    updateMatchStatisticsDocument(
            mongo_cpp_client,
            current_timestamp,
            match_statistics_doc_oid,
            ResponseType::USER_MATCH_OPTION_YES
    );

    compareFinalDocumentValues();
}

TEST_F(UpdateMatchStatisticsDocumentTesting, successful) {
    std::optional<bsoncxx::oid> match_statistics_doc_oid = individual_match_doc.current_object_oid;

    updateMatchStatisticsDocument(
            mongo_cpp_client,
            current_timestamp,
            match_statistics_doc_oid,
            ResponseType::USER_MATCH_OPTION_YES
    );

    individual_match_doc.status_array.emplace_back(
            individual_match_statistics_keys::status_array::status_enum::YES,
            bsoncxx::types::b_date{current_timestamp}
            );

    compareFinalDocumentValues();
}

TEST_F(UpdateMatchStatisticsDocumentTesting, oidDoesNotExist) {
    std::optional<bsoncxx::oid> match_statistics_doc_oid = bsoncxx::oid{};

    updateMatchStatisticsDocument(
            mongo_cpp_client,
            current_timestamp,
            match_statistics_doc_oid,
            ResponseType::USER_MATCH_OPTION_YES
    );

    compareFinalDocumentValues();

    FreshErrorsDoc error;
    error.getFromCollection();

    //error was stored
    EXPECT_NE(error.current_object_oid.to_string(), "000000000000000000000000");
}
