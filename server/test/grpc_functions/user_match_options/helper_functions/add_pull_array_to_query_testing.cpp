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

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class AddPullArrayToQueryTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::builder::stream::document update_document;

    const bsoncxx::oid random_user_oid{};

    bsoncxx::builder::stream::document project_reduced_arrays_doc_builder;
    bsoncxx::builder::stream::document dummy_doc_view;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        MatchingElement matching_element;
        matching_element.generateRandomValues();
        matching_element.oid = random_user_oid;

        user_account_doc.algorithm_matched_accounts_list.emplace_back(matching_element);
        user_account_doc.setIntoCollection();

        dummy_doc_view
                << "dummy" << "doc";
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunctionAndUpdate(int number_elements) {
        addPullArrayToQuery(
                random_user_oid,
                dummy_doc_view.view(),
                project_reduced_arrays_doc_builder,
                user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST,
                user_account_keys::accounts_list::OID,
                number_elements
        );

        //make sure the document is not empty, otherwise it will clear all data on the update
        project_reduced_arrays_doc_builder
            << "$set" << open_document
                << "_id" << user_account_oid
            << close_document;

        auto result = user_accounts_collection.update_one(
                document{}
                        << "_id" << bsoncxx::types::b_oid{user_account_oid}
                        << finalize,
                project_reduced_arrays_doc_builder.view()
        );

        EXPECT_EQ(result->result().matched_count(), 1);
    }

    void finalComparison() {
        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }
};

TEST_F(AddPullArrayToQueryTesting, oneElement) {
    runFunctionAndUpdate(1);

    user_account_doc.algorithm_matched_accounts_list.pop_back();

    finalComparison();
}

TEST_F(AddPullArrayToQueryTesting, noElements) {
    runFunctionAndUpdate(0);

    finalComparison();
}
