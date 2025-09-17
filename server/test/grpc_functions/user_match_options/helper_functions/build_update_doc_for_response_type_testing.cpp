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

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class BuildUpdateDocForResponseTypeTesting : public ::testing::Test {
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

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void updateAccountValues() {
        mongocxx::pipeline pipe;

        pipe.add_fields(
                update_document.view()
        );

        auto result = user_accounts_collection.update_one(
                document{}
                        << "_id" << user_account_oid
                << finalize,
                pipe
        );

        EXPECT_EQ(result->result().matched_count(), 1);
    }

    void buildAndRunFunction(
            ResponseType response_type,
            BlockReportFieldLocationsCalledFrom called_from
            ) {

        switch (called_from) {
            case BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER:
                buildAddFieldsDocForBlockReportPipeline<BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER>(
                        current_timestamp,
                        update_document,
                        response_type,
                        random_user_oid
                );
                break;
            case BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER:
                buildAddFieldsDocForBlockReportPipeline<BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER>(
                        current_timestamp,
                        update_document,
                        response_type,
                        random_user_oid
                );
                break;
            case USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER:
                buildAddFieldsDocForBlockReportPipeline<USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER>(
                        current_timestamp,
                        update_document,
                        response_type,
                        random_user_oid
                );
                break;
            case USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER:
                buildAddFieldsDocForBlockReportPipeline<USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER>(
                        current_timestamp,
                        update_document,
                        response_type,
                        random_user_oid
                );
                break;
        }

        updateAccountValues();
    }

    void addToOtherUserBlockedList() {
        user_account_doc.other_users_blocked.emplace_back(
                random_user_oid.to_string(),
                bsoncxx::types::b_date{current_timestamp}
        );
    }

    void finalComparison() {
        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

};

TEST_F(BuildUpdateDocForResponseTypeTesting, BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER_responseTypeNo) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_NO,
            BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER
    );

    user_account_doc.number_times_swiped_no++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER_responseTypeBlock) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_BLOCK,
            BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER
    );

    addToOtherUserBlockedList();

    user_account_doc.number_times_this_user_blocked_from_chat_room++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER_responseTypeReport) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_REPORT,
            BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER
    );

    addToOtherUserBlockedList();

    user_account_doc.number_times_this_user_reported_from_chat_room++;

    finalComparison();
}


TEST_F(BuildUpdateDocForResponseTypeTesting, BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER_responseTypeNo) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_NO,
            BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER
    );

    user_account_doc.number_times_others_swiped_no++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER_responseTypeBlock) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_BLOCK,
            BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER
    );

    user_account_doc.number_times_blocked_by_others_in_chat_room++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER_responseTypeReport) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_REPORT,
            BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER
    );

    user_account_doc.number_times_reported_by_others_in_chat_room++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER_responseTypeNo) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_NO,
            USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER
    );

    user_account_doc.number_times_swiped_no++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER_responseTypeBlock) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_BLOCK,
            USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER
    );

    addToOtherUserBlockedList();

    user_account_doc.number_times_swiped_block++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER_responseTypeReport) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_REPORT,
            USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER
    );

    addToOtherUserBlockedList();

    user_account_doc.number_times_swiped_report++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER_responseTypeNo) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_NO,
            USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER
    );

    user_account_doc.number_times_others_swiped_no++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER_responseTypeBlock) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_BLOCK,
            USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER
    );

    user_account_doc.number_times_others_swiped_block++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER_responseTypeReport) {
    buildAndRunFunction(
            ResponseType::USER_MATCH_OPTION_REPORT,
            USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER
    );

    user_account_doc.number_times_others_swiped_report++;

    finalComparison();
}

TEST_F(BuildUpdateDocForResponseTypeTesting, invalidResponseType) {
    buildAndRunFunction(
            ResponseType(-1),
            USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER
    );

    finalComparison();
}
