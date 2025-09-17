//
// Created by jeremiah on 9/3/22.
//

#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include "user_account_keys.h"
#include <google/protobuf/util/message_differencer.h>

#include "helper_functions/find_matches_helper_functions.h"
#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ExtractAndSaveUserInfoTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    bsoncxx::builder::stream::document user_account_doc_builder;

    UserAccountValues user_account_values;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    ReturnStatus return_status = ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_;
    findmatches::FindMatchesCapMessage::SuccessTypes success_type = findmatches::FindMatchesCapMessage_SuccessTypes_FindMatchesCapMessage_SuccessTypes_INT_MAX_SENTINEL_DO_NOT_USE_;

    const std::function<void(
            const ReturnStatus&,
            const findmatches::FindMatchesCapMessage::SuccessTypes&
    )> send_error_message_to_client = [&]
            (const ReturnStatus& _return_status, const findmatches::FindMatchesCapMessage::SuccessTypes& _success_type) {
        return_status = _return_status;
        success_type = _success_type;
    };

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account_value;

    void generateRandomMatchedItems(std::vector<MatchingElement>& matching_element_vector) {
        //guarantee at least one element is always inserted
        size_t num_to_insert = rand() % 10 + 1;
        for(size_t i = 0; i < num_to_insert; ++i) {
            matching_element_vector.emplace_back(
                    bsoncxx::oid{},
                    1234.43,
                    55.11256,
                    bsoncxx::types::b_date{user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{rand() % 10000 + 1}},
                    bsoncxx::types::b_date{user_account_values.earliest_time_frame_start_timestamp - std::chrono::milliseconds{rand() % 100000}},
                    rand() % 2,
                    bsoncxx::oid{}
            );
        }
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        //sleep to guarantee currentTimestamp is unique
        std::this_thread::sleep_for(std::chrono::milliseconds{2});

        const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_oid;
        const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = getCurrentTimestamp(); //want the 'real' timestamp here because sms verification will generate real values for the user account
        const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = user_account_values.current_timestamp + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED;
        const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = user_account_values.current_timestamp + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES;
        const_cast<double&>(user_account_values.longitude) = 123.45111;
        const_cast<double&>(user_account_values.latitude) = -54.22467;

        user_account_doc.last_time_match_algorithm_ran = bsoncxx::types::b_date{user_account_values.current_timestamp - matching_algorithm::TIME_BETWEEN_ALGORITHM_MATCH_INVALIDATION};

        generateRandomMatchedItems(user_account_doc.has_been_extracted_accounts_list);
        generateRandomMatchedItems(user_account_doc.other_users_matched_accounts_list);
        generateRandomMatchedItems(user_account_doc.algorithm_matched_accounts_list);

        user_account_doc.setIntoCollection();

        user_account_doc.convertToDocument(user_account_doc_builder);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunctionInsideTransaction(bool expected_return_value = true) {
        mongocxx::client_session::with_transaction_cb transactionCallback = [&](
                mongocxx::client_session* session
        ) {
            bool return_value = extractAndSaveUserInfo(
                    user_account_values,
                    user_accounts_collection,
                    session,
                    send_error_message_to_client,
                    find_user_account_value
            );

            ASSERT_EQ(return_value, expected_return_value);
        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        //NOTE: this must go before the code below, this will block for the transaction to finish
        try {
            session.with_transaction(transactionCallback);
        } catch (const std::exception& e) {
            EXPECT_EQ(std::string(e.what()), "dummy_string");
        }
    }

    template <typename T>
    void compareMatchedDocuments(
            const std::vector<T>& user_account_values_list,
            const std::vector<MatchingElement>& user_account_doc_list
            ) {
        ASSERT_EQ(user_account_values_list.size(), user_account_doc_list.size());
        for(size_t i = 0; i < user_account_values_list.size(); ++i) {
            //list is reversed inside extractUserInfo
            size_t final_index = user_account_doc_list.size() - i - 1;
            bsoncxx::document::view doc_view = user_account_values_list[i];
            EXPECT_EQ(doc_view[user_account_keys::accounts_list::OID].get_oid().value.to_string(), user_account_doc_list[final_index].oid.to_string());
            EXPECT_EQ(doc_view[user_account_keys::accounts_list::POINT_VALUE].get_double().value, user_account_doc_list[final_index].point_value);
            EXPECT_EQ(doc_view[user_account_keys::accounts_list::DISTANCE].get_double().value, user_account_doc_list[final_index].distance);
            EXPECT_EQ(doc_view[user_account_keys::accounts_list::EXPIRATION_TIME].get_date().value.count(), user_account_doc_list[final_index].expiration_time.value.count());
            EXPECT_EQ(doc_view[user_account_keys::accounts_list::MATCH_TIMESTAMP].get_date().value.count(), user_account_doc_list[final_index].match_timestamp.value.count());
            EXPECT_EQ(doc_view[user_account_keys::accounts_list::FROM_MATCH_ALGORITHM_LIST].get_bool().value, user_account_doc_list[final_index].from_match_algorithm_list);
            EXPECT_EQ(doc_view[user_account_keys::accounts_list::SAVED_STATISTICS_OID].get_oid().value.to_string(), user_account_doc_list[final_index].saved_statistics_oid.to_string());
        }
    }

    void checkUserAccountValuesMatch() {
        EXPECT_EQ(user_account_values.user_account_doc_view["_id"].get_oid().value.to_string(), user_account_oid.to_string());
        EXPECT_EQ(user_account_values.number_swipes_remaining, matching_algorithm::MAXIMUM_NUMBER_SWIPES);
        EXPECT_EQ(user_account_values.draw_from_list_value, user_account_doc.int_for_match_list_to_draw_from);
        ASSERT_EQ(
                std::distance(
                        user_account_values.genders_to_match_with_bson_array.begin(),
                        user_account_values.genders_to_match_with_bson_array.end()
                ),
                user_account_doc.genders_range.size()
        );

        for(size_t i = 0; i < user_account_doc.genders_range.size(); ++i) {
            EXPECT_EQ(user_account_values.genders_to_match_with_bson_array.find(i)->get_string().value.to_string(), user_account_doc.genders_range[i]);
        }

        EXPECT_EQ(user_account_values.gender, user_account_doc.gender);
        EXPECT_EQ(user_account_values.age, user_account_doc.age);
        EXPECT_EQ(user_account_values.min_age_range, user_account_doc.age_range.min);
        EXPECT_EQ(user_account_values.max_age_range, user_account_doc.age_range.max);

        compareMatchedDocuments<bsoncxx::document::view>(
                user_account_values.other_users_swiped_yes_list,
                user_account_doc.other_users_matched_accounts_list
        );

        compareMatchedDocuments<bsoncxx::document::view_or_value>(
                user_account_values.algorithm_matched_account_list,
                user_account_doc.algorithm_matched_accounts_list
        );
    }

    void checkUserAccountDocumentMatches() {
        ASSERT_TRUE(find_user_account_value);

        user_account_doc.last_time_find_matches_ran = bsoncxx::types::b_date{user_account_values.current_timestamp};
        user_account_doc.location.latitude = user_account_values.latitude;
        user_account_doc.location.longitude = user_account_values.longitude;

        UserAccountDoc extracted_user_account_doc(user_account_oid);

        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

    void runBasicSuccessCheck() {

        EXPECT_EQ(return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
        EXPECT_EQ(success_type, findmatches::FindMatchesCapMessage_SuccessTypes_FindMatchesCapMessage_SuccessTypes_INT_MAX_SENTINEL_DO_NOT_USE_);

        checkUserAccountDocumentMatches();
        checkUserAccountValuesMatch();
    }

};

TEST_F(ExtractAndSaveUserInfoTesting, sessionIsNullptr) {

    bool return_value = extractAndSaveUserInfo(
            user_account_values,
            user_accounts_collection,
            nullptr,
            send_error_message_to_client,
            find_user_account_value
    );

    EXPECT_FALSE(return_value);
    EXPECT_FALSE(find_user_account_value);

    EXPECT_EQ(return_status, ReturnStatus::LG_ERROR);
    EXPECT_EQ(success_type, findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN);
}

TEST_F(ExtractAndSaveUserInfoTesting, onlyBasicInfoUpdated) {
    runFunctionInsideTransaction();

    runBasicSuccessCheck();
}

TEST_F(ExtractAndSaveUserInfoTesting, swipesLastUpdatedTimeRequiresUpdated) {

    const long swipes_last_updated_time = user_account_values.current_timestamp.count() / matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED.count();

    user_account_doc.number_swipes_remaining = 1;
    user_account_doc.swipes_last_updated_time = swipes_last_updated_time - 1;

    user_account_doc.setIntoCollection();

    runFunctionInsideTransaction();

    user_account_doc.number_swipes_remaining = matching_algorithm::MAXIMUM_NUMBER_SWIPES;
    user_account_doc.swipes_last_updated_time = swipes_last_updated_time;

    runBasicSuccessCheck();
}

TEST_F(ExtractAndSaveUserInfoTesting, hasBeenExtractedAccountsList_cleaned_byExpirationTime) {

    int index_to_drop = rand() % user_account_doc.has_been_extracted_accounts_list.size();
    user_account_doc.has_been_extracted_accounts_list[index_to_drop].expiration_time = bsoncxx::types::b_date{user_account_values.earliest_time_frame_start_timestamp};

    user_account_doc.setIntoCollection();

    runFunctionInsideTransaction();

    user_account_doc.has_been_extracted_accounts_list.erase(user_account_doc.has_been_extracted_accounts_list.begin() + index_to_drop);

    runBasicSuccessCheck();
}

TEST_F(ExtractAndSaveUserInfoTesting, otherUsersMatchedAccountsList_cleaned_byExpirationTime) {

    int index_to_drop = rand() % user_account_doc.other_users_matched_accounts_list.size();
    user_account_doc.other_users_matched_accounts_list[index_to_drop].expiration_time = bsoncxx::types::b_date{user_account_values.earliest_time_frame_start_timestamp};

    user_account_doc.setIntoCollection();

    runFunctionInsideTransaction();

    user_account_doc.other_users_matched_accounts_list.erase(user_account_doc.other_users_matched_accounts_list.begin() + index_to_drop);

    runBasicSuccessCheck();
}

TEST_F(ExtractAndSaveUserInfoTesting, algorithmMatchedAccountsList_cleaned_byExpirationTime) {

    int index_to_drop = rand() % user_account_doc.algorithm_matched_accounts_list.size();
    user_account_doc.algorithm_matched_accounts_list[index_to_drop].expiration_time = bsoncxx::types::b_date{user_account_values.earliest_time_frame_start_timestamp};

    user_account_doc.setIntoCollection();

    runFunctionInsideTransaction();

    user_account_doc.algorithm_matched_accounts_list.erase(user_account_doc.algorithm_matched_accounts_list.begin() + index_to_drop);

    runBasicSuccessCheck();
}

TEST_F(ExtractAndSaveUserInfoTesting, algorithmMatchedAccountsList_cleaned_byTimeBetweenAlgorithmMatchInvalidation) {
    user_account_doc.last_time_match_algorithm_ran = bsoncxx::types::b_date{user_account_values.current_timestamp - matching_algorithm::TIME_BETWEEN_ALGORITHM_MATCH_INVALIDATION - std::chrono::milliseconds{1}};

    user_account_doc.setIntoCollection();

    runFunctionInsideTransaction();

    user_account_doc.algorithm_matched_accounts_list.clear();

    runBasicSuccessCheck();
}

TEST_F(ExtractAndSaveUserInfoTesting, noSwipesRemaining) {
    user_account_doc.number_swipes_remaining = 0;

    user_account_doc.setIntoCollection();

    runFunctionInsideTransaction(false);

    EXPECT_TRUE(find_user_account_value);

    EXPECT_EQ(return_status, ReturnStatus::SUCCESS);
    EXPECT_EQ(success_type, findmatches::FindMatchesCapMessage_SuccessTypes_NO_SWIPES_REMAINING);

    checkUserAccountDocumentMatches();
}

TEST_F(ExtractAndSaveUserInfoTesting, userMatchedWithEveryone) {
    user_account_doc.genders_range.clear();
    user_account_doc.genders_range.emplace_back(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);

    user_account_doc.setIntoCollection();

    runFunctionInsideTransaction();

    EXPECT_TRUE(user_account_values.user_matches_with_everyone);

    runBasicSuccessCheck();
}

TEST_F(ExtractAndSaveUserInfoTesting, userMatchedDoesNotMatchWithEveryone) {
    user_account_doc.genders_range.clear();
    user_account_doc.genders_range.emplace_back(general_values::MALE_GENDER_VALUE);

    user_account_doc.setIntoCollection();

    runFunctionInsideTransaction();

    EXPECT_FALSE(user_account_values.user_matches_with_everyone);

    runBasicSuccessCheck();
}

