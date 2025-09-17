//
// Created by jeremiah on 3/18/23.
//

#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include <gtest/gtest.h>
#include <google/protobuf/util/message_differencer.h>

#include "clear_database_for_testing.h"
#include "chat_rooms_objects.h"
#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "UserEventCommands.pb.h"
#include "user_event_commands.h"
#include "event_request_message_is_valid.h"
#include "generate_randoms.h"
#include "connection_pool_global_variable.h"
#include "check_for_valid_event_request_message.h"
#include "activities_info_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class AddEventTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    user_event_commands::AddUserEventRequest request;
    user_event_commands::AddUserEventResponse response;

    std::chrono::milliseconds current_timestamp{-1};

    EventRequestMessage event_request;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        const EventChatRoomAdminInfo chat_room_admin_info{
                UserAccountType::USER_ACCOUNT_TYPE,
                user_account_oid.to_string(),
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front()
        };

        event_request = generateRandomEventRequestMessage(
                chat_room_admin_info,
                current_timestamp
        );

        current_timestamp = getCurrentTimestamp();

        EXPECT_FALSE(event_request.activity().time_frame_array().empty());
        if (!event_request.activity().time_frame_array().empty()
            && event_request.activity().time_frame_array(0).start_time_frame() < current_timestamp.count()) {
            const int diff = (int) event_request.activity().time_frame_array(0).stop_time_frame() -
                             (int) event_request.activity().time_frame_array(0).start_time_frame();

            event_request.mutable_activity()->mutable_time_frame_array(0)->set_start_time_frame(
                    current_timestamp.count() +
                    2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());
            event_request.mutable_activity()->mutable_time_frame_array(0)->set_stop_time_frame(
                    event_request.activity().time_frame_array(0).start_time_frame() + diff);
        }

        user_account_doc.subscription_status = UserSubscriptionStatus::BASIC_SUBSCRIPTION;
        user_account_doc.setIntoCollection();

        setupRequest(event_request);
    }

    void setupRequest(const EventRequestMessage& _event_request) {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front()
        );

        request.mutable_event_request()->CopyFrom(_event_request);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        addUserEvent(&request, &response);
    }

    void compareSuccessfullyCreatedEvent(
            const bsoncxx::oid& event_oid,
            const std::chrono::milliseconds& timestamp_used
            ) {
        UserAccountDoc generated_user_event;
        generated_user_event.getFromEventRequestMessageAfterStored(
                event_request,
                event_oid,
                UserAccountType::USER_GENERATED_EVENT_TYPE,
                timestamp_used,
                user_account_oid.to_string(),
                response.chat_room_info().chat_room_id()
        );

        UserAccountDoc extracted_user_event(event_oid);
        EXPECT_EQ(generated_user_event, extracted_user_event);

        //make sure event picture docs exist and are correct
        int num_pictures = 0;
        int index = 0;
        for (const auto& pic: extracted_user_event.pictures) {
            if (pic.pictureStored()) {
                if (index < event_request.pictures().size()) {
                    auto pic_msg_info = event_request.pictures()[index];

                    bsoncxx::oid picture_reference;
                    bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

                    pic.getPictureReference(
                            picture_reference,
                            timestamp_stored
                    );

                    UserPictureDoc generated_picture_doc;
                    generated_picture_doc.getFromEventsPictureMessage(
                            pic_msg_info,
                            picture_reference,
                            timestamp_stored,
                            event_oid,
                            index
                    );

                    UserPictureDoc extracted_picture_doc(picture_reference);
                    EXPECT_EQ(extracted_picture_doc, generated_picture_doc);

                } else {
                    EXPECT_FALSE(true);
                }
                num_pictures++;
            }
            index++;
        }
        ASSERT_EQ(event_request.pictures().size(), num_pictures);
    }

    void compareSuccessfullyUpdateUserAccount(const bsoncxx::oid& event_oid) {
        user_account_doc.user_created_events.emplace_back(
                event_oid,
                bsoncxx::types::b_date{
                        std::chrono::milliseconds{event_request.activity().time_frame_array(0).stop_time_frame()}
                        - matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED
                },
                LetsGoEventStatus::ONGOING
        );

        user_account_doc.chat_rooms.emplace_back(
                response.chat_room_info().chat_room_id(),
                bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp_ms() + 2}},
                std::optional<bsoncxx::oid>{event_oid}
        );

        UserAccountDoc extracted_user_account(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account);
    }

    void compareSuccessfullyCreatedEventChatRoom(
            const bsoncxx::oid& event_oid,
            const std::chrono::milliseconds& timestamp_used
            ) {

        ChatRoomHeaderDoc generated_chat_room_header;
        generated_chat_room_header.generateHeaderForEventChatRoom(
                response.chat_room_info().chat_room_id(),
                response.chat_room_info().chat_room_name(),
                response.chat_room_info().chat_room_password(),
                std::chrono::milliseconds{response.chat_room_info().last_activity_time_timestamp()},
                event_oid,
                chat_room_values::QR_CODE_DEFAULT,
                chat_room_values::QR_CODE_MESSAGE_DEFAULT,
                std::chrono::milliseconds{chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT},
                event_request.location_longitude(),
                event_request.location_latitude(),
                event_request.min_allowed_age(),
                timestamp_used,
                user_account_oid
        );

        ChatRoomHeaderDoc extracted_chat_room_header(response.chat_room_info().chat_room_id());
        EXPECT_EQ(generated_chat_room_header, extracted_chat_room_header);

        ChatRoomMessageDoc cap_message(
                response.chat_room_info().chat_room_cap_message().message_uuid(),
                response.chat_room_info().chat_room_id()
        );
        EXPECT_EQ(cap_message.message_type, MessageSpecifics::MessageBodyCase::kChatRoomCapMessage);

        ChatRoomMessageDoc current_user_joined_message(
                response.chat_room_info().current_user_joined_chat_message().message_uuid(),
                response.chat_room_info().chat_room_id()
        );
        EXPECT_EQ(current_user_joined_message.message_type, MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage);
    }

    void checkNothingChanged() {
        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(extracted_user_account_doc, user_account_doc);

        const long num_accounts = user_accounts_collection.count_documents(document{} << finalize);

        //Make sure event account was not added. User account should be only document in collection.
        EXPECT_EQ(num_accounts, 1);
    }

    void createEventSuccessfully() {
        runFunction();

        ASSERT_EQ(response.return_status(), ReturnStatus::SUCCESS);
        ASSERT_GT(response.timestamp_ms(), 0);

        const bsoncxx::oid response_event_oid{response.event_oid()};
        const std::chrono::milliseconds timestamp_used{response.timestamp_ms()};

        compareSuccessfullyCreatedEvent(response_event_oid, timestamp_used);

        compareSuccessfullyUpdateUserAccount(response_event_oid);

        compareSuccessfullyCreatedEventChatRoom(response_event_oid, timestamp_used);
    }

    void extractActivityIndexForPassedAge(const int ACTIVITY_MIN_AGE, int& activity_index) {
        mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

        bsoncxx::stdx::optional<bsoncxx::document::value> optional_activities_doc = activities_info_collection.find_one(
                document{}
                        << "_id" << activities_info_keys::ID
                        << finalize
        );

        if(!optional_activities_doc) {
            EXPECT_TRUE(false);
            return;
        }

        const bsoncxx::document::view activities_doc = optional_activities_doc->view();

        const bsoncxx::array::view categories_array = activities_doc[activities_info_keys::CATEGORIES].get_array().value;
        const bsoncxx::array::view activities_array = activities_doc[activities_info_keys::ACTIVITIES].get_array().value;

        //find an activity that is age 21 (the category must be 21 too)
        activity_index = 0;
        for(const auto& activity : activities_array) {
            const bsoncxx::document::view activity_doc = activity.get_document().value;
            const int activity_min_age = activity_doc[activities_info_keys::activities::MIN_AGE].get_int32().value;
            if(activity_min_age == ACTIVITY_MIN_AGE && activity_index > 0) {
                const int category_index = activity_doc[activities_info_keys::activities::CATEGORY_INDEX].get_int32().value;
                const bsoncxx::document::view category_doc = categories_array[category_index].get_document().value;
                const int category_min_age = category_doc[activities_info_keys::categories::MIN_AGE].get_int32().value;
                if(category_min_age == ACTIVITY_MIN_AGE) {
                    break;
                }
            }
            activity_index++;
        }
    }
};

TEST_F(AddEventTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                return response.return_status();
            }
    );

    checkNothingChanged();
}

TEST_F(AddEventTesting, invalidQrCodeMessage) {
    request.mutable_event_request()->set_qr_code_message("123");

    runFunction();

    ASSERT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    checkNothingChanged();
}

TEST_F(AddEventTesting, invalidQrCodeFileSize) {
    request.mutable_event_request()->set_qr_code_file_size(1);

    runFunction();

    ASSERT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    checkNothingChanged();
}

TEST_F(AddEventTesting, invalidQrCodeFileInBytes) {
    request.mutable_event_request()->set_qr_code_file_in_bytes("a");
    request.mutable_event_request()->set_qr_code_file_size(1);

    runFunction();

    ASSERT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    checkNothingChanged();
}

TEST_F(AddEventTesting, invalidEventRequestChecked) {
    checkForValidEventRequestMessage(
            event_request,
            [&](const EventRequestMessage& event_request) -> bool {
                setupRequest(event_request);

                response.Clear();
                runFunction();

                return response.return_status() == ReturnStatus::SUCCESS;
            }
    );

    checkNothingChanged();

    //Make sure the event_request was valid to start by successfully running it.
    setupRequest(event_request);

    createEventSuccessfully();
}

TEST_F(AddEventTesting, userTooYoungToHostEvent) {

    const static int ACTIVITY_MIN_AGE = 21;
    int activity_index = 0;
    extractActivityIndexForPassedAge(ACTIVITY_MIN_AGE, activity_index);

    ASSERT_NE(activity_index, 0);

    int birth_year;
    int birth_day_year;
    generateBirthYearForPassedAge(
            server_parameter_restrictions::LOWEST_ALLOWED_AGE,
            birth_year,
            user_account_doc.birth_month,
            user_account_doc.birth_day_of_month,
            birth_day_year
    );

    user_account_doc.age = server_parameter_restrictions::LOWEST_ALLOWED_AGE;
    user_account_doc.birth_year = birth_year;
    user_account_doc.birth_day_of_year = birth_day_year;
    user_account_doc.setIntoCollection();

    request.mutable_event_request()->set_min_allowed_age(ACTIVITY_MIN_AGE);
    request.mutable_event_request()->mutable_activity()->set_activity_index(activity_index);

    runFunction();

    ASSERT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    checkNothingChanged();
}

TEST_F(AddEventTesting, successfullyCreatedEvent) {
    createEventSuccessfully();
}
