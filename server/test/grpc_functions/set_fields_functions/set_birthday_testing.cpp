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
#include "android_specific_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetBirthdayTesting : public testing::Test {
protected:

    setfields::SetBirthdayRequest request;
    setfields::SetBirthdayResponse response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    UserAccountStatisticsDoc user_account_statistics;

    void setupRandomValidRequest() {
        request.set_birth_year(rand() % 55 + 1950);
        request.set_birth_month(rand() % 12 + 1);
        request.set_birth_day_of_month(rand() % 28 + 1);
    }

    void setupValidAdminRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.mutable_login_info()->set_current_account_id(user_account_oid.to_string());

        setupRandomValidRequest();
    }

    void setupValidUserRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        setupRandomValidRequest();
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
        setBirthday(&request, &response);
    }

    template<bool setup_post_login_timestamp = false>
    void compareUserAccounts() {
        UserAccountDoc extracted_user_account(user_account_oid);

        if (setup_post_login_timestamp) {
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
        EXPECT_EQ(
                response.age(),
                calculateAge(
                        getCurrentTimestamp(),
                        request.birth_year(),
                        request.birth_month(),
                        request.birth_day_of_month()
                )
        );
    }

    template <bool handle_age_ranges>
    void compareSuccessfullyUpdated() {
        checkSuccessfulResponse();

        const int birth_day_of_year = initializeTmByDate(
                request.birth_year(),
                request.birth_month(),
                request.birth_day_of_month()
        ).tm_yday;

        const int generated_age = calculateAge(
                getCurrentTimestamp(),
                request.birth_year(),
                request.birth_month(),
                request.birth_day_of_month()
        );

        if(handle_age_ranges) {
            int min_age_range = user_account.age_range.min;
            int max_age_range = user_account.age_range.max;

            if(user_account.age != generated_age
                && (generated_age < 20
                    || user_account.age < 20)
            ) {
                AgeRangeDataObject default_age_range = calculateAgeRangeFromUserAge(generated_age);

                EXPECT_NE(default_age_range.min_age, -1);
                EXPECT_NE(default_age_range.max_age, -1);

                min_age_range = default_age_range.min_age;
                max_age_range = default_age_range.max_age;

                user_account.age_range.min = default_age_range.min_age;
                user_account.age_range.max = default_age_range.max_age;
            }

            EXPECT_EQ(response.min_age_range(), min_age_range);
            EXPECT_EQ(response.max_age_range(), max_age_range);
        }

        user_account.age = generated_age;
        user_account.birth_year = request.birth_year();
        user_account.birth_month = request.birth_month();
        user_account.birth_day_of_month = request.birth_day_of_month();
        user_account.birth_day_of_year = birth_day_of_year;
        user_account.birthday_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};
        user_account.last_time_displayed_info_updated = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};

        //should not have changed
        compareUserAccounts();

        user_account_statistics.birth_info.emplace_back(
                user_account.birth_year,
                user_account.birth_month,
                user_account.birth_day_of_month,
                user_account.birthday_timestamp
        );

        compareUserStatisticsAccounts();
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

    void setupNewUserAgeAndAgeRange(
            const int new_user_age,
            const int min_age_range,
            const int max_age_range
            ) {
        user_account.age = new_user_age;

        generateBirthYearForPassedAge(
                user_account.age,
                user_account.birth_year,
                user_account.birth_month,
                user_account.birth_day_of_month,
                user_account.birth_day_of_year
        );

        user_account.age_range.min = min_age_range;
        user_account.age_range.max = max_age_range;
        user_account.setIntoCollection();
    }

    void saveBirthDayInfoForAgeToResponse(int expected_age) {
        int birth_year;
        int birth_day_of_year;

        generateBirthYearForPassedAge(
                expected_age,
                birth_year,
                request.birth_month(),
                request.birth_day_of_month(),
                birth_day_of_year
        );

        request.set_birth_year(birth_year);
    }
};

TEST_F(SetBirthdayTesting, invalidLoginInfo) {
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

TEST_F(SetBirthdayTesting, noAdminPriveledge) {
    setupValidAdminRequest();

    runFunction(true, AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_EQ(response.return_status(), ReturnStatus::LG_ERROR);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetBirthdayTesting, invalidUserAccountOidPassedAsAdmin) {
    setupValidAdminRequest();

    request.mutable_login_info()->set_current_account_id("invalid_account_oid");

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_USER_OID);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetBirthdayTesting, invalidBirthdayPassed) {
    const bool use_admin_info = randomizeClientOrAdmin();

    const int num = rand() % 3;
    switch (num) {
        case 0:
            request.set_birth_year(request.birth_year() + rand() % 10 + 120);
            break;
        case 1:
            request.set_birth_month(request.birth_month() + rand() % 10 + 12);
            break;
        default:
            request.set_birth_day_of_month(request.birth_month() + rand() % 10 + 30);
            break;
    }

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(),
              use_admin_info ? ReturnStatus::LG_ERROR : ReturnStatus::INVALID_PARAMETER_PASSED);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetBirthdayTesting, invalidAgeFromBirthday_tooYoung) {
    const bool use_admin_info = randomizeClientOrAdmin();

    saveBirthDayInfoForAgeToResponse(server_parameter_restrictions::LOWEST_ALLOWED_AGE - 1);

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetBirthdayTesting, invalidAgeFromBirthday_tooOld) {
    const bool use_admin_info = randomizeClientOrAdmin();

    saveBirthDayInfoForAgeToResponse(server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1);

    runFunction(use_admin_info);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetBirthdayTesting, accountStatusActive_sentByUser) {
    setupValidUserRequest();

    runFunction(false);

    checkSuccessfulResponse();

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetBirthdayTesting, ageRangeRequiresUpdate_sentByAdmin) {
    //Age should be setup first, some values below it can rely on user_account.age.
    //Set user age to minimum age possible. And age range to match it.
    const int new_user_age = server_parameter_restrictions::LOWEST_ALLOWED_AGE;
    setupNewUserAgeAndAgeRange(
            new_user_age,
            server_parameter_restrictions::LOWEST_ALLOWED_AGE,
            server_parameter_restrictions::LOWEST_ALLOWED_AGE + general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_CHILDREN
    );

    //sets up request, should be done second
    setupValidAdminRequest();

    //Set age of the request to the highest possible age.
    saveBirthDayInfoForAgeToResponse(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);

    runFunction(true);

    compareSuccessfullyUpdated<true>();
}

TEST_F(SetBirthdayTesting, allParametersValid_sentByUser) {
    //Setup account in which account requires more info and birthday has not yet been set.
    user_account.status = UserAccountStatus::STATUS_REQUIRES_MORE_INFO;
    user_account.birth_year = -1;
    user_account.birth_month = -1;
    user_account.birth_day_of_month = -1;
    user_account.birth_day_of_year = -1;
    user_account.birthday_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    user_account.age = -1;
    user_account.age_range.min = -1;
    user_account.age_range.max = -1;
    user_account.setIntoCollection();

    setupValidUserRequest();

    runFunction(true);

    compareSuccessfullyUpdated<false>();
}

TEST_F(SetBirthdayTesting, allParametersValid_sentByAdmin) {
    setupValidAdminRequest();

    runFunction(true);

    compareSuccessfullyUpdated<true>();
}