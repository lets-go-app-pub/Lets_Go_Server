//
// Created by jeremiah on 9/15/22.
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
#include <google/protobuf/util/message_differencer.h>

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "find_matches.h"
#include "setup_login_info.h"
#include "user_match_options.h"
#include "report_values.h"
#include "../find_matches/helper_functions/generate_matching_users.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class UpdateSingleMatchMemberTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc match_account_doc;

    user_match_options::UpdateSingleMatchMemberRequest request;
    UpdateOtherUserResponse response;

    void setUpValidRequest() {
        //setup with simple valid info
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front()
        );

        auto* member_info = request.mutable_chat_room_member_info();

        member_info->set_account_oid(match_account_oid.to_string());
        member_info->set_account_state(AccountStateInChatRoom(-1));

        member_info->set_first_name(match_account_doc.first_name);
        member_info->set_age(match_account_doc.age);
        member_info->set_member_info_last_updated_timestamp(getCurrentTimestamp().count());

        bool thumbnail_set = false;
        int index = 0;
        for(const auto& pic : match_account_doc.pictures) {

            if(pic.pictureStored()) {
                bsoncxx::oid picture_reference;
                bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

                pic.getPictureReference(picture_reference, timestamp_stored);

                if (!thumbnail_set) {
                    UserPictureDoc thumbnail_doc(picture_reference);

                    member_info->set_thumbnail_size_in_bytes(thumbnail_doc.thumbnail_size_in_bytes);
                    member_info->set_thumbnail_index_number(index);
                    member_info->set_thumbnail_timestamp(timestamp_stored.value.count());

                    thumbnail_set = true;
                }

                auto* pic_info = member_info->add_pictures_last_updated_timestamps();

                pic_info->set_index_number(index);
                pic_info->set_last_updated_timestamp(timestamp_stored.value.count());
            }
            index++;
        }
    }

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

        MatchingElement matching_element;

        //Make this a matching element that is not expired.
        matching_element.generateRandomValues();
        matching_element.oid = match_account_oid;
        matching_element.distance = 0;
        matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED};
        matching_element.from_match_algorithm_list = false;

        user_account_doc.has_been_extracted_accounts_list.emplace_back(
                matching_element
        );
        user_account_doc.setIntoCollection();

        setUpValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    template <bool check_match_account = true>
    void compareAccounts() {
        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);

        if(check_match_account) {
            UserAccountDoc extracted_match_account_doc(match_account_oid);
            EXPECT_EQ(match_account_doc, extracted_match_account_doc);
        }
    }

    void runFunction() {
        updateSingleMatchMember(
            &request, &response
        );
    }

};

TEST_F(UpdateSingleMatchMemberTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                request.mutable_login_info()->CopyFrom(login_info);

                runFunction();

                EXPECT_EQ(response.user_info().account_oid(), "");

                return response.return_status();
            }
    );

    compareAccounts();
}

TEST_F(UpdateSingleMatchMemberTesting, invalidAccountOid) {
    //NOTE: This simply makes sure filterAndStoreSingleUserToBeUpdated() runs, it is tested elsewhere.
    request.mutable_chat_room_member_info()->set_account_oid("invalid_account_oid");

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(response.user_info().account_oid(), "");

    compareAccounts();
}

TEST_F(UpdateSingleMatchMemberTesting, requestedUserNotInsideExtractedAccountList) {
    user_account_doc.has_been_extracted_accounts_list.pop_back();
    user_account_doc.setIntoCollection();

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_FALSE(response.match_still_valid());
    EXPECT_EQ(response.user_info().account_oid(), "");

    compareAccounts();
}

TEST_F(UpdateSingleMatchMemberTesting, requestedUserAccountNoLongerExists) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    auto delete_result = user_accounts_collection.delete_one(
        document{}
            << "_id" << match_account_oid
        << finalize
    );

    ASSERT_TRUE(delete_result);
    ASSERT_EQ(delete_result->result().deleted_count(), 1);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_FALSE(response.match_still_valid());
    EXPECT_EQ(response.user_info().account_oid(), "");

    compareAccounts<false>();
}

TEST_F(UpdateSingleMatchMemberTesting, successful) {
    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_TRUE(response.match_still_valid());

    //If the function updateSingleOtherUser() ran everything else will be set too. The function is checked
    // elsewhere.
    EXPECT_EQ(response.user_info().account_oid(), match_account_oid.to_string());

    compareAccounts();
}
