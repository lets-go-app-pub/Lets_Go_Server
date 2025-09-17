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

class SetGenderRangeTesting : public testing::Test {
protected:

    setfields::SetGenderRangeRequest request;
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

        request.add_gender_range(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
    }

    void setupValidUserRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        request.add_gender_range(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
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
        setGenderRange(&request, &response);
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

TEST_F(SetGenderRangeTesting, invalidLoginInfo) {
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

TEST_F(SetGenderRangeTesting, noAdminPriveledge) {
    setupValidAdminRequest();

    runFunction(true, AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_EQ(response.return_status(), ReturnStatus::LG_ERROR);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderRangeTesting, invalidUserAccountOidPassedAsAdmin) {
    setupValidAdminRequest();

    request.mutable_login_info()->set_current_account_id("invalid_account_oid");

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_USER_OID);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderRangeTesting, genderRange_empty) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.clear_gender_range();

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), use_admin_info ? ReturnStatus::LG_ERROR : ReturnStatus::INVALID_PARAMETER_PASSED);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderRangeTesting, genderRange_allGendersInvalid) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.clear_gender_range();
    request.add_gender_range("");
    request.add_gender_range(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1));

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), use_admin_info ? ReturnStatus::LG_ERROR : ReturnStatus::INVALID_PARAMETER_PASSED);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetGenderRangeTesting, genderRange_tooLarge) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.clear_gender_range();
    for(int i = 0; i < server_parameter_restrictions::NUMBER_GENDER_USER_CAN_MATCH_WITH + 1; ++i) {
        request.add_gender_range(std::string((char)i + 'a', 1));
    }

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_GT(response.timestamp(), 0);

    user_account.genders_range.clear();
    for(int i = 0; i < server_parameter_restrictions::NUMBER_GENDER_USER_CAN_MATCH_WITH; ++i) {
        user_account.genders_range.emplace_back(std::string((char)i + 'a', 1));
    }

    user_account.post_login_info_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};

    compareUserAccounts();

    user_account_statistics.gender_ranges.emplace_back(
            user_account.genders_range,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}

TEST_F(SetGenderRangeTesting, genderRange_duplicateGenders) {
    const bool use_admin_info = randomizeClientOrAdmin();

    //NOTE: The $ is to make sure it does not interfere with the mongodb pipeline.
    const std::string duplicate_gender_name = "$duplicate";

    request.clear_gender_range();
    request.add_gender_range(duplicate_gender_name);
    request.add_gender_range(duplicate_gender_name);

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_GT(response.timestamp(), 0);

    user_account.genders_range.clear();
    user_account.genders_range.emplace_back(duplicate_gender_name);

    user_account.post_login_info_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};

    compareUserAccounts();

    user_account_statistics.gender_ranges.emplace_back(
            user_account.genders_range,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}

TEST_F(SetGenderRangeTesting, genderRange_everyonePassedWithAnotherGender) {
    const bool use_admin_info = randomizeClientOrAdmin();

    const std::string removed_gender_name = "this_should_be_removed";

    request.clear_gender_range();
    request.add_gender_range(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
    request.add_gender_range(removed_gender_name);

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_GT(response.timestamp(), 0);

    user_account.genders_range.clear();
    user_account.genders_range.emplace_back(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);

    user_account.post_login_info_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};

    compareUserAccounts();

    user_account_statistics.gender_ranges.emplace_back(
            user_account.genders_range,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}

TEST_F(SetGenderRangeTesting, successful) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.clear_gender_range();
    request.add_gender_range(general_values::FEMALE_GENDER_VALUE);

    {
        bsoncxx::oid staying_account_oid = insertRandomAccounts(1, 0);
        MatchingElement staying_matching_element;
        staying_matching_element.generateRandomValues();
        staying_matching_element.oid = staying_account_oid;
        staying_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{15L * 60L * 1000L}};

        UserAccountDoc staying_user_account(staying_account_oid);
        staying_user_account.gender = general_values::FEMALE_GENDER_VALUE;
        staying_user_account.setIntoCollection();

        bsoncxx::oid removing_account_oid = insertRandomAccounts(1, 0);
        MatchingElement removed_matching_element;
        removed_matching_element.generateRandomValues();
        removed_matching_element.oid = removing_account_oid;
        removed_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{15L * 60L * 1000L}};

        UserAccountDoc removing_user_account(removing_account_oid);
        removing_user_account.gender = general_values::MALE_GENDER_VALUE;
        removing_user_account.setIntoCollection();

        user_account.other_users_matched_accounts_list.emplace_back(staying_matching_element);
        user_account.other_users_matched_accounts_list.emplace_back(removed_matching_element);
    }
    user_account.setIntoCollection();

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_GT(response.timestamp(), 0);

    user_account.genders_range.clear();
    user_account.genders_range.emplace_back(general_values::FEMALE_GENDER_VALUE);

    user_account.post_login_info_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};

    user_account.other_users_matched_accounts_list.pop_back();

    compareUserAccounts();

    user_account_statistics.gender_ranges.emplace_back(
            user_account.genders_range,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}
