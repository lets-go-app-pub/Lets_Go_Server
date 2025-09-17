//
// Created by jeremiah on 6/16/22.
//

#include <gtest/gtest.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <admin_functions_for_set_values.h>
#include <clear_database_for_testing.h>
#include <generate_multiple_random_accounts.h>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>
#include <utility_general_functions.h>
#include "specific_match_queries/specific_match_queries.h"
#include "event_request_message_is_valid.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "generate_randoms.h"
#include <mongocxx/collection.hpp>
#include <account_objects.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class BuildValidateMatchSpecifics : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        //Must be generated after the database is cleared.
        generated_account_oid = insertRandomAccounts(1, 0);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    static void createEvent() {
        createAndStoreEventAdminAccount();

        const EventChatRoomAdminInfo chat_room_admin_info(
                UserAccountType::ADMIN_GENERATED_EVENT_TYPE
        );

        const auto current_timestamp = getCurrentTimestamp();

        generateRandomEventAndEventInfo(
                chat_room_admin_info,
                TEMP_ADMIN_ACCOUNT_NAME,
                current_timestamp
        );
    }
};

TEST_F(BuildValidateMatchSpecifics, activitiesOfMatchHaveNotBeenUpdated) {

    bsoncxx::builder::stream::document matchDoc;

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    activitiesOfMatchHaveNotBeenUpdated(matchDoc, current_timestamp);

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, atLeastOneCategoryMatches) {

    bsoncxx::builder::stream::document matchDoc;

    UserAccountDoc user_account(generated_account_oid);

    bsoncxx::builder::basic::array index_values;

    index_values.append(user_account.categories.front().index_value);

    atLeastOneCategoryMatches(
            matchDoc,
            index_values,
            AccountCategoryType::ACTIVITY_TYPE
            );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 1);

}

TEST_F(BuildValidateMatchSpecifics, atLeastOneCategoryMatches_noneMatch) {

    bsoncxx::builder::stream::document matchDoc;

    UserAccountDoc user_account(generated_account_oid);

    bsoncxx::builder::basic::array index_values;

    std::set<int> activity_index_values;

    for(const auto& category : user_account.categories) {
        if(category.type == AccountCategoryType::ACTIVITY_TYPE) {
            activity_index_values.insert(category.index_value);
        }
    }

    int index_that_does_not_exist = 0;

    for(int i = 1; i < 500000; i++) {
        if(!activity_index_values.contains(i)) {
            index_that_does_not_exist = i;
            break;
        }
    }

    index_values.append(index_that_does_not_exist);

    atLeastOneCategoryMatches(
            matchDoc,
            index_values,
            AccountCategoryType::ACTIVITY_TYPE
            );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 0);
}

TEST_F(BuildValidateMatchSpecifics, distanceStillInMatchMaxDistance) {

    bsoncxx::builder::stream::document matchDoc;

    UserAccountDoc user_account(generated_account_oid);

    distanceStillInMatchMaxDistance(
            matchDoc,
            user_account.max_distance - 1
            );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, matchingIsActivatedOnMatchAccount) {

    bsoncxx::builder::stream::document matchDoc;

    matchingIsActivatedOnMatchAccount(
            matchDoc
            );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, matchAgeInsideUserAgeRangeOrEvent) {

    UserAccountDoc user_account(generated_account_oid);

    createEvent();

    bsoncxx::builder::stream::document matchDoc;

    matchAgeInsideUserAgeRangeOrEvent(
            matchDoc,
            user_account.age - 1,
            user_account.age + 1,
            false
    );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 2);
}

TEST_F(BuildValidateMatchSpecifics, matchAgeInsideUserAgeRangeOrEvent_onlyMatchWithEvents) {

    UserAccountDoc user_account(generated_account_oid);

    createEvent();

    bsoncxx::builder::stream::document matchDoc;

    matchAgeInsideUserAgeRangeOrEvent(
            matchDoc,
            user_account.age - 1,
            user_account.age + 1,
            true
    );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, matchGenderInsideUserGenderRangeOrEvent) {

    UserAccountDoc user_account(generated_account_oid);

    createEvent();

    bsoncxx::builder::basic::array genders_to_match_with;
    genders_to_match_with.append(user_account.gender);

    bsoncxx::builder::stream::document matchDoc;

    matchGenderInsideUserGenderRangeOrEvent(
            matchDoc,
            genders_to_match_with,
            false,
            false
    );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 2);
}

TEST_F(BuildValidateMatchSpecifics, matchGenderInsideUserGenderRangeOrEvent_onlyMatchWithEvents) {

    UserAccountDoc user_account(generated_account_oid);

    createEvent();

    bsoncxx::builder::basic::array genders_to_match_with;
    genders_to_match_with.append(user_account.gender);

    bsoncxx::builder::stream::document matchDoc;

    matchGenderInsideUserGenderRangeOrEvent(
            matchDoc,
            genders_to_match_with,
            false,
            true
    );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, userAgeInsideMatchAgeRange) {

    UserAccountDoc user_account(generated_account_oid);

    int valid_age = (user_account.age_range.min + user_account.age_range.max)/2;

    bsoncxx::builder::stream::document matchDoc;

    userAgeInsideMatchAgeRange(
            matchDoc,
            valid_age
    );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, eventExpirationNotReached) {

    UserAccountDoc user_account(generated_account_oid);

    bsoncxx::builder::stream::document matchDoc;

    eventExpirationNotReached(
            matchDoc,
            getCurrentTimestamp()
    );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, userGenderInsideMatchGenderRange) {

    UserAccountDoc user_account(generated_account_oid);

    std::string valid_gender = user_account.genders_range.front();

    bsoncxx::builder::stream::document matchDoc;

    userGenderInsideMatchGenderRange(
            matchDoc,
            valid_gender
            );

    long number = user_accounts_collection.count_documents(matchDoc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, userNotOnMatchBlockedList) {

    UserAccountDoc user_account(generated_account_oid);

    std::string valid_oid = bsoncxx::oid{}.to_string();

    bsoncxx::builder::stream::document match_doc;

    userNotOnMatchBlockedList(
            match_doc,
            valid_oid
            );

    long number = user_accounts_collection.count_documents(match_doc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, userNotRecentMatchForMatch) {

    UserAccountDoc user_account(generated_account_oid);

    bsoncxx::oid valid_oid{};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bsoncxx::builder::stream::document match_doc;

    userNotRecentMatchForMatch(
            match_doc,
            valid_oid,
            current_timestamp
            );

    long number = user_accounts_collection.count_documents(match_doc.view());

    EXPECT_EQ(number, 1);
}

TEST_F(BuildValidateMatchSpecifics, universalMatchConditions) {
    //NOTE: This calls a series of the other functions tested above, so it has no direct test.
}