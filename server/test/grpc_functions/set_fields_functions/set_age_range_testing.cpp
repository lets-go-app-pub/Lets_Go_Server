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

class SetAgeRangeTesting : public testing::Test {
protected:

    setfields::SetAgeRangeRequest request;
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

        request.set_min_age(user_account.age);
        request.set_max_age(user_account.age);
    }

    void setupValidUserRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        request.set_min_age(user_account.age);
        request.set_max_age(user_account.age);
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
        if(use_admin_info) {
            createTempAdminAccount(admin_level);
        }

        //guarantee different timestamp used
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
        setAgeRange(&request, &response);
    }

    template <bool setup_post_login_timestamp = false>
    void compareUserAccounts() {
        UserAccountDoc extracted_user_account(user_account_oid);

        if(setup_post_login_timestamp) {
            EXPECT_GT(extracted_user_account.post_login_info_timestamp, user_account.post_login_info_timestamp);
            user_account.post_login_info_timestamp = extracted_user_account.post_login_info_timestamp;
        }

        EXPECT_EQ(user_account, extracted_user_account);
    }

    void compareUserStatisticsAccounts() {
        UserAccountStatisticsDoc extracted_user_account_statistics(user_account_oid);
        EXPECT_EQ(user_account_statistics, extracted_user_account_statistics);
    }

    void checkSuccessfulResponse() {
        EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
        EXPECT_GT(response.timestamp(), 0);
    }

    void setupNewUserAge(const int new_user_age) {
        user_account.age = new_user_age;

        generateBirthYearForPassedAge(
                user_account.age,
                user_account.birth_year,
                user_account.birth_month,
                user_account.birth_day_of_month,
                user_account.birth_day_of_year
        );
        user_account.setIntoCollection();
    }

    bool randomizeClientOrAdmin() {
        const bool use_admin_info = rand() % 2;

        if(use_admin_info) { //use admin info
            setupValidAdminRequest();
        } else { //use client info
            setupValidUserRequest();
        }

        return use_admin_info;
    }

};

TEST_F(SetAgeRangeTesting, invalidLoginInfo) {
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

TEST_F(SetAgeRangeTesting, noAdminPriveledge) {
    setupValidAdminRequest();

    runFunction(true, AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_EQ(response.return_status(), ReturnStatus::LG_ERROR);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetAgeRangeTesting, invalidUserAccountOidPassedAsAdmin) {
    setupValidAdminRequest();

    request.mutable_login_info()->set_current_account_id("invalid_account_oid");

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_USER_OID);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetAgeRangeTesting, minAgeBelowLowestAllowed) {
    //Age should be setup first, some values below it can rely on user_account.age.
    const int new_user_age = 21;
    setupNewUserAge(new_user_age);

    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_min_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE - 1);
    request.set_max_age(user_account.age);

    runFunction(use_admin_info);

    checkSuccessfulResponse();

    user_account.age_range.min = 18;
    user_account.age_range.max = user_account.age;

    compareUserAccounts<true>();

    user_account_statistics.age_ranges.emplace_back(
            user_account.age_range.min,
            user_account.age_range.max,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}

TEST_F(SetAgeRangeTesting, minAgeBelowUserAgeRestriction) {
    //Age should be setup first, some values below it can rely on user_account.age.
    const int new_user_age = 21;
    setupNewUserAge(new_user_age);

    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_min_age(17);
    request.set_max_age(user_account.age);

    runFunction(use_admin_info);

    checkSuccessfulResponse();

    user_account.age_range.min = 18;
    user_account.age_range.max = user_account.age;

    compareUserAccounts<true>();

    user_account_statistics.age_ranges.emplace_back(
            user_account.age_range.min,
            user_account.age_range.max,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}

TEST_F(SetAgeRangeTesting, maxAgeAboveLowestAllowed) {
    //Age should be setup first, some values below it can rely on user_account.age.
    const int new_user_age = 21;
    setupNewUserAge(new_user_age);

    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_min_age(user_account.age);
    request.set_max_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1);

    runFunction(use_admin_info);

    checkSuccessfulResponse();

    user_account.age_range.min = user_account.age;
    user_account.age_range.max = server_parameter_restrictions::HIGHEST_ALLOWED_AGE;

    compareUserAccounts<true>();

    user_account_statistics.age_ranges.emplace_back(
            user_account.age_range.min,
            user_account.age_range.max,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}

TEST_F(SetAgeRangeTesting, maxAgeAboveUserAgeRestriction) {
    //Age should be setup first, some values below it can rely on user_account.age.
    const int new_user_age = 15;
    setupNewUserAge(new_user_age);

    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_min_age(user_account.age);
    request.set_max_age(18);

    runFunction(use_admin_info);

    checkSuccessfulResponse();

    user_account.age_range.min = user_account.age;
    user_account.age_range.max = 17;

    compareUserAccounts<true>();

    user_account_statistics.age_ranges.emplace_back(
            user_account.age_range.min,
            user_account.age_range.max,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}

TEST_F(SetAgeRangeTesting, allParametersValid_sentByUser) {
    //Age should be setup first, some values below it can rely on user_account.age.
    const int new_user_age = 55;
    setupNewUserAge(new_user_age);

    setupValidUserRequest();

    request.set_min_age(new_user_age - 10);
    request.set_max_age(new_user_age + 10);

    runFunction(false);

    checkSuccessfulResponse();

    user_account.age_range.min = request.min_age();
    user_account.age_range.max = request.max_age();

    compareUserAccounts<true>();

    user_account_statistics.age_ranges.emplace_back(
            user_account.age_range.min,
            user_account.age_range.max,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}

TEST_F(SetAgeRangeTesting, allParametersValid_sentByAdmin) {
    //Age should be setup first, some values below it can rely on user_account.age.
    const int new_user_age = 14;
    setupNewUserAge(new_user_age);

    setupValidAdminRequest();

    request.set_min_age(new_user_age - 1);
    request.set_max_age(new_user_age + 2);

    runFunction(true);

    checkSuccessfulResponse();

    user_account.age_range.min = request.min_age();
    user_account.age_range.max = request.max_age();

    compareUserAccounts<true>();

    user_account_statistics.age_ranges.emplace_back(
            user_account.age_range.min,
            user_account.age_range.max,
            user_account.post_login_info_timestamp
    );

    compareUserStatisticsAccounts();
}
