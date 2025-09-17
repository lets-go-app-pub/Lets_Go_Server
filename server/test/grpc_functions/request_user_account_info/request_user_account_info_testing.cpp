//
// Created by jeremiah on 10/6/22.
//

#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "setup_login_info.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "RequestStatistics.pb.h"
#include "compare_equivalent_messages.h"
#include "request_user_account_info.h"
#include "generate_multiple_random_accounts.h"
#include "event_request_message_is_valid.h"
#include "generate_randoms.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RequestUserAccountInfoTesting : public ::testing::Test {
protected:

    RequestUserAccountInfoRequest request;
    RequestUserAccountInfoResponse response;

    bsoncxx::oid requested_account_oid;
    UserAccountDoc requested_user_account;

    std::chrono::milliseconds current_timestamp;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_user_account_oid(requested_account_oid.to_string());
        request.set_request_event(false);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        requested_account_oid = insertRandomAccounts(1, 0);
        requested_user_account.getFromCollection(requested_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        runRequestUserAccountInfo(&request, &response);
    }

    void checkFunctionFailed() {
        EXPECT_FALSE(response.error_msg().empty());
        EXPECT_FALSE(response.success());
    }

    void checkFunctionSuccess() {
        CompleteUserAccountInfo generated_user_account_info = requested_user_account.convertToCompleteUserAccountInfo(
                std::chrono::milliseconds{response.user_account_info().timestamp_user_returned()}
        );

        EXPECT_TRUE(response.success());

        compareEquivalentMessages<CompleteUserAccountInfo>(
                generated_user_account_info,
                response.user_account_info()
        );
    }

    void generateEvent() {
        createAndStoreEventAdminAccount();

        current_timestamp = getCurrentTimestamp();

        const EventChatRoomAdminInfo chat_room_admin_info(
                UserAccountType::ADMIN_GENERATED_EVENT_TYPE
        );

        auto event_info = generateRandomEventRequestMessage(
                chat_room_admin_info,
                current_timestamp
        );

        //Add an event chat room
        auto event_created_return_values = generateRandomEvent(
                chat_room_admin_info,
                TEMP_ADMIN_ACCOUNT_NAME,
                current_timestamp,
                event_info
        );

        ASSERT_TRUE(event_created_return_values.return_status == ReturnStatus::SUCCESS);

        requested_account_oid = bsoncxx::oid{event_created_return_values.event_account_oid};
        requested_user_account.getFromCollection(requested_account_oid);
    };
};

TEST_F(RequestUserAccountInfoTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_msg().empty());
                return response.success();
            }
    );
}

TEST_F(RequestUserAccountInfoTesting, noAdminPriveledge_userRequested) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    checkFunctionFailed();
}

TEST_F(RequestUserAccountInfoTesting, noAdminPriveledge_eventRequested) {
    request.set_request_event(true);

    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    checkFunctionFailed();
}

TEST_F(RequestUserAccountInfoTesting, noPhoneNumberOrAccountOidPassed) {
    request.clear_user_account_oid();
    request.clear_user_phone_number();

    runFunction();

    checkFunctionFailed();
}

TEST_F(RequestUserAccountInfoTesting, requestedAccountDoesNotExist) {
    request.clear_user_account_oid();
    request.clear_user_phone_number();

    runFunction();

    checkFunctionFailed();
}

TEST_F(RequestUserAccountInfoTesting, requestValidUserAccount_requestEventSet) {
    request.set_user_account_oid(requested_account_oid.to_string());
    request.set_request_event(true);

    runFunction();

    checkFunctionFailed();
}

TEST_F(RequestUserAccountInfoTesting, requestValidEventAccount_requestUserSet) {
    generateEvent();
    request.set_user_account_oid(requested_account_oid.to_string());
    request.set_request_event(false);

    runFunction();

    checkFunctionFailed();
}

TEST_F(RequestUserAccountInfoTesting, requestValidUserAccount_byAccountOid) {
    request.set_user_account_oid(requested_account_oid.to_string());

    runFunction();

    checkFunctionSuccess();
}

TEST_F(RequestUserAccountInfoTesting, requestValidEvent) {
    generateEvent();
    request.set_user_account_oid(requested_account_oid.to_string());
    request.set_request_event(true);

    runFunction();

    checkFunctionSuccess();
}

TEST_F(RequestUserAccountInfoTesting, requestValidUserAccount_byPhoneNumber) {
    request.set_user_phone_number(requested_user_account.phone_number);

    runFunction();

    checkFunctionSuccess();
}
