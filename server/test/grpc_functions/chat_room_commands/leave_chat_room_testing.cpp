//
// Created by jeremiah on 8/23/22.
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
#include "chat_room_commands_helper_functions.h"
#include "generate_randoms.h"
#include "utility_chat_functions_test.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GrpcLeaveChatRoomTesting : public ::testing::Test {
protected:

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

};

TEST_F(GrpcLeaveChatRoomTesting, invalidLoginInfo) {

    bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(user_account_oid);

    checkLoginInfoClientOnly(
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {
                grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
                grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

                leave_chat_room_request.mutable_login_info()->CopyFrom(login_info);
                leave_chat_room_request.set_chat_room_id(generateRandomChatRoomId());

                leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

                return leave_chat_room_response.return_status();
            }
    );

    UserAccountDoc user_account_doc_after(user_account_oid);

    EXPECT_EQ(user_account_doc, user_account_doc_after);
}

TEST_F(GrpcLeaveChatRoomTesting, invalidChatRoomId) {

    bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(user_account_oid);

    grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
    grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

    setupUserLoginInfo(
            leave_chat_room_request.mutable_login_info(),
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front()
    );

    leave_chat_room_request.set_chat_room_id("123");

    leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

    EXPECT_EQ(leave_chat_room_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

//NOTE: Most of the functionality is handled internally with the inside leaveChatRoom() function (tested elsewhere).
TEST_F(GrpcLeaveChatRoomTesting, properInput) {

    bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(user_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    ChatRoomHeaderDoc original_chat_room_header(chat_room_id);

    grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
    grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

    setupUserLoginInfo(
            leave_chat_room_request.mutable_login_info(),
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front()
    );

    leave_chat_room_request.set_chat_room_id(chat_room_id);

    leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

    UserPictureDoc thumbnail_picture;

    //get UserPictureDoc object for the generated user's thumbnail
    for (const auto& pic : user_account_doc.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            thumbnail_picture.getFromCollection(picture_reference);

            break;
        }
    }

    checkLeaveChatRoomResult(
            getCurrentTimestamp(),
            original_chat_room_header,
            user_account_oid,
            thumbnail_picture.current_object_oid.to_string(),
            thumbnail_picture.thumbnail_in_bytes,
            create_chat_room_response.chat_room_id(),
            leave_chat_room_response.return_status()
    );
}
