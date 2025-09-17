//
// Created by jeremiah on 3/18/23.
//

#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/collection.hpp>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "UserEventCommands.pb.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "user_event_commands.h"
#include "event_request_message_is_valid.h"
#include "generate_randoms.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class CancelEventTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    std::string chat_room_id;
    std::string chat_room_password;

    user_event_commands::CancelEventRequest request;
    user_event_commands::CancelEventResponse response;

    bsoncxx::oid event_oid;

    UserAccountDoc generated_event_account_doc;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void setupValidUserRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front()
        );

        request.set_event_oid(event_oid.to_string());
    }

    void setupValidAdminRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_event_oid(event_oid.to_string());
    }

    void runFunction(bool create_admin, AdminLevelEnum privilege_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        if(create_admin)
            createTempAdminAccount(privilege_level);

        cancelEvent(&request, &response);
    }

    void setupAdminGeneratedEvent() {
        createAndStoreEventAdminAccount();

        //Sleep so current_timestamp is after admin account creation.
        std::this_thread::sleep_for(std::chrono::milliseconds{1});

        const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

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

        event_oid = bsoncxx::oid{event_created_return_values.event_account_oid};
        generated_event_account_doc.getFromCollection(event_oid);
    }

    void setupUserGeneratedEvent(bool create_new_user) {

        bsoncxx::oid user_oid;
        if(create_new_user) {
            user_oid  = insertRandomAccounts(1, 0);
        } else {
            user_oid = user_account_oid;
        }
        UserAccountDoc user_doc(user_oid);

        //User must be subscribed to create events.
        user_doc.subscription_status = UserSubscriptionStatus::BASIC_SUBSCRIPTION;
        user_doc.setIntoCollection();

        const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

        const EventChatRoomAdminInfo chat_room_admin_info{
                UserAccountType::USER_ACCOUNT_TYPE,
                user_oid.to_string(),
                user_doc.logged_in_token,
                user_doc.installation_ids.front()
        };

        EventRequestMessage event_request = generateRandomEventRequestMessage(
                chat_room_admin_info,
                current_timestamp
        );

        EXPECT_FALSE(event_request.activity().time_frame_array().empty());
        if(!event_request.activity().time_frame_array().empty()
           && event_request.activity().time_frame_array(0).start_time_frame() < current_timestamp.count()) {
            const int diff = (int)event_request.activity().time_frame_array(0).stop_time_frame() - (int)event_request.activity().time_frame_array(0).start_time_frame();

            event_request.mutable_activity()->mutable_time_frame_array(0)->set_start_time_frame(current_timestamp.count() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());
            event_request.mutable_activity()->mutable_time_frame_array(0)->set_stop_time_frame(event_request.activity().time_frame_array(0).start_time_frame() + diff);
        }

        user_event_commands::AddUserEventRequest add_user_event_request;
        user_event_commands::AddUserEventResponse add_user_event_response;

        setupUserLoginInfo(
                add_user_event_request.mutable_login_info(),
                user_oid,
                user_doc.logged_in_token,
                user_doc.installation_ids.front()
        );

        add_user_event_request.mutable_event_request()->CopyFrom(event_request);

        addUserEvent(&add_user_event_request, &add_user_event_response);

        ASSERT_EQ(add_user_event_response.return_status(), ReturnStatus::SUCCESS);

        event_oid = bsoncxx::oid{add_user_event_response.event_oid()};
        generated_event_account_doc.getFromCollection(event_oid);

        if(!create_new_user) {
            user_account_doc.getFromCollection(user_account_oid);
        }
    }

    void compareAccounts() {
        UserAccountDoc extracted_event_account_doc(event_oid);
        EXPECT_EQ(extracted_event_account_doc, generated_event_account_doc);

        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(extracted_user_account_doc, user_account_doc);
    }

};

TEST_F(CancelEventTesting, invalidLoginInfo) {
    createTempAdminAccount(AdminLevelEnum::PRIMARY_DEVELOPER);

    setupAdminGeneratedEvent();
    setupValidUserRequest();

    checkLoginInfoAdminAndClient(
        TEMP_ADMIN_ACCOUNT_NAME,
        TEMP_ADMIN_ACCOUNT_PASSWORD,
        user_account_oid,
        user_account_doc.logged_in_token,
        user_account_doc.installation_ids.front(),
        [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

            request.mutable_login_info()->CopyFrom(login_info);

            response.Clear();

            runFunction(false);

            return response.return_status();
        }
    );

    compareAccounts();
}

TEST_F(CancelEventTesting, noAdminPriveledge) {
    setupAdminGeneratedEvent();

    setupValidAdminRequest();

    runFunction(true, AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_EQ(response.return_status(), ReturnStatus::LG_ERROR);
    EXPECT_FALSE(response.error_string().empty());

    compareAccounts();
}

TEST_F(CancelEventTesting, admin_invalidOidPassed) {
    setupAdminGeneratedEvent();

    setupValidAdminRequest();

    request.set_event_oid("abv");

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::LG_ERROR);
    EXPECT_FALSE(response.error_string().empty());

    compareAccounts();
}

TEST_F(CancelEventTesting, user_invalidOidPassed) {
    setupUserGeneratedEvent(false);

    setupValidUserRequest();

    request.set_event_oid("abv");

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_TRUE(response.error_string().empty());

    compareAccounts();
}

TEST_F(CancelEventTesting, admin_eventDoesNotExist) {
    setupValidAdminRequest();

    request.set_event_oid(bsoncxx::oid{}.to_string());

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::LG_ERROR);
    EXPECT_FALSE(response.error_string().empty());

    compareAccounts();
}

TEST_F(CancelEventTesting, user_eventDoesNotExist) {
    setupValidUserRequest();

    request.set_event_oid(bsoncxx::oid{}.to_string());

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_TRUE(response.error_string().empty());

    compareAccounts();
}

TEST_F(CancelEventTesting, userAttemptsToCancel_adminEvent) {
    setupAdminGeneratedEvent();

    setupValidUserRequest();

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_TRUE(response.error_string().empty());

    compareAccounts();
}

TEST_F(CancelEventTesting, userAttemptsToCancel_differentUserEvent) {
    setupUserGeneratedEvent(true);

    setupValidUserRequest();

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_TRUE(response.error_string().empty());

    compareAccounts();
}

TEST_F(CancelEventTesting, userCancelsUserEvent) {
    setupUserGeneratedEvent(false);

    setupValidUserRequest();

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_TRUE(response.error_string().empty());

    EXPECT_FALSE(user_account_doc.user_created_events.empty());
    if(!user_account_doc.user_created_events.empty()) {
        user_account_doc.user_created_events[0].event_state = LetsGoEventStatus::CANCELED;
    }

    generated_event_account_doc.matching_activated = false;
    generated_event_account_doc.event_expiration_time = bsoncxx::types::b_date{general_values::event_expiration_time_values::EVENT_CANCELED};
    for(auto& category : generated_event_account_doc.categories) {
        category.time_frames.clear();
    }

    compareAccounts();
}

TEST_F(CancelEventTesting, adminCancelsUserEvent) {
    setupUserGeneratedEvent(false);

    setupValidAdminRequest();

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_TRUE(response.error_string().empty());

    EXPECT_FALSE(user_account_doc.user_created_events.empty());
    if(!user_account_doc.user_created_events.empty()) {
        user_account_doc.user_created_events[0].event_state = LetsGoEventStatus::CANCELED;
    }

    generated_event_account_doc.matching_activated = false;
    generated_event_account_doc.event_expiration_time = bsoncxx::types::b_date{general_values::event_expiration_time_values::EVENT_CANCELED};
    for(auto& category : generated_event_account_doc.categories) {
        category.time_frames.clear();
    }

    compareAccounts();
}

TEST_F(CancelEventTesting, adminCancelsAdminEvent) {
    setupAdminGeneratedEvent();

    setupValidAdminRequest();

    runFunction(true);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_TRUE(response.error_string().empty());

    generated_event_account_doc.matching_activated = false;
    generated_event_account_doc.event_expiration_time = bsoncxx::types::b_date{general_values::event_expiration_time_values::EVENT_CANCELED};
    for(auto& category : generated_event_account_doc.categories) {
        category.time_frames.clear();
    }

    compareAccounts();
}
