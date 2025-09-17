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
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "helper_functions/user_match_options_helper_functions.h"
#include "errors_objects.h"
#include "../../find_matches/helper_functions/generate_matching_users.h"
#include "chat_rooms_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class MoveExtractedElementToOtherAccountTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc match_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    MatchingElement matching_element;

    bsoncxx::builder::stream::document user_document_builder;

    bsoncxx::builder::stream::document user_extracted_array_builder;
    std::optional<bsoncxx::document::view> user_extracted_array_view;

    int testing_result = 0;

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

        //Make this a matching element that is not expired.
        matching_element.generateRandomValues();
        matching_element.oid = match_account_oid;
        matching_element.distance = 0;
        matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED};
        matching_element.from_match_algorithm_list = true;

        user_account_doc.convertToDocument(user_document_builder);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {

        matching_element.convertToDocument(user_extracted_array_builder);
        user_extracted_array_view = user_extracted_array_builder.view();

        match_account_doc.setIntoCollection();

        mongocxx::client_session::with_transaction_cb transactionCallback = [&](
                mongocxx::client_session* callback_session
        ) {
            bool return_result = moveExtractedElementToOtherAccount(
                    mongo_cpp_client,
                    accounts_db,
                    user_accounts_collection,
                    callback_session,
                    user_extracted_array_view,
                    user_document_builder.view(),
                    match_account_oid,
                    user_account_oid,
                    current_timestamp,
                    testing_result
            );

            EXPECT_TRUE(return_result);
        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        //NOTE: this must go before the code below, this will block for the transaction to finish
        try {
            session.with_transaction(transactionCallback);
        } catch (const std::exception& e) {
            EXPECT_EQ(std::string(e.what()), "dummy_string");
        }
    }

    void checkUserAccounts() {
        UserAccountDoc extracted_match_account_doc(match_account_oid);
        EXPECT_EQ(match_account_doc, extracted_match_account_doc);

        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

    static void addMatchingElement(
            std::vector<MatchingElement>& accounts_list
            ) {
        MatchingElement extracted_user_matching_element;

        extracted_user_matching_element.generateRandomValues();
        extracted_user_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED};

        accounts_list.emplace_back(
                extracted_user_matching_element
        );
    }

};

TEST_F(MoveExtractedElementToOtherAccountTesting, nullSessionPassed) {

    matching_element.convertToDocument(user_extracted_array_builder);
    user_extracted_array_view = user_extracted_array_builder.view();

    bool return_result = moveExtractedElementToOtherAccount(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            nullptr,
            user_extracted_array_view,
            user_document_builder.view(),
            match_account_oid,
            user_account_oid,
            current_timestamp,
            testing_result
    );

    EXPECT_FALSE(return_result);
    EXPECT_EQ(testing_result, 0);

    checkUserAccounts();
}

TEST_F(MoveExtractedElementToOtherAccountTesting, fromMatchAlgorithm_notInsideExtractedList) {

    //NOTE: This is one of the generic cases. The other is 'fromOtherUserSwipedYesList'.

    matching_element.from_match_algorithm_list = true;

    matching_element.convertToDocument(user_extracted_array_builder);
    user_extracted_array_view = user_extracted_array_builder.view();

    //This is to make sure the element is inserted in a sorted order.
    addMatchingElement(match_account_doc.other_users_matched_accounts_list);
    match_account_doc.other_users_matched_accounts_list.back().point_value = matching_element.point_value + 1;

    addMatchingElement(match_account_doc.other_users_matched_accounts_list);
    match_account_doc.other_users_matched_accounts_list.back().point_value = matching_element.point_value - 1;

    runFunction();

    EXPECT_EQ(testing_result, 0);

    matching_element.oid = user_account_oid;
    matching_element.from_match_algorithm_list = false;

    //insert in a sorted order
    match_account_doc.other_users_matched_accounts_list.insert(
            match_account_doc.other_users_matched_accounts_list.begin() + 1,
            matching_element
            );

    checkUserAccounts();
}

TEST_F(MoveExtractedElementToOtherAccountTesting, fromMatchAlgorithm_existsInsideExtractedList) {

    matching_element.from_match_algorithm_list = true;

    //Set up element already inside extracted list.
    addMatchingElement(match_account_doc.has_been_extracted_accounts_list);
    match_account_doc.has_been_extracted_accounts_list.back().oid = user_account_oid;

    runFunction();

    EXPECT_EQ(testing_result, 0);

    matching_element.oid = user_account_oid;
    matching_element.from_match_algorithm_list = false;

    match_account_doc.has_been_extracted_accounts_list.pop_back();
    match_account_doc.has_been_extracted_accounts_list.emplace_back(
            matching_element
    );

    checkUserAccounts();
}

TEST_F(MoveExtractedElementToOtherAccountTesting, fromOtherUserSwipedYesList) {

    //NOTE: This is one of the generic cases. The other is 'fromMatchAlgorithm_notInsideExtractedList'.

    matching_element.from_match_algorithm_list = false;

    runFunction();

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    std::vector<std::string> collection_names = chat_rooms_db.list_collection_names();

    std::string chat_room_id;
    const std::chrono::milliseconds last_active_time{current_timestamp + std::chrono::milliseconds{2}};

    //extract the chat room id
    for(const auto& collection_name : collection_names) {
        //do not drop the info collection
        if(collection_name != collection_names::CHAT_ROOM_INFO) {
            chat_room_id = collection_name.substr(collection_names::CHAT_ROOM_ID_.size());
        }
    }

    EXPECT_EQ(testing_result, 3);

    match_account_doc.chat_rooms.emplace_back(
            chat_room_id,
            bsoncxx::types::b_date{last_active_time}
            );

    match_account_doc.other_accounts_matched_with.emplace_back(
            user_account_oid.to_string(),
            bsoncxx::types::b_date{last_active_time}
            );

    user_account_doc.chat_rooms.emplace_back(
            chat_room_id,
            bsoncxx::types::b_date{last_active_time}
    );

    user_account_doc.other_accounts_matched_with.emplace_back(
            match_account_oid.to_string(),
            bsoncxx::types::b_date{last_active_time}
    );

    checkUserAccounts();

    //NOTE: Guarantee basic matching info is correct, the other functions involved in creating
    // a chat room are checked elsewhere.
    ChatRoomHeaderDoc header_doc(chat_room_id);

    ASSERT_NE(header_doc.matching_oid_strings, nullptr);
    ASSERT_EQ(header_doc.matching_oid_strings->size(), 2);

    bool user_exists_in_arr = false, match_exists_in_arr = false;
    for(const auto& oid : *header_doc.matching_oid_strings) {
        if(oid == user_account_oid.to_string()) {
            user_exists_in_arr = true;
        } else if(oid == match_account_oid.to_string()) {
            match_exists_in_arr = true;
        }
    }

    EXPECT_TRUE(user_exists_in_arr);
    EXPECT_TRUE(match_exists_in_arr);

    user_exists_in_arr = false;
    match_exists_in_arr = false;

    EXPECT_EQ(header_doc.accounts_in_chat_room.size(), 2);

    for(const auto& account : header_doc.accounts_in_chat_room) {
        if(account.account_oid.to_string() == user_account_oid.to_string()) {
            user_exists_in_arr = true;

            EXPECT_EQ(account.state_in_chat_room, AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN);
        } else if(account.account_oid.to_string() == match_account_oid.to_string()) {
            match_exists_in_arr = true;

            EXPECT_EQ(account.state_in_chat_room, AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);
        }
    }

    EXPECT_TRUE(user_exists_in_arr);
    EXPECT_TRUE(match_exists_in_arr);

    //NOTE: Statistics function is checked elsewhere.
}

TEST_F(MoveExtractedElementToOtherAccountTesting, matchIsExpired) {
    matching_element.from_match_algorithm_list = true;
    matching_element.expiration_time = bsoncxx::types::b_date{current_timestamp + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED};

    runFunction();

    EXPECT_EQ(testing_result, 1);

    checkUserAccounts();
}

TEST_F(MoveExtractedElementToOtherAccountTesting, matchIsNoLongerValid) {

    matching_element.from_match_algorithm_list = true;

    match_account_doc.genders_range.clear();

    match_account_doc.genders_range.emplace_back(
            gen_random_alpha_numeric_string(100)
            );

    runFunction();

    EXPECT_EQ(testing_result, 2);

    checkUserAccounts();
}

TEST_F(MoveExtractedElementToOtherAccountTesting, properlyRemovesAccountsListValues) {

    //NOTE: HAS_BEEN_EXTRACTED_ACCOUNTS_LIST was checked above inside fromMatchAlgorithm_existsInsideExtractedList.

    addMatchingElement(match_account_doc.algorithm_matched_accounts_list);
    match_account_doc.algorithm_matched_accounts_list.back().oid = user_account_oid;

    addMatchingElement(match_account_doc.other_users_matched_accounts_list);
    match_account_doc.other_users_matched_accounts_list.back().oid = user_account_oid;

    runFunction();

    EXPECT_EQ(testing_result, 0);

    matching_element.oid = user_account_oid;
    matching_element.from_match_algorithm_list = false;

    //Removes previous (and duplicate) values before inserting new values.
    match_account_doc.algorithm_matched_accounts_list.clear();
    match_account_doc.other_users_matched_accounts_list.clear();

    match_account_doc.other_users_matched_accounts_list.emplace_back(
            matching_element
    );

    checkUserAccounts();
}
