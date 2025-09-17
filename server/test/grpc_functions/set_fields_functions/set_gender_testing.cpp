//
// Created by jeremiah on 10/7/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "chat_room_shared_keys.h"
#include "setup_login_info.h"
#include "connection_pool_global_variable.h"
#include "ManageServerCommands.pb.h"
#include "generate_multiple_random_accounts.h"
#include "set_fields_functions.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetGenderTesting : public testing::Test {
protected:

    setfields::SetStringRequest request;
    setfields::SetFieldResponse response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    UserAccountStatisticsDoc user_account_statistics;

    void setupValidAdminRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.mutable_login_info()->set_current_account_id(user_account_oid.to_string());

        request.set_set_string("random_name");
    }

    void setupValidUserRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        request.set_set_string("random_name");
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(user_account_oid);

        user_account_statistics.getFromCollection(user_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(bool use_admin_info, AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        if (use_admin_info) {
            createTempAdminAccount(admin_level);
        }

        //guarantee different timestamp used
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        setGender(&request, &response);
    }

    void compareUserAccounts() {
        UserAccountDoc extracted_user_account(user_account_oid);
        EXPECT_EQ(user_account, extracted_user_account);
    }

    void compareUserStatisticsAccounts() {
        UserAccountStatisticsDoc extracted_user_account_statistics(user_account_oid);
        EXPECT_EQ(user_account_statistics, extracted_user_account_statistics);
    }

    bool randomizeClientOrAdmin() {
        const bool use_admin_info = rand() % 2;

        if (use_admin_info) { //use admin info
            setupValidAdminRequest();
        } else { //use client info
            setupValidUserRequest();
        }

        return use_admin_info;
    }

};

TEST_F(SetGenderTesting, invalidLoginInfo) {
    setupValidUserRequest();
    createTempAdminAccount(AdminLevelEnum::PRIMARY_DEVELOPER);

    checkLoginInfoAdminAndClient(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            user_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();

                runFunction(false);

                return response.return_status();
            }
    );

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderTesting, noAdminPriveledge) {
    setupValidAdminRequest();

    runFunction(true, AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_EQ(response.return_status(), ReturnStatus::LG_ERROR);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderTesting, invalidUserAccountOidPassedAsAdmin) {
    setupValidAdminRequest();

    request.mutable_login_info()->set_current_account_id("invalid_account_oid");

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_USER_OID);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderTesting, invalidGender_tooShort) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_set_string("");

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), use_admin_info ? ReturnStatus::LG_ERROR : ReturnStatus::INVALID_PARAMETER_PASSED);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderTesting, invalidGender_tooLong) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_set_string(std::string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1, 'a'));

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), use_admin_info ? ReturnStatus::LG_ERROR : ReturnStatus::INVALID_PARAMETER_PASSED);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderTesting, invalidGender_eventGenderPassed) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_set_string(general_values::EVENT_GENDER_VALUE);

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), use_admin_info ? ReturnStatus::LG_ERROR : ReturnStatus::INVALID_PARAMETER_PASSED);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderTesting, successful) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_set_string("gen_Z");

    {
        bsoncxx::oid staying_account_oid = insertRandomAccounts(1, 0);
        MatchingElement staying_matching_element;
        staying_matching_element.generateRandomValues();
        staying_matching_element.oid = staying_account_oid;
        staying_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{15L * 60L * 1000L}};

        UserAccountDoc staying_user_account(staying_account_oid);
        staying_user_account.genders_range.clear();
        staying_user_account.genders_range.emplace_back(request.set_string());
        staying_user_account.setIntoCollection();

        bsoncxx::oid removing_account_oid = insertRandomAccounts(1, 0);
        MatchingElement removed_matching_element;
        removed_matching_element.generateRandomValues();
        removed_matching_element.oid = removing_account_oid;
        removed_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{15L * 60L * 1000L}};

        UserAccountDoc removing_user_account(removing_account_oid);
        removing_user_account.genders_range.clear();
        removing_user_account.genders_range.emplace_back(request.set_string() + 'a');
        removing_user_account.setIntoCollection();

        user_account.other_users_matched_accounts_list.emplace_back(staying_matching_element);
        user_account.other_users_matched_accounts_list.emplace_back(removed_matching_element);
    }
    user_account.setIntoCollection();

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_GT(response.timestamp(), 0);

    user_account.gender = request.set_string();
    user_account.gender_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};
    user_account.last_time_displayed_info_updated = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};

    user_account.other_users_matched_accounts_list.pop_back();

    compareUserAccounts();

    user_account_statistics.genders.emplace_back(
            user_account.gender,
            user_account.gender_timestamp
    );

    compareUserStatisticsAccounts();
}