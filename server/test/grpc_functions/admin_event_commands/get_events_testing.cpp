//
// Created by jeremiah on 3/26/23.
//

#include <fstream>
#include <mongocxx/pool.hpp>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "chat_rooms_objects.h"

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "UserEventCommands.pb.h"
#include "event_request_message_is_valid.h"
#include "generate_randoms.h"
#include "connection_pool_global_variable.h"
#include "check_for_valid_event_request_message.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "admin_event_commands.h"
#include "generate_multiple_random_accounts.h"
#include "user_event_commands.h"
#include "compare_equivalent_messages.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GetEventsTesting : public ::testing::Test {
protected:

    admin_event_commands::GetEventsRequest request;
    admin_event_commands::GetEventsResponse response;

    std::chrono::milliseconds current_timestamp{-1};

    EventRequestMessage admin_event_request;
    bsoncxx::oid admin_event_account_oid;

    bsoncxx::oid user_account_oid;
    UserAccountDoc user_account_doc;

    EventRequestMessage user_event_request;
    bsoncxx::oid user_event_account_oid;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        createTempAdminAccount(AdminLevelEnum::PRIMARY_DEVELOPER);

        createAdminGeneratedEvent();

        createUserGeneratedEvent();

        setupRequest();
    }

    void createAdminGeneratedEvent() {
        createAndStoreEventAdminAccount();

        const EventChatRoomAdminInfo chat_room_admin_info(
                UserAccountType::ADMIN_GENERATED_EVENT_TYPE
        );

        admin_event_request = generateRandomEventRequestMessage(
                chat_room_admin_info,
                current_timestamp
        );

        current_timestamp = getCurrentTimestamp();

        EXPECT_FALSE(admin_event_request.activity().time_frame_array().empty());
        if (!admin_event_request.activity().time_frame_array().empty()
            && admin_event_request.activity().time_frame_array(0).start_time_frame() < current_timestamp.count()) {
            const int diff = (int) admin_event_request.activity().time_frame_array(0).stop_time_frame() -
                             (int) admin_event_request.activity().time_frame_array(0).start_time_frame();

            admin_event_request.mutable_activity()->mutable_time_frame_array(0)->set_start_time_frame(
                    current_timestamp.count() +
                    2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());
            admin_event_request.mutable_activity()->mutable_time_frame_array(0)->set_stop_time_frame(
                    admin_event_request.activity().time_frame_array(0).start_time_frame() + diff);
        }

        admin_event_account_oid =
                bsoncxx::oid{
                        generateRandomEvent(
                                chat_room_admin_info,
                                TEMP_ADMIN_ACCOUNT_NAME,
                                current_timestamp,
                                admin_event_request
                        ).event_account_oid
                };
    }

    void createUserGeneratedEvent() {
        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        //User must be subscribed to create events.
        user_account_doc.subscription_status = UserSubscriptionStatus::BASIC_SUBSCRIPTION;
        user_account_doc.setIntoCollection();

        const EventChatRoomAdminInfo chat_room_user_info{
                UserAccountType::USER_ACCOUNT_TYPE,
                user_account_oid.to_string(),
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front()
        };

        user_event_request = generateRandomEventRequestMessage(
                chat_room_user_info,
                current_timestamp
        );

        EXPECT_FALSE(user_event_request.activity().time_frame_array().empty());
        if (!user_event_request.activity().time_frame_array().empty()
            && user_event_request.activity().time_frame_array(0).start_time_frame() < current_timestamp.count()) {
            const int diff = (int) user_event_request.activity().time_frame_array(0).stop_time_frame() -
                             (int) user_event_request.activity().time_frame_array(0).start_time_frame();

            user_event_request.mutable_activity()->mutable_time_frame_array(0)->set_start_time_frame(
                    current_timestamp.count() +
                    2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());
            user_event_request.mutable_activity()->mutable_time_frame_array(0)->set_stop_time_frame(
                    user_event_request.activity().time_frame_array(0).start_time_frame() + diff);
        }

        user_event_commands::AddUserEventRequest add_user_event_request;
        user_event_commands::AddUserEventResponse add_user_event_response;

        setupUserLoginInfo(
                add_user_event_request.mutable_login_info(),
                user_account_oid,
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front()
        );

        add_user_event_request.mutable_event_request()->CopyFrom(user_event_request);

        addUserEvent(&add_user_event_request, &add_user_event_response);

        ASSERT_EQ(add_user_event_response.return_status(), ReturnStatus::SUCCESS);

        user_event_account_oid = bsoncxx::oid{add_user_event_response.event_oid()};
    }

    void setupRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.mutable_account_types_to_search()->add_events_to_search_by(UserAccountType::ADMIN_GENERATED_EVENT_TYPE);
        request.mutable_account_types_to_search()->add_events_to_search_by(UserAccountType::USER_GENERATED_EVENT_TYPE);

        request.set_start_time_search(admin_event_commands::GetEventsRequest_TimeSearch_NO_SEARCH);

        request.set_expiration_time_search(admin_event_commands::GetEventsRequest_TimeSearch_NO_SEARCH);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        if (AdminLevelEnum::PRIMARY_DEVELOPER != admin_level) {
            createTempAdminAccount(admin_level);
        }

        getEvents(&request, &response);
    }

    admin_event_commands::SingleEvent convertEventAccountToSingleEventMessage(
            const bsoncxx::oid& oid,
            const std::chrono::milliseconds& returned_timestamp
    ) {
        admin_event_commands::SingleEvent single_event;

        UserAccountDoc account_doc(oid);
        single_event.set_event_oid(oid.to_string());
        single_event.set_time_created_ms(account_doc.time_created.to_int64());
        single_event.set_expiration_time_ms(account_doc.event_expiration_time.to_int64());
        single_event.set_created_by(account_doc.event_values->created_by);
        single_event.set_chat_room_id(account_doc.event_values->chat_room_id);
        single_event.set_event_title(account_doc.event_values->event_title);
        single_event.set_account_type(account_doc.account_type);
        single_event.set_event_status(
                convertExpirationTimeToEventStatus(
                        account_doc.event_expiration_time.value,
                        returned_timestamp
                )
        );
        single_event.set_number_swipes_yes(account_doc.number_times_others_swiped_yes);
        single_event.set_number_swipes_no(account_doc.number_times_others_swiped_no);
        single_event.set_number_swipes_block_and_report(
                account_doc.number_times_others_swiped_block +
                account_doc.number_times_others_swiped_report
        );

        return single_event;
    }

    void successfullyReturnedEvent(
            const admin_event_commands::SingleEvent& passed_single_event,
            const std::chrono::milliseconds& returned_timestamp,
            const bsoncxx::oid& oid
    ) {
        const admin_event_commands::SingleEvent generated_single_event = convertEventAccountToSingleEventMessage(
                oid,
                returned_timestamp
        );

        compareEquivalentMessages(
                passed_single_event,
                generated_single_event
        );
    }

    void successfullyReturnedUserEvent(
            const admin_event_commands::SingleEvent& passed_single_event,
            const std::chrono::milliseconds& returned_timestamp
    ) {
        successfullyReturnedEvent(
                passed_single_event,
                returned_timestamp,
                user_event_account_oid
        );
    }

    void successfullyReturnedAdminEvent(
            const admin_event_commands::SingleEvent& passed_single_event,
            const std::chrono::milliseconds& returned_timestamp
    ) {
        successfullyReturnedEvent(
                passed_single_event,
                returned_timestamp,
                admin_event_account_oid
        );
    }

    void forceAdminEventToBeFirstInArray() {
        std::sort(response.mutable_events()->begin(), response.mutable_events()->end(), [](
                const admin_event_commands::SingleEvent& lhs,
                const admin_event_commands::SingleEvent& rhs
        ){
            return lhs.account_type() < rhs.account_type();
        });
    }
};

TEST_F(GetEventsTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_message().empty());
                return response.successful();
            }
    );
}

TEST_F(GetEventsTesting, invalidAdminPrivilegeLevel) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());
}

TEST_F(GetEventsTesting, invalidEventsCreatedByCase) {
    request.clear_events_created_by();
    runFunction();

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());
}

TEST_F(GetEventsTesting, accountTypesToSearchUsed_emptyAccountTypes) {
    request.mutable_account_types_to_search()->Clear();
    runFunction();

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());
}

TEST_F(GetEventsTesting, accountTypesToSearchUsed_userAccountType) {
    request.mutable_account_types_to_search()->Clear();
    request.mutable_account_types_to_search()->add_events_to_search_by(UserAccountType::USER_ACCOUNT_TYPE);
    runFunction();

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());
}

TEST_F(GetEventsTesting, accountTypesToSearchUsed_invalidAccountType) {
    request.mutable_account_types_to_search()->Clear();
    request.mutable_account_types_to_search()->add_events_to_search_by(UserAccountType(-1));
    runFunction();

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());
}

TEST_F(GetEventsTesting, singleUserCreatedByUsed_emptySingleUserString) {
    request.set_single_user_created_by("");
    runFunction();

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());
}

TEST_F(GetEventsTesting, invalidStartTimeSearch) {
    request.set_start_time_search(
            admin_event_commands::GetEventsRequest_TimeSearch_GetEventsRequest_TimeSearch_INT_MAX_SENTINEL_DO_NOT_USE_);
    runFunction();

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());
}

TEST_F(GetEventsTesting, invalidExpirationTimeSearch) {
    request.set_expiration_time_search(
            admin_event_commands::GetEventsRequest_TimeSearch_GetEventsRequest_TimeSearch_INT_MAX_SENTINEL_DO_NOT_USE_);
    runFunction();

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());
}

TEST_F(GetEventsTesting, accountTypesToSearchUsed_adminGeneratedEvent) {
    request.mutable_account_types_to_search()->Clear();
    request.mutable_account_types_to_search()->add_events_to_search_by(UserAccountType::ADMIN_GENERATED_EVENT_TYPE);
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 1);

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, accountTypesToSearchUsed_userGeneratedEvent) {
    request.mutable_account_types_to_search()->Clear();
    request.mutable_account_types_to_search()->add_events_to_search_by(UserAccountType::USER_GENERATED_EVENT_TYPE);
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 1);

    successfullyReturnedUserEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, accountTypesToSearchUsed_allEvents) {
    request.mutable_account_types_to_search()->Clear();
    request.mutable_account_types_to_search()->add_events_to_search_by(UserAccountType::USER_GENERATED_EVENT_TYPE);
    request.mutable_account_types_to_search()->add_events_to_search_by(UserAccountType::ADMIN_GENERATED_EVENT_TYPE);
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 2);

    forceAdminEventToBeFirstInArray();

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );

    successfullyReturnedUserEvent(
            response.events(1),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, singledUserCreated_adminName_successfullyFound) {
    request.set_single_user_created_by(TEMP_ADMIN_ACCOUNT_NAME);
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 1);

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, singledUserCreated_userOid_successfullyFound) {
    request.set_single_user_created_by(user_account_oid.to_string());
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 1);

    successfullyReturnedUserEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, startTimeSearch_GreaterThan) {

    UserAccountDoc admin_event_doc(admin_event_account_oid);
    UserAccountDoc user_event_doc(user_event_account_oid);

    const std::chrono::milliseconds random_time{rand() % 1000000};

    admin_event_doc.time_created = bsoncxx::types::b_date{random_time + std::chrono::milliseconds{1}};
    user_event_doc.time_created = bsoncxx::types::b_date{random_time};

    admin_event_doc.setIntoCollection();
    user_event_doc.setIntoCollection();

    request.set_start_time_search(admin_event_commands::GetEventsRequest_TimeSearch_GREATER_THAN);
    request.set_events_start_time_ms(random_time.count());
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 1);

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, startTimeSearch_LessThan) {

    UserAccountDoc admin_event_doc(admin_event_account_oid);
    UserAccountDoc user_event_doc(user_event_account_oid);

    const std::chrono::milliseconds random_time{rand() % 1000000};

    admin_event_doc.time_created = bsoncxx::types::b_date{random_time - std::chrono::milliseconds{1}};
    user_event_doc.time_created = bsoncxx::types::b_date{random_time};

    admin_event_doc.setIntoCollection();
    user_event_doc.setIntoCollection();

    request.set_start_time_search(admin_event_commands::GetEventsRequest_TimeSearch_LESS_THAN);
    request.set_events_start_time_ms(random_time.count());
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 1);

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, startTimeSearch_NoSearch) {
    request.set_start_time_search(admin_event_commands::GetEventsRequest_TimeSearch_NO_SEARCH);
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 2);

    forceAdminEventToBeFirstInArray();

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );

    successfullyReturnedUserEvent(
            response.events(1),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, expirationTimeSearch_GreaterThan) {

    UserAccountDoc admin_event_doc(admin_event_account_oid);
    UserAccountDoc user_event_doc(user_event_account_oid);

    const std::chrono::milliseconds random_time{rand() % 1000000};

    admin_event_doc.event_expiration_time = bsoncxx::types::b_date{random_time + std::chrono::milliseconds{1}};
    user_event_doc.event_expiration_time = bsoncxx::types::b_date{random_time};

    admin_event_doc.setIntoCollection();
    user_event_doc.setIntoCollection();

    request.set_expiration_time_search(admin_event_commands::GetEventsRequest_TimeSearch_GREATER_THAN);
    request.set_events_expiration_time_ms(random_time.count());
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 1);

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, expirationTimeSearch_LessThan) {
    UserAccountDoc admin_event_doc(admin_event_account_oid);
    UserAccountDoc user_event_doc(user_event_account_oid);

    const std::chrono::milliseconds random_time{rand() % 1000000};

    admin_event_doc.event_expiration_time = bsoncxx::types::b_date{random_time - std::chrono::milliseconds{1}};
    user_event_doc.event_expiration_time = bsoncxx::types::b_date{random_time};

    admin_event_doc.setIntoCollection();
    user_event_doc.setIntoCollection();

    request.set_expiration_time_search(admin_event_commands::GetEventsRequest_TimeSearch_LESS_THAN);
    request.set_events_expiration_time_ms(random_time.count());
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 1);

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, expirationTimeSearch_NoSearch) {
    request.set_expiration_time_search(admin_event_commands::GetEventsRequest_TimeSearch_NO_SEARCH);
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 2);

    forceAdminEventToBeFirstInArray();

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );

    successfullyReturnedUserEvent(
            response.events(1),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, expirationTimeSearch_LessThan_skipsCanceledMessages) {
    UserAccountDoc admin_event_doc(admin_event_account_oid);
    UserAccountDoc user_account_doc(user_account_oid);

    const std::chrono::milliseconds random_time{rand() % 1000000};

    admin_event_doc.event_expiration_time = bsoncxx::types::b_date{random_time - std::chrono::milliseconds{1}};
    admin_event_doc.setIntoCollection();

    user_event_commands::CancelEventRequest cancel_event_request;
    user_event_commands::CancelEventResponse cancel_event_response;

    setupUserLoginInfo(
            cancel_event_request.mutable_login_info(),
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front()
    );

    cancel_event_request.set_event_oid(user_event_account_oid.to_string());

    cancelEvent(&cancel_event_request, &cancel_event_response);

    ASSERT_EQ(cancel_event_response.return_status(), ReturnStatus::SUCCESS);

    request.set_expiration_time_search(admin_event_commands::GetEventsRequest_TimeSearch_LESS_THAN);
    request.set_events_expiration_time_ms(random_time.count());
    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    ASSERT_EQ(response.events().size(), 1);

    successfullyReturnedAdminEvent(
            response.events(0),
            std::chrono::milliseconds{response.timestamp_ms()}
    );
}

TEST_F(GetEventsTesting, noEventsFound) {
    request.set_start_time_search(admin_event_commands::GetEventsRequest_TimeSearch_LESS_THAN);
    request.set_events_start_time_ms(-5000);
    runFunction();

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());
}
