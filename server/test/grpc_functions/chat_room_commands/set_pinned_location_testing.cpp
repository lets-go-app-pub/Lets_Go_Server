//
// Created by jeremiah on 3/18/23.
//

#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/collection.hpp>
#include <account_objects.h>
#include <reports_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "grpc_mock_stream/mock_stream.h"
#include "generate_randoms.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetPinnedLocationTesting : public ::testing::Test {
protected:

    bsoncxx::oid admin_account_oid;

    UserAccountDoc admin_account_doc;

    std::string chat_room_id;
    std::string chat_room_password;
    const std::string message_uuid = generateUUID();

    grpc_chat_commands::SetPinnedLocationRequest request;
    grpc_chat_commands::SetPinnedLocationResponse response;

    ChatRoomHeaderDoc generated_header_doc;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        admin_account_oid = insertRandomAccounts(1, 0);

        admin_account_doc.getFromCollection(admin_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                admin_account_oid,
                admin_account_doc.logged_in_token,
                admin_account_doc.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();
        chat_room_password = create_chat_room_response.chat_room_password();

        admin_account_doc.getFromCollection(admin_account_oid);

        //Sleep to guarantee last active times will be different when running promoteNewAdmin().
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

        setupUserLoginInfo(
                request.mutable_login_info(),
                admin_account_oid,
                admin_account_doc.logged_in_token,
                admin_account_doc.installation_ids.front()
        );

        request.set_chat_room_id(chat_room_id);

        storeValidLocation(request);

        request.set_message_uuid(message_uuid);

        generated_header_doc.getFromCollection(chat_room_id);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    static void storeValidLocation(grpc_chat_commands::SetPinnedLocationRequest& set_pinned_location_request) {
        // the precision of doubles.
        int precision = 10e4;

        double generated_longitude = rand() % (180*precision);
        if(rand() % 2) {
            generated_longitude *= -1;
        }
        generated_longitude /= precision;

        double generated_latitude = rand() % (90*precision);
        if(rand() % 2) {
            generated_latitude *= -1;
        }
        generated_latitude /= precision;

        set_pinned_location_request.set_new_pinned_longitude(generated_longitude);
        set_pinned_location_request.set_new_pinned_latitude(generated_latitude);
    }

    void runFunction() {
        setPinnedLocation(&request, &response);
    }

    void invalidParameterReturn() {
        EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
        EXPECT_EQ(response.operation_failed(), false);
    }

    void noUpdatesToDocs() {
        UserAccountDoc admin_account_doc_after(admin_account_oid);
        EXPECT_EQ(admin_account_doc, admin_account_doc_after);

        ChatRoomHeaderDoc extracted_header_doc(chat_room_id);
        EXPECT_EQ(extracted_header_doc, generated_header_doc);

        ChatRoomMessageDoc chat_room_message(message_uuid, chat_room_id);
        EXPECT_NE(chat_room_message.id, message_uuid);
    }

    void checkForSuccessfulReturnValues() {
        EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
        EXPECT_EQ(response.operation_failed(), false);
    }

    void checkForSuccessfulUserAccountUpdate() {
        UserAccountDoc admin_account_doc_after(admin_account_oid);

        EXPECT_EQ(admin_account_doc.chat_rooms.size(), 1);
        EXPECT_EQ(admin_account_doc_after.chat_rooms.size(), 1);
        if(!admin_account_doc.chat_rooms.empty()
           && !admin_account_doc_after.chat_rooms.empty()
                ) {
            EXPECT_GT(
                    admin_account_doc_after.chat_rooms[0].last_time_viewed,
                    admin_account_doc.chat_rooms[0].last_time_viewed
            );
            admin_account_doc.chat_rooms[0].last_time_viewed = admin_account_doc_after.chat_rooms[0].last_time_viewed;
        }

        EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    }

    void checkForSuccessfulHeaderDocUpdate() {
        ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

        EXPECT_GT(extracted_header_doc.chat_room_last_active_time, generated_header_doc.chat_room_last_active_time);
        generated_header_doc.chat_room_last_active_time = extracted_header_doc.chat_room_last_active_time;

        EXPECT_EQ(generated_header_doc.accounts_in_chat_room.size(), 1);
        EXPECT_EQ(extracted_header_doc.accounts_in_chat_room.size(), 1);

        bsoncxx::types::b_date extract_admin_last_active_time{std::chrono::milliseconds{-1}};
        for(const auto& x : extracted_header_doc.accounts_in_chat_room) {
            if(x.account_oid == admin_account_oid) {
                extract_admin_last_active_time = x.last_activity_time;
                break;
            }
        }

        for(auto& x : generated_header_doc.accounts_in_chat_room) {
            if(x.account_oid == admin_account_oid) {
                EXPECT_GT(extract_admin_last_active_time, x.last_activity_time);
                x.last_activity_time = extract_admin_last_active_time;
            }
        }

        generated_header_doc.pinned_location.emplace(
                ChatRoomHeaderDoc::PinnedLocation{
                        request.new_pinned_longitude(),
                        request.new_pinned_latitude()
                }
        );

        EXPECT_EQ(extracted_header_doc, generated_header_doc);
    }

    void checkForSuccessfulChatMessageStored() {
        ChatRoomMessageDoc chat_room_message(message_uuid, chat_room_id);

        EXPECT_EQ(chat_room_message.id, message_uuid);
        if(!chat_room_message.id.empty()) {
            EXPECT_EQ(chat_room_message.message_type, MessageSpecifics::MessageBodyCase::kNewPinnedLocationMessage);
        }
    }
};

TEST_F(SetPinnedLocationTesting, invalidLoginInfo) {

    checkLoginInfoClientOnly(
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {
                grpc_chat_commands::SetPinnedLocationResponse set_pinned_location_response;

                request.mutable_login_info()->CopyFrom(login_info);

                setPinnedLocation(&request, &set_pinned_location_response);

                return set_pinned_location_response.return_status();
            }
    );

    UserAccountDoc admin_account_doc_after(admin_account_oid);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
}

TEST_F(SetPinnedLocationTesting, invalidChatRoomId) {

    request.set_chat_room_id(generateRandomChatRoomId() + "aa");

    runFunction();

    invalidParameterReturn();

    noUpdatesToDocs();
}

TEST_F(SetPinnedLocationTesting, invalidLongitude) {
    request.set_new_pinned_longitude(181);

    runFunction();

    invalidParameterReturn();

    noUpdatesToDocs();
}

TEST_F(SetPinnedLocationTesting, invalidLatitude) {
    request.set_new_pinned_latitude(91);

    runFunction();

    invalidParameterReturn();

    noUpdatesToDocs();
}

TEST_F(SetPinnedLocationTesting, invalidMessageUuid) {
    request.set_message_uuid("abc");

    runFunction();

    invalidParameterReturn();

    noUpdatesToDocs();
}

TEST_F(SetPinnedLocationTesting, successfullyRan) {
    runFunction();

    checkForSuccessfulReturnValues();

    checkForSuccessfulUserAccountUpdate();

    checkForSuccessfulHeaderDocUpdate();

    checkForSuccessfulChatMessageStored();
}

TEST_F(SetPinnedLocationTesting, successfullyRan_passedDefaultLatLongValues) {
    request.set_new_pinned_longitude(chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE);
    request.set_new_pinned_latitude(chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE);

    runFunction();

    checkForSuccessfulReturnValues();

    checkForSuccessfulUserAccountUpdate();

    checkForSuccessfulHeaderDocUpdate();

    checkForSuccessfulChatMessageStored();
}

TEST_F(SetPinnedLocationTesting, sendingUserNotAdminOfChatRoom) {

    const bsoncxx::oid joined_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc joined_account_doc(joined_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            joined_account_oid,
            joined_account_doc.logged_in_token,
            joined_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    EXPECT_EQ(mock_server_writer.write_params.size(), 2);
    if (mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    joined_account_doc.getFromCollection(joined_account_oid);
    generated_header_doc.getFromCollection(chat_room_id);

    setupUserLoginInfo(
            request.mutable_login_info(),
            joined_account_oid,
            joined_account_doc.logged_in_token,
            joined_account_doc.installation_ids.front()
    );

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.operation_failed(), true);

    UserAccountDoc joined_account_doc_after(joined_account_oid);
    EXPECT_EQ(joined_account_doc, joined_account_doc_after);

    noUpdatesToDocs();
}

TEST_F(SetPinnedLocationTesting, chatRoomDoesNotExist) {
    request.set_chat_room_id(generateRandomChatRoomId());

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.operation_failed(), true);

    noUpdatesToDocs();
}

TEST_F(SetPinnedLocationTesting, userNotInChatRoomHeader) {
    const bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_account_doc(second_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_account_doc.logged_in_token,
            second_account_doc.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    second_account_doc.getFromCollection(second_account_oid);

    request.set_chat_room_id(create_chat_room_response.chat_room_id());

    ChatRoomHeaderDoc second_chat_room_header_doc(create_chat_room_response.chat_room_id());

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.operation_failed(), true);

    noUpdatesToDocs();

    UserAccountDoc second_account_doc_after(second_account_oid);
    EXPECT_EQ(second_account_doc, second_account_doc_after);

    ChatRoomHeaderDoc second_chat_room_header_doc_after(create_chat_room_response.chat_room_id());
    EXPECT_EQ(second_chat_room_header_doc, second_chat_room_header_doc_after);
}
