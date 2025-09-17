//
// Created by jeremiah on 9/15/22.
//

#include <gtest/gtest.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <clear_database_for_testing.h>
#include <generate_multiple_random_accounts.h>
#include <account_objects.h>
#include <user_account_keys.h>

#include "grpc_function_server_template.h"
#include "../../find_matches/helper_functions/generate_matching_users.h"
#include "set_fields_helper_functions/set_fields_helper_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RemoveInvalidElementsForUpdatedMatchingParametersTemplate : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc match_account_doc;

    bsoncxx::builder::stream::document user_account_doc_builder;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::stdx::optional<bsoncxx::document::value> final_user_account_doc;
    bsoncxx::builder::stream::document match_parameters_to_query;
    bsoncxx::builder::stream::document fields_to_update_document;
    bsoncxx::builder::stream::document fields_to_project;

    inline const static int NEW_MAX_DISTANCE = -1;
    inline const static int MAX_DISTANCE_GREATER_THAN = 0;

    MatchingElement matching_element;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        match_account_oid = insertRandomAccounts(1, 0);
        match_account_doc.getFromCollection(match_account_oid);

        ASSERT_TRUE(
                buildMatchingUserForPassedAccount(
                        user_account_doc,
                        match_account_doc,
                        user_account_oid,
                        match_account_oid
                )
        );

        user_account_doc.algorithm_matched_accounts_list.clear();
        user_account_doc.other_users_matched_accounts_list.clear();

        user_account_doc.setIntoCollection();

        user_account_doc_builder.clear();
        user_account_doc.convertToDocument(user_account_doc_builder);

        match_parameters_to_query
                << user_account_keys::MAX_DISTANCE << open_document
                    << "$gte" << MAX_DISTANCE_GREATER_THAN
                << close_document;

        fields_to_update_document
                << user_account_keys::MAX_DISTANCE << NEW_MAX_DISTANCE;

        matching_element.generateRandomValues();
        matching_element.oid = match_account_oid;
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        removeInvalidElementsForUpdatedMatchingParameters(
                user_account_oid,
                user_account_doc_builder.view(),
                user_accounts_collection,
                final_user_account_doc,
                match_parameters_to_query.view(),
                fields_to_update_document.view(),
                fields_to_project.view()
        );
    }

    void compareAccounts() {
        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }
};

TEST_F(RemoveInvalidElementsForUpdatedMatchingParametersTemplate, emptyLists) {
    runFunction();

    user_account_doc.max_distance = NEW_MAX_DISTANCE;

    compareAccounts();
}

TEST_F(RemoveInvalidElementsForUpdatedMatchingParametersTemplate, userShouldBeRemoved) {

    //make sure removed
    match_account_doc.max_distance = MAX_DISTANCE_GREATER_THAN - 1;
    match_account_doc.setIntoCollection();

    user_account_doc.algorithm_matched_accounts_list.emplace_back(
            matching_element
            );
    user_account_doc.setIntoCollection();

    user_account_doc_builder.clear();
    user_account_doc.convertToDocument(user_account_doc_builder);

    runFunction();

    user_account_doc.max_distance = NEW_MAX_DISTANCE;
    user_account_doc.algorithm_matched_accounts_list.pop_back();

    compareAccounts();
}

TEST_F(RemoveInvalidElementsForUpdatedMatchingParametersTemplate, userShouldNotBeRemoved) {
    //make sure NOT removed
    match_account_doc.max_distance = MAX_DISTANCE_GREATER_THAN;
    match_account_doc.setIntoCollection();

    user_account_doc.algorithm_matched_accounts_list.emplace_back(
            matching_element
    );
    user_account_doc.setIntoCollection();

    user_account_doc_builder.clear();
    user_account_doc.convertToDocument(user_account_doc_builder);

    runFunction();

    user_account_doc.max_distance = NEW_MAX_DISTANCE;

    compareAccounts();
}

TEST_F(RemoveInvalidElementsForUpdatedMatchingParametersTemplate, removedFrom_algorithmMatchedAccountList) {
    //make sure removed
    match_account_doc.max_distance = MAX_DISTANCE_GREATER_THAN - 1;
    match_account_doc.setIntoCollection();

    user_account_doc.algorithm_matched_accounts_list.emplace_back(
            matching_element
    );
    user_account_doc.setIntoCollection();

    user_account_doc_builder.clear();
    user_account_doc.convertToDocument(user_account_doc_builder);

    runFunction();

    user_account_doc.max_distance = NEW_MAX_DISTANCE;
    user_account_doc.algorithm_matched_accounts_list.pop_back();

    compareAccounts();
}

TEST_F(RemoveInvalidElementsForUpdatedMatchingParametersTemplate, removedFrom_otherUsersMatchedAccountList) {
    //make sure removed
    match_account_doc.max_distance = MAX_DISTANCE_GREATER_THAN - 1;
    match_account_doc.setIntoCollection();

    user_account_doc.other_users_matched_accounts_list.emplace_back(
            matching_element
    );
    user_account_doc.setIntoCollection();

    user_account_doc_builder.clear();
    user_account_doc.convertToDocument(user_account_doc_builder);

    runFunction();

    user_account_doc.max_distance = NEW_MAX_DISTANCE;
    user_account_doc.other_users_matched_accounts_list.pop_back();

    compareAccounts();
}
