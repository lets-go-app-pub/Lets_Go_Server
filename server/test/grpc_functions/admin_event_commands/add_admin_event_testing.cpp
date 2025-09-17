//
// Created by jeremiah on 3/19/23.
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
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "UserEventCommands.pb.h"
#include "event_request_message_is_valid.h"
#include "generate_randoms.h"
#include "connection_pool_global_variable.h"
#include "check_for_valid_event_request_message.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "admin_event_commands.h"
#include "event_admin_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class AddAdminEventTesting : public ::testing::Test {
protected:

    admin_event_commands::AddAdminEventRequest request;
    admin_event_commands::AddAdminEventResponse response;

    std::chrono::milliseconds current_timestamp{-1};

    EventRequestMessage event_request;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        createAndStoreEventAdminAccount();

        const EventChatRoomAdminInfo chat_room_admin_info(
                UserAccountType::ADMIN_GENERATED_EVENT_TYPE
        );

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

        setupRequest(event_request);
    }

    void setupRequest(const EventRequestMessage& _event_request) {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.mutable_event_request()->CopyFrom(_event_request);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        addAdminEvent(&request, &response);
    }

    void compareSuccessfullyCreatedEvent(
            const bsoncxx::oid& event_oid,
            const std::chrono::milliseconds& timestamp_used
    ) {
        UserAccountDoc generated_user_event;
        generated_user_event.getFromEventRequestMessageAfterStored(
                event_request,
                event_oid,
                UserAccountType::ADMIN_GENERATED_EVENT_TYPE,
                timestamp_used,
                TEMP_ADMIN_ACCOUNT_NAME,
                response.chat_room_id()
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

    void compareSuccessfullyCreatedEventChatRoom(
            const bsoncxx::oid& event_oid,
            const std::chrono::milliseconds& timestamp_used
    ) {
        ChatRoomHeaderDoc extracted_chat_room_header(response.chat_room_id());

        ChatRoomHeaderDoc generated_chat_room_header;
        generated_chat_room_header.generateHeaderForEventChatRoom(
                response.chat_room_id(),
                extracted_chat_room_header.chat_room_name,
                extracted_chat_room_header.chat_room_password,
                timestamp_used + std::chrono::milliseconds{2},
                event_oid,
                event_request.qr_code_file_in_bytes().empty() ? chat_room_values::QR_CODE_DEFAULT : event_request.qr_code_file_in_bytes(),
                event_request.qr_code_file_in_bytes().empty() || event_request.qr_code_message().empty() ? chat_room_values::QR_CODE_MESSAGE_DEFAULT : event_request.qr_code_message(),
                event_request.qr_code_file_in_bytes().empty() ? std::chrono::milliseconds{chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT} : std::chrono::milliseconds{timestamp_used + std::chrono::milliseconds{2}},
                event_request.location_longitude(),
                event_request.location_latitude(),
                event_request.min_allowed_age(),
                timestamp_used,
                event_admin_values::OID
        );

        EXPECT_EQ(generated_chat_room_header, extracted_chat_room_header);
    }

    void checkNothingChanged() {
        const long num_accounts = user_accounts_collection.count_documents(document{} << finalize);

        //Make sure event account was not added. User account should be only document in collection.
        EXPECT_EQ(num_accounts, 1);
    }

    void createEventSuccessfully() {
        runFunction();

        ASSERT_TRUE(response.successful());
        ASSERT_GT(response.timestamp_ms(), 0);

        const bsoncxx::oid response_event_oid{response.event_oid()};
        const std::chrono::milliseconds timestamp_used{response.timestamp_ms()};

        compareSuccessfullyCreatedEvent(response_event_oid, timestamp_used);

        compareSuccessfullyCreatedEventChatRoom(response_event_oid, timestamp_used);
    }
};

TEST_F(AddAdminEventTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info)->bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_message().empty());
                return response.successful();
            }
    );

    checkNothingChanged();
}

TEST_F(AddAdminEventTesting, invalidAdminPrivilegeLevel) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());

    checkNothingChanged();
}

TEST_F(AddAdminEventTesting, invalidEventRequestChecked) {
    checkForValidEventRequestMessage(
            event_request,
            [&](const EventRequestMessage& event_request) -> bool {
                setupRequest(event_request);

                response.Clear();
                runFunction();

                return response.successful();
            }
    );

    checkNothingChanged();

    //Make sure the event_request was valid to start by successfully running it.
    setupRequest(event_request);

    createEventSuccessfully();
}

TEST_F(AddAdminEventTesting, successfullyCreatedEvent) {
    createEventSuccessfully();
}

TEST_F(AddAdminEventTesting, successfull_noQrCodePassed) {
    event_request.set_qr_code_file_size(0);
    event_request.clear_qr_code_file_in_bytes();

    setupRequest(event_request);

    createEventSuccessfully();
}

TEST_F(AddAdminEventTesting, successfull_qrCodePassed_noQrCodeMessage) {
    const std::string dummy_qr_code = gen_random_alpha_numeric_string(rand() % server_parameter_restrictions::MAXIMUM_QR_CODE_SIZE_IN_BYTES + 1);
    event_request.set_qr_code_file_size(dummy_qr_code.size());
    event_request.set_qr_code_file_in_bytes(dummy_qr_code);

    event_request.clear_qr_code_message();

    setupRequest(event_request);

    createEventSuccessfully();
}
