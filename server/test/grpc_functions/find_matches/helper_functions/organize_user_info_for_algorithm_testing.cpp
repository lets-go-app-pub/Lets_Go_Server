//
// Created by jeremiah on 9/2/22.
//

#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include "generate_matching_users.h"
#include "user_account_keys.h"

#include "helper_functions/find_matches_helper_functions.h"
#include "generate_multiple_random_accounts.h"
#include "extract_data_from_bsoncxx.h"
#include "generate_randoms.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ClearOidArrayElementTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    UserAccountValues user_account_values;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

};

class ClearStringArrayElementTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    UserAccountValues user_account_values;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        user_account_doc.other_users_blocked.clear();
        user_account_doc.other_users_blocked.emplace_back(
                bsoncxx::oid{}.to_string(),
                bsoncxx::types::b_date{std::chrono::milliseconds{123}}
        );

        user_account_doc.setIntoCollection();

        //insert an invalid OID_STRING (it should be type utf8 not type oid)
        user_accounts_collection.update_one(
            document{}
                << "_id" << user_account_oid
            << finalize,
            document{}
                << "$push" << open_document
                    << user_account_keys::OTHER_USERS_BLOCKED << open_document
                        << user_account_keys::other_users_blocked::OID_STRING << bsoncxx::oid{}
                        << user_account_keys::other_users_blocked::TIMESTAMP_BLOCKED << bsoncxx::types::b_date{std::chrono::milliseconds{456}}
                    << close_document
                << close_document
            << finalize
        );

        const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_doc.current_object_oid;

    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

};

class SaveRestrictedOIDToArrayTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    UserAccountValues user_account_values;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    std::vector<bsoncxx::oid> storeRestrictedAccountsToVectorAndSort() {
        std::vector<bsoncxx::oid> restricted_accounts_copy;
        std::copy(user_account_values.restricted_accounts.begin(), user_account_values.restricted_accounts.end(), std::back_inserter(restricted_accounts_copy));

        std::sort(restricted_accounts_copy.begin(), restricted_accounts_copy.end());

        return restricted_accounts_copy;
    }
};

class StoreRestrictedAccountsTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    UserAccountValues user_account_values;

    bsoncxx::builder::stream::document user_account_doc_builder;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        user_account_doc.convertToDocument(user_account_doc_builder);

        const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_oid;
        user_account_values.user_account_doc_view = user_account_doc_builder;
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runCheck(const bsoncxx::oid& added_account_oid) {

        user_account_doc.setIntoCollection();
        user_account_doc_builder.clear();
        user_account_doc.convertToDocument(user_account_doc_builder);
        user_account_values.user_account_doc_view = user_account_doc_builder;

        storeRestrictedAccounts(
                user_account_values,
                nullptr,
                user_accounts_collection
        );

        ASSERT_EQ(user_account_values.restricted_accounts.size(), 2);

        auto it = user_account_values.restricted_accounts.begin();
        const std::string first_oid =it->to_string();
        it++;
        const std::string second_oid = it->to_string();

        EXPECT_TRUE(
                (first_oid == user_account_oid.to_string()
                && second_oid == added_account_oid.to_string())
                ||
                (first_oid == added_account_oid.to_string()
                 && second_oid == user_account_oid.to_string())
                );
    }

    void compareMatchesArray(std::vector<MatchingElement>& vector_to_add) {

        bsoncxx::oid added_account_oid{};

        vector_to_add.emplace_back(
                added_account_oid,
                1234.5678,
                52.1,
                bsoncxx::types::b_date{std::chrono::milliseconds{456}},
                bsoncxx::types::b_date{std::chrono::milliseconds{123}},
                true,
                bsoncxx::oid{}
        );

        runCheck(added_account_oid);
    }

    template <typename T>
    void compareArrayWithStringOid(std::vector<T>& vector_to_add) {

        bsoncxx::oid added_account_oid{};

        vector_to_add.emplace_back(
                added_account_oid.to_string(),
                bsoncxx::types::b_date{std::chrono::milliseconds{456}}
        );

        runCheck(added_account_oid);
    }

    void compareArrayWithOid(std::vector<UserAccountDoc::SingleChatRoom>& vector_to_add) {

        bsoncxx::oid added_account_oid{};

        vector_to_add.emplace_back(
                generateRandomChatRoomId(),
                bsoncxx::types::b_date{std::chrono::milliseconds{456}},
                added_account_oid
        );

        runCheck(added_account_oid);
    }

};

class BuildCategoriesArrayForCurrentUserTesting : public ::testing::Test {
protected:

    int index_value = rand() % 100;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    UserAccountValues user_account_values;

    bsoncxx::builder::stream::document user_account_doc_builder;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        user_account_doc.convertToDocument(user_account_doc_builder);

        const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_oid;
        user_account_values.user_account_doc_view = user_account_doc_builder;

        const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{250};
        const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{300};;
        const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{500};
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    template <bool start_time>
    void compareTimeframeValue(
            const TimeFrameStruct& time_frame,
            const std::chrono::milliseconds& expected_time
            ) const {
        EXPECT_EQ(time_frame.time, expected_time);
        EXPECT_EQ(time_frame.startStopValue, -1 + 2 * start_time);
        EXPECT_EQ(time_frame.fromUser, true);
    }

    void compareActivityStructNonTimeframe(
            const ActivityStruct& activity_or_category,
            const std::chrono::milliseconds& expected_total_time,
            const size_t& expected_time_frames_size
    ) const {
        EXPECT_EQ(activity_or_category.activityIndex, index_value);
        EXPECT_EQ(activity_or_category.totalTime, expected_total_time);
        EXPECT_EQ(activity_or_category.timeFrames.size(), expected_time_frames_size);
    }

    void compareActivityStructTwoTimeFrames(
            const ActivityStruct& activity_or_category,
            const std::chrono::milliseconds& expected_start_time,
            const std::chrono::milliseconds& expected_stop_time
            ) const {
        EXPECT_EQ(activity_or_category.activityIndex, index_value);
        EXPECT_EQ(activity_or_category.totalTime, expected_stop_time - expected_start_time);
        EXPECT_EQ(activity_or_category.timeFrames.size(), 2);
        if(activity_or_category.timeFrames.size() == 2) {
            compareTimeframeValue<true>(
                    activity_or_category.timeFrames[0],
                    expected_start_time
            );

            compareTimeframeValue<false>(
                    activity_or_category.timeFrames[1],
                    expected_stop_time
            );
        }
    }

    void saveToCategories(
            const std::vector<TestCategory>& categories
            ) {
        user_account_doc.categories.clear();

        std::copy(categories.begin(), categories.end(), std::back_inserter(user_account_doc.categories));

        user_account_doc_builder.clear();
        user_account_doc.convertToDocument(user_account_doc_builder);
        user_account_values.user_account_doc_view = user_account_doc_builder;
    }
};

class OrganizeUserInfoForAlgorithmTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc match_account_doc;

    UserAccountValues user_account_values;

    bsoncxx::builder::stream::document user_account_doc_builder;
    bsoncxx::builder::basic::array user_gender_range_builder;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        ASSERT_TRUE(
                generateMatchingUsers(
                        user_account_doc,
                        match_account_doc,
                        user_account_oid,
                        match_account_oid,
                        user_account_values,
                        user_gender_range_builder
                )
        );

        user_account_doc.search_by_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

        user_account_doc.convertToDocument(user_account_doc_builder);

        const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_oid;
        user_account_values.user_account_doc_view = user_account_doc_builder;

        const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{250};
        const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{300};;
        const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{500};
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

};

TEST_F(ClearOidArrayElementTesting, properlyClearsArray) {

    user_account_doc.has_been_extracted_accounts_list.clear();

    user_account_doc.has_been_extracted_accounts_list.emplace_back(
            bsoncxx::oid{},
            1234.5678,
            52.1,
            bsoncxx::types::b_date{std::chrono::milliseconds{456}},
            bsoncxx::types::b_date{std::chrono::milliseconds{123}},
            true,
            bsoncxx::oid{}
    );

    user_account_doc.setIntoCollection();

    const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_doc.current_object_oid;

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session
    ) {
        bool return_val = clearOidArrayElement(
                user_account_values,
                user_accounts_collection,
                session,
                user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST
        );

        EXPECT_TRUE(return_val);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "dummy_string");
    }

    user_account_doc.has_been_extracted_accounts_list.clear();

    UserAccountDoc generated_user_account_doc(user_account_oid);

    EXPECT_EQ(generated_user_account_doc, user_account_doc);
}

TEST_F(ClearOidArrayElementTesting, notInsideTransaction) {

    user_account_doc.has_been_extracted_accounts_list.clear();

    user_account_doc.has_been_extracted_accounts_list.emplace_back(
            bsoncxx::oid{},
            1234.5678,
            52.1,
            bsoncxx::types::b_date{std::chrono::milliseconds{456}},
            bsoncxx::types::b_date{std::chrono::milliseconds{123}},
            true,
            bsoncxx::oid{}
    );

    user_account_doc.setIntoCollection();

    const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_doc.current_object_oid;

    bool return_val = clearOidArrayElement(
            user_account_values,
            user_accounts_collection,
            nullptr,
            user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST
    );

    EXPECT_FALSE(return_val);

    UserAccountDoc generated_user_account_doc(user_account_oid);

    EXPECT_EQ(generated_user_account_doc, user_account_doc);
}

TEST_F(ClearOidArrayElementTesting, accountDoesNotExist) {

    const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = bsoncxx::oid{};

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session
    ) {
        bool return_val = clearOidArrayElement(
                user_account_values,
                user_accounts_collection,
                session,
                user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST
        );

        EXPECT_FALSE(return_val);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "dummy_string");
    }

}

TEST_F(ClearStringArrayElementTesting, properlyClearsArray) {

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session
    ) {
        bool return_val = clearStringArrayElement(
                user_account_values,
                user_accounts_collection,
                session,
                user_account_keys::OTHER_USERS_BLOCKED,
                user_account_keys::other_users_blocked::OID_STRING
        );

        EXPECT_TRUE(return_val);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "dummy_string");
    }

    //cannot use the UserAccountDoc because it could contain an invalid type
    auto doc_result = user_accounts_collection.find_one(
            document{}
                << "_id" << user_account_oid
            << finalize
            );

    ASSERT_TRUE(doc_result);

    auto other_users_blocked_arr = doc_result->view()[user_account_keys::OTHER_USERS_BLOCKED].get_array().value;

    int num_elements = 0;
    for(const auto& ele : other_users_blocked_arr) {
        bsoncxx::document::view other_user_blocked_doc = ele.get_document();

        auto oid_string_ele = other_user_blocked_doc[user_account_keys::other_users_blocked::OID_STRING];
        ASSERT_EQ(oid_string_ele.type(), bsoncxx::type::k_utf8);
        EXPECT_EQ(user_account_doc.other_users_blocked.front().oid_string, other_user_blocked_doc[user_account_keys::other_users_blocked::OID_STRING].get_string().value.to_string());
        num_elements++;
    }

    EXPECT_EQ(num_elements, 1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    EXPECT_EQ(extracted_user_account_doc, user_account_doc);
}

TEST_F(ClearStringArrayElementTesting, notInsideTransaction) {

    bool return_val = clearStringArrayElement(
            user_account_values,
            user_accounts_collection,
            nullptr,
            user_account_keys::OTHER_USERS_BLOCKED,
            user_account_keys::other_users_blocked::OID_STRING
    );

    EXPECT_FALSE(return_val);

    //cannot use the UserAccountDoc because it could contain an invalid type
    auto doc_result = user_accounts_collection.find_one(
            document{}
                    << "_id" << user_account_oid
                    << finalize
    );

    ASSERT_TRUE(doc_result);

    auto other_users_blocked_arr = doc_result->view()[user_account_keys::OTHER_USERS_BLOCKED].get_array().value;

    int num_elements = std::distance(other_users_blocked_arr.begin(), other_users_blocked_arr.end());

    EXPECT_EQ(num_elements, 2);
}

TEST_F(ClearStringArrayElementTesting, accountDoesNotExist) {

    const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = bsoncxx::oid{};

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session
    ) {
        bool return_val = clearStringArrayElement(
                user_account_values,
                user_accounts_collection,
                session,
                user_account_keys::OTHER_USERS_BLOCKED,
                user_account_keys::other_users_blocked::OID_STRING
        );

        EXPECT_FALSE(return_val);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "dummy_string");
    }

    //cannot use the UserAccountDoc because it could contain an invalid type
    auto doc_result = user_accounts_collection.find_one(
            document{}
                    << "_id" << user_account_oid
                    << finalize
    );

    ASSERT_TRUE(doc_result);

    auto other_users_blocked_arr = doc_result->view()[user_account_keys::OTHER_USERS_BLOCKED].get_array().value;

    int num_elements = std::distance(other_users_blocked_arr.begin(), other_users_blocked_arr.end());

    EXPECT_EQ(num_elements, 2);
}

TEST_F(SaveRestrictedOIDToArrayTesting, fieldTypeIsString_true) {
    bsoncxx::builder::stream::document extract_from_doc;

    user_account_doc.other_users_blocked.clear();
    user_account_doc.other_users_blocked.emplace_back(
            bsoncxx::oid{}.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{123}}
    );
    user_account_doc.other_users_blocked.emplace_back(
            bsoncxx::oid{}.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{456}}
    );

    user_account_doc.convertToDocument(extract_from_doc);

    bool return_val = saveRestrictedOIDToArray<true>(
            user_account_values,
            user_account_keys::OTHER_USERS_BLOCKED,
            user_account_keys::other_users_blocked::OID_STRING,
            extract_from_doc,
            true
    );

    EXPECT_TRUE(return_val);
    ASSERT_EQ(user_account_doc.other_users_blocked.size(), user_account_values.restricted_accounts.size());

    std::vector<bsoncxx::oid> restricted_accounts_copy = storeRestrictedAccountsToVectorAndSort();

    std::sort(
            user_account_doc.other_users_blocked.begin(),
            user_account_doc.other_users_blocked.end(), [](const UserAccountDoc::OtherBlockedUser& l, const UserAccountDoc::OtherBlockedUser& r){
                return l.oid_string < r.oid_string;
            });

    for(int i = 0; i < (int)user_account_doc.other_users_blocked.size(); ++i) {
        EXPECT_EQ(user_account_doc.other_users_blocked[i].oid_string, restricted_accounts_copy[i].to_string());
    }
}

TEST_F(SaveRestrictedOIDToArrayTesting, fieldTypeIsString_false) {
    bsoncxx::builder::stream::document extract_from_doc;

    user_account_doc.has_been_extracted_accounts_list.clear();
    user_account_doc.has_been_extracted_accounts_list.emplace_back(
            bsoncxx::oid{},
            1234.5678,
            52.1,
            bsoncxx::types::b_date{std::chrono::milliseconds{456}},
            bsoncxx::types::b_date{std::chrono::milliseconds{123}},
            true,
            bsoncxx::oid{}
    );

    user_account_doc.convertToDocument(extract_from_doc);

    bool return_val = saveRestrictedOIDToArray<false>(
            user_account_values,
            user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
            user_account_keys::accounts_list::OID,
            extract_from_doc,
            true
    );

    EXPECT_TRUE(return_val);
    ASSERT_EQ(user_account_doc.has_been_extracted_accounts_list.size(), user_account_values.restricted_accounts.size());

    std::vector<bsoncxx::oid> restricted_accounts_copy = storeRestrictedAccountsToVectorAndSort();

    std::sort(
            user_account_doc.has_been_extracted_accounts_list.begin(),
            user_account_doc.has_been_extracted_accounts_list.end(), [](const MatchingElement& l, const MatchingElement& r){
                return l.oid.to_string() < r.oid.to_string();
            });

    for(int i = 0; i < (int)user_account_doc.has_been_extracted_accounts_list.size(); ++i) {
        EXPECT_EQ(user_account_doc.has_been_extracted_accounts_list[i].oid.to_string(), restricted_accounts_copy[i].to_string());
    }
}

TEST_F(SaveRestrictedOIDToArrayTesting, invalidFieldType) {
    bsoncxx::builder::stream::document extract_from_doc;

    user_account_doc.has_been_extracted_accounts_list.clear();
    user_account_doc.has_been_extracted_accounts_list.emplace_back(
            bsoncxx::oid{},
            1234.5678,
            52.1,
            bsoncxx::types::b_date{std::chrono::milliseconds{456}},
            bsoncxx::types::b_date{std::chrono::milliseconds{123}},
            true,
            bsoncxx::oid{}
    );

    user_account_doc.convertToDocument(extract_from_doc);

    bool return_val = saveRestrictedOIDToArray<true>(
            user_account_values,
            user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
            user_account_keys::accounts_list::OID,
            extract_from_doc,
            true
    );

    EXPECT_FALSE(return_val);
    ASSERT_EQ(0, user_account_values.restricted_accounts.size());
}

TEST_F(SaveRestrictedOIDToArrayTesting, noValuesToStore) {
    bsoncxx::builder::stream::document extract_from_doc;

    user_account_doc.has_been_extracted_accounts_list.clear();

    user_account_doc.convertToDocument(extract_from_doc);

    bool return_val = saveRestrictedOIDToArray<false>(
            user_account_values,
            user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
            user_account_keys::accounts_list::OID,
            extract_from_doc,
            true
    );

    EXPECT_TRUE(return_val);
    ASSERT_EQ(user_account_doc.has_been_extracted_accounts_list.size(), user_account_values.restricted_accounts.size());

    std::vector<bsoncxx::oid> restricted_accounts_copy = storeRestrictedAccountsToVectorAndSort();

    std::sort(
            user_account_doc.has_been_extracted_accounts_list.begin(),
            user_account_doc.has_been_extracted_accounts_list.end(), [](const MatchingElement& l, const MatchingElement& r){
                return l.oid.to_string() < r.oid.to_string();
            });

    for(int i = 0; i < (int)user_account_doc.has_been_extracted_accounts_list.size(); ++i) {
        EXPECT_EQ(user_account_doc.has_been_extracted_accounts_list[i].oid.to_string(), restricted_accounts_copy[i].to_string());
    }
}

TEST_F(SaveRestrictedOIDToArrayTesting, fieldDoesNotExist_parameterWorks) {
    bsoncxx::builder::stream::document extract_from_doc;

    user_account_doc.chat_rooms.clear();
    user_account_doc.chat_rooms.emplace_back(
            generateRandomChatRoomId(),
            bsoncxx::types::b_date{std::chrono::milliseconds{rand() % 10000000 + 10}}
    );

    user_account_doc.chat_rooms.emplace_back(
            generateRandomChatRoomId(),
            bsoncxx::types::b_date{std::chrono::milliseconds{rand() % 10000000 + 10}},
            bsoncxx::oid{}
    );

    user_account_doc.convertToDocument(extract_from_doc);

    bool return_val = saveRestrictedOIDToArray<false>(
            user_account_values,
            user_account_keys::CHAT_ROOMS,
            user_account_keys::chat_rooms::EVENT_OID,
            extract_from_doc,
            false
    );

    EXPECT_TRUE(return_val);
    ASSERT_EQ(1, user_account_values.restricted_accounts.size());
}

TEST_F(StoreRestrictedAccountsTesting, onlyStoreCurrentAccountOid) {

    storeRestrictedAccounts(
            user_account_values,
            nullptr,
            user_accounts_collection
    );

    ASSERT_EQ(user_account_values.restricted_accounts.size(), 1);
    EXPECT_EQ(user_account_values.restricted_accounts.begin()->to_string(), user_account_oid.to_string());
}

TEST_F(StoreRestrictedAccountsTesting, extractAndStore_hasBeenExtracted) {
    compareMatchesArray(user_account_doc.has_been_extracted_accounts_list);
}

TEST_F(StoreRestrictedAccountsTesting, extractAndStore_otherUsersMatched) {
    compareMatchesArray(user_account_doc.other_users_matched_accounts_list);
}

TEST_F(StoreRestrictedAccountsTesting, extractAndStore_algorithmMatched) {
    compareMatchesArray(user_account_doc.algorithm_matched_accounts_list);
}

TEST_F(StoreRestrictedAccountsTesting, extractAndStore_matchedWith) {
    compareArrayWithStringOid<UserAccountDoc::OtherAccountMatchedWith>(user_account_doc.other_accounts_matched_with);
}

TEST_F(StoreRestrictedAccountsTesting, extractAndStore_otherUserBlocked) {
    compareArrayWithStringOid<UserAccountDoc::OtherBlockedUser>(user_account_doc.other_users_blocked);
}

TEST_F(StoreRestrictedAccountsTesting, extractAndStore_chatRooms) {
    compareArrayWithOid(user_account_doc.chat_rooms);
}

TEST_F(StoreRestrictedAccountsTesting, extractAndStore_previouslyMatched_underMinimumTime) {
    bsoncxx::oid added_account_oid{};

    //previously_matched_timestamp + matching_algorithm::TIME_BETWEEN_SAME_ACCOUNT_MATCHES) >;
    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{1000};

    user_account_doc.previously_matched_accounts.emplace_back(
            added_account_oid,
            bsoncxx::types::b_date{user_account_values.current_timestamp},
            1
    );

    storeRestrictedAccounts(
            user_account_values,
            nullptr,
            user_accounts_collection
    );

    ASSERT_EQ(user_account_values.restricted_accounts.size(), 1);
    EXPECT_EQ(user_account_values.restricted_accounts.begin()->to_string(), user_account_oid.to_string());
}

TEST_F(StoreRestrictedAccountsTesting, extractAndStore_previouslyMatched_overMinimumTime) {
    bsoncxx::oid added_account_oid{};

    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{1000};

    user_account_doc.previously_matched_accounts.emplace_back(
            added_account_oid,
            bsoncxx::types::b_date{user_account_values.current_timestamp + matching_algorithm::TIME_BETWEEN_SAME_ACCOUNT_MATCHES + std::chrono::milliseconds{1}},
            1
    );

    runCheck(added_account_oid);
}

TEST_F(BuildCategoriesArrayForCurrentUserTesting, emptyTimeframeArray) {

    saveToCategories(
        std::vector<TestCategory>{
                TestCategory(AccountCategoryType::ACTIVITY_TYPE, index_value)
        }
    );

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    bool result_var = buildCategoriesArrayForCurrentUser(user_account_values);

    EXPECT_TRUE(result_var);

    EXPECT_EQ(user_account_values.user_categories.size(), 0);
    EXPECT_EQ(user_account_values.user_activities.size(), 1);
    if(!user_account_values.user_activities.empty()) {
        compareActivityStructTwoTimeFrames(
                user_account_values.user_activities[0],
                user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{1},
                user_account_values.end_of_time_frame_timestamp
        );
    }
}

TEST_F(BuildCategoriesArrayForCurrentUserTesting, firstTimeIsStopTime) {

    saveToCategories(
            std::vector<TestCategory>{
                    TestCategory(
                            AccountCategoryType::ACTIVITY_TYPE,
                            index_value,
                            std::vector<TestTimeframe>{
                                    TestTimeframe(user_account_values.earliest_time_frame_start_timestamp.count() + 100, -1)
                            }
                    )
            }
    );

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    bool result_var = buildCategoriesArrayForCurrentUser(user_account_values);

    EXPECT_TRUE(result_var);

    EXPECT_EQ(user_account_values.user_categories.size(), 0);
    EXPECT_EQ(user_account_values.user_activities.size(), 1);
    if(!user_account_values.user_activities.empty()) {
        compareActivityStructTwoTimeFrames(
                user_account_values.user_activities[0],
                user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{1},
                std::chrono::milliseconds{user_account_doc.categories.front().time_frames.front().time}
        );
    }
}

TEST_F(BuildCategoriesArrayForCurrentUserTesting, noActivitiesOrCategories) {

    saveToCategories(
        std::vector<TestCategory>{}
    );

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    bool result_var = buildCategoriesArrayForCurrentUser(user_account_values);

    EXPECT_FALSE(result_var);

    EXPECT_EQ(user_account_values.user_categories.size(), 0);
    EXPECT_EQ(user_account_values.user_activities.size(), 0);
}

TEST_F(BuildCategoriesArrayForCurrentUserTesting, categoryProperlyExtracted) {

    saveToCategories(
            std::vector<TestCategory>{
                    TestCategory(AccountCategoryType::CATEGORY_TYPE, index_value)
            }
    );

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    bool result_var = buildCategoriesArrayForCurrentUser(user_account_values);

    EXPECT_TRUE(result_var);

    EXPECT_EQ(user_account_values.user_categories.size(), 1);
    EXPECT_EQ(user_account_values.user_activities.size(), 0);
    if(!user_account_values.user_categories.empty()) {
        compareActivityStructTwoTimeFrames(
                user_account_values.user_categories[0],
                user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{1},
                user_account_values.end_of_time_frame_timestamp
        );
    }
}

TEST_F(BuildCategoriesArrayForCurrentUserTesting, standardTimeframes) {

    saveToCategories(
            std::vector<TestCategory>{
                    TestCategory(
                            AccountCategoryType::ACTIVITY_TYPE,
                            index_value,
                            std::vector<TestTimeframe>{
                                    TestTimeframe(user_account_values.earliest_time_frame_start_timestamp.count() + 50, 1),
                                    TestTimeframe(user_account_values.earliest_time_frame_start_timestamp.count() + 100, -1)
                            }
                    )
            }
    );

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    bool result_var = buildCategoriesArrayForCurrentUser(user_account_values);

    EXPECT_TRUE(result_var);

    EXPECT_EQ(user_account_values.user_categories.size(), 0);
    EXPECT_EQ(user_account_values.user_activities.size(), 1);
    if(!user_account_values.user_activities.empty()) {
        compareActivityStructTwoTimeFrames(
                user_account_values.user_activities[0],
                std::chrono::milliseconds{user_account_doc.categories.front().time_frames[0].time},
                std::chrono::milliseconds{user_account_doc.categories.front().time_frames[1].time}
        );
    }
}

TEST_F(BuildCategoriesArrayForCurrentUserTesting, multipleStandardTimeframes) {
    saveToCategories(
            std::vector<TestCategory>{
                    TestCategory(
                            AccountCategoryType::ACTIVITY_TYPE,
                            index_value,
                            std::vector<TestTimeframe>{
                                    TestTimeframe(user_account_values.earliest_time_frame_start_timestamp.count() + 50, 1),
                                    TestTimeframe(user_account_values.earliest_time_frame_start_timestamp.count() + 100, -1),
                                    TestTimeframe(user_account_values.earliest_time_frame_start_timestamp.count() + 120, 1),
                                    TestTimeframe(user_account_values.earliest_time_frame_start_timestamp.count() + 133, -1)
                            }
                    )
            }
    );

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

    bool result_var = buildCategoriesArrayForCurrentUser(user_account_values);

    EXPECT_TRUE(result_var);

    EXPECT_EQ(user_account_values.user_categories.size(), 0);
    EXPECT_EQ(user_account_values.user_activities.size(), 1);
    if(!user_account_values.user_activities.empty()) {

        compareActivityStructNonTimeframe(
                user_account_values.user_activities[0],
                (std::chrono::milliseconds{user_account_doc.categories.front().time_frames[3].time} -
                std::chrono::milliseconds{user_account_doc.categories.front().time_frames[2].time}) +

                (std::chrono::milliseconds{user_account_doc.categories.front().time_frames[1].time} -
                std::chrono::milliseconds{user_account_doc.categories.front().time_frames[0].time}),
                4
        );

        compareTimeframeValue<true>(
                user_account_values.user_activities[0].timeFrames[0],
                std::chrono::milliseconds{user_account_doc.categories.front().time_frames[0].time}
        );

        compareTimeframeValue<false>(
                user_account_values.user_activities[0].timeFrames[1],
                std::chrono::milliseconds{user_account_doc.categories.front().time_frames[1].time}
        );

        compareTimeframeValue<true>(
                user_account_values.user_activities[0].timeFrames[2],
                std::chrono::milliseconds{user_account_doc.categories.front().time_frames[2].time}
        );

        compareTimeframeValue<false>(
                user_account_values.user_activities[0].timeFrames[3],
                std::chrono::milliseconds{user_account_doc.categories.front().time_frames[3].time}
        );
    }
}

//NOTE: organizeUserInfoForAlgorithm() is tested inside begin_algorithm_check_cooldown_testing.cpp because the
// Google suite is nearly identical.
