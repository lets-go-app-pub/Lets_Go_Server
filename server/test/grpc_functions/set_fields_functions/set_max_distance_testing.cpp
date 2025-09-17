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

class SetMaxDistanceTesting : public testing::Test {
protected:

    setfields::SetMaxDistanceRequest request;
    setfields::SetFieldResponse response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    UserAccountStatisticsDoc user_account_statistics;

    void setupRandomMaxDistance() {
        using namespace server_parameter_restrictions;

        request.set_max_distance(
                (rand() % (MAXIMUM_ALLOWED_DISTANCE - MINIMUM_ALLOWED_DISTANCE)) + MINIMUM_ALLOWED_DISTANCE
        );
    }

    void setupValidAdminRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.mutable_login_info()->set_current_account_id(user_account_oid.to_string());

        setupRandomMaxDistance();
    }

    void setupValidUserRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        setupRandomMaxDistance();
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
        setMaxDistance(&request, &response);
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

    void compareSuccessfulFunction(int expected_max_distance) {
        EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
        EXPECT_GT(response.timestamp(), 0);

        user_account.max_distance = expected_max_distance;
        user_account.post_login_info_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};

        compareUserAccounts();

        user_account_statistics.max_distances.emplace_back(
                user_account.max_distance,
                user_account.post_login_info_timestamp
        );

        compareUserStatisticsAccounts();
    }

};

TEST_F(SetMaxDistanceTesting, invalidLoginInfo) {
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

TEST_F(SetMaxDistanceTesting, noAdminPriveledge) {
    setupValidAdminRequest();

    runFunction(true, AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_EQ(response.return_status(), ReturnStatus::LG_ERROR);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetMaxDistanceTesting, invalidUserAccountOidPassedAsAdmin) {
    setupValidAdminRequest();

    request.mutable_login_info()->set_current_account_id("invalid_account_oid");

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_USER_OID);

    //should not have changed
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetMaxDistanceTesting, invalidMaxDistance_tooSmall) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_max_distance(server_parameter_restrictions::MINIMUM_ALLOWED_DISTANCE - 1);

    runFunction(use_admin_info);

    compareSuccessfulFunction(server_parameter_restrictions::MINIMUM_ALLOWED_DISTANCE);
}

TEST_F(SetMaxDistanceTesting, invalidMaxDistance_tooLarge) {
    const bool use_admin_info = randomizeClientOrAdmin();

    request.set_max_distance(server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE + 1);

    runFunction(use_admin_info);

    compareSuccessfulFunction(server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE);
}

TEST_F(SetMaxDistanceTesting, successful) {
    const bool use_admin_info = randomizeClientOrAdmin();

    {
        MatchingElement staying_matching_element;
        staying_matching_element.generateRandomValues();
        staying_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{15L * 60L * 1000L}};
        staying_matching_element.distance = request.max_distance() + .01;

        MatchingElement removed_matching_element;
        removed_matching_element.generateRandomValues();
        removed_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{15L * 60L * 1000L}};
        removed_matching_element.distance = request.max_distance() - .01;

        user_account.other_users_matched_accounts_list.emplace_back(staying_matching_element);
        user_account.other_users_matched_accounts_list.emplace_back(removed_matching_element);
    }
    user_account.setIntoCollection();

    runFunction(use_admin_info);

    user_account.other_users_matched_accounts_list.pop_back();

    compareSuccessfulFunction(request.max_distance());
}