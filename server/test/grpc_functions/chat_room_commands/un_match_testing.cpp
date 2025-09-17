//
// Created by jeremiah on 8/24/22.
//

#include <fstream>
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
#include "build_match_made_chat_room.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GrpcUnMatchTesting : public ::testing::Test {
protected:

    SetupTestingForUnMatch values;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        //must be called AFTER the database is cleared
        values.initialize();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(GrpcUnMatchTesting, invalidLoginValues) {
    checkLoginInfoClientOnly(
        values.generated_account_oid,
        values.user_account.logged_in_token,
        values.user_account.installation_ids.front(),
        [&](const LoginToServerBasicInfo& login_info)-> ReturnStatus {
            grpc_chat_commands::UnMatchRequest un_match_request;
            grpc_chat_commands::UnMatchResponse un_match_response;

            un_match_request.mutable_login_info()->CopyFrom(login_info);
            un_match_request.set_chat_room_id(values.matching_chat_room_id);
            un_match_request.set_matched_account_oid(values.second_generated_account_oid.to_string());

            unMatch(&un_match_request, &un_match_response);

            return un_match_response.return_status();
        }
    );

    UserAccountDoc user_account_after(values.generated_account_oid);
    UserAccountDoc second_user_account_after(values.second_generated_account_oid);

    ChatRoomHeaderDoc original_chat_room_header_after(values.matching_chat_room_id);

    UserPictureDoc thumbnail_picture_after(values.thumbnail_picture.current_object_oid);
    UserPictureDoc second_thumbnail_picture_after(values.second_thumbnail_picture.current_object_oid);

    EXPECT_EQ(user_account_after, values.user_account);
    EXPECT_EQ(second_user_account_after, values.second_user_account);

    EXPECT_EQ(original_chat_room_header_after, values.original_chat_room_header);

    EXPECT_EQ(thumbnail_picture_after, values.thumbnail_picture);
    EXPECT_EQ(second_thumbnail_picture_after, values.second_thumbnail_picture);
}

TEST_F(GrpcUnMatchTesting, invalidChatRoomId) {
    grpc_chat_commands::UnMatchRequest un_match_request;
    grpc_chat_commands::UnMatchResponse un_match_response;

    setupUserLoginInfo(
            un_match_request.mutable_login_info(),
            values.generated_account_oid,
            values.user_account.logged_in_token,
            values.user_account.installation_ids.front()
    );

    un_match_request.set_chat_room_id("values.matching_chat_room_id");
    un_match_request.set_matched_account_oid(values.second_generated_account_oid.to_string());

    unMatch(&un_match_request, &un_match_response);

    EXPECT_EQ(un_match_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(GrpcUnMatchTesting, invalidMatchedAccountOid) {
    grpc_chat_commands::UnMatchRequest un_match_request;
    grpc_chat_commands::UnMatchResponse un_match_response;

    setupUserLoginInfo(
            un_match_request.mutable_login_info(),
            values.generated_account_oid,
            values.user_account.logged_in_token,
            values.user_account.installation_ids.front()
    );

    un_match_request.set_chat_room_id(values.matching_chat_room_id);
    un_match_request.set_matched_account_oid("values.second_generated_account_oid.to_string()");

    unMatch(&un_match_request, &un_match_response);

    EXPECT_EQ(un_match_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(GrpcUnMatchTesting, matchedOidSameAsSendingOid) {
    grpc_chat_commands::UnMatchRequest un_match_request;
    grpc_chat_commands::UnMatchResponse un_match_response;

    setupUserLoginInfo(
            un_match_request.mutable_login_info(),
            values.generated_account_oid,
            values.user_account.logged_in_token,
            values.user_account.installation_ids.front()
    );

    un_match_request.set_chat_room_id(values.matching_chat_room_id);
    un_match_request.set_matched_account_oid(values.generated_account_oid.to_string());

    unMatch(&un_match_request, &un_match_response);

    EXPECT_EQ(un_match_response.return_status(), ReturnStatus::SUCCESS);
}

TEST_F(GrpcUnMatchTesting, successful) {

    grpc_chat_commands::UnMatchRequest un_match_request;
    grpc_chat_commands::UnMatchResponse un_match_response;

    setupUserLoginInfo(
            un_match_request.mutable_login_info(),
            values.generated_account_oid,
            values.user_account.logged_in_token,
            values.user_account.installation_ids.front()
    );

    un_match_request.set_chat_room_id(values.matching_chat_room_id);
    un_match_request.set_matched_account_oid(values.second_generated_account_oid.to_string());

    unMatch(&un_match_request, &un_match_response);

    EXPECT_EQ(un_match_response.return_status(), ReturnStatus::SUCCESS);

    checkIfUnmatchSuccessful(
            values.original_chat_room_header,
            std::chrono::milliseconds{un_match_response.timestamp()},
            values.user_account,
            values.second_user_account,
            values.thumbnail_picture,
            values.second_thumbnail_picture,
            values.matching_chat_room_id,
            values.generated_account_oid,
            values.second_generated_account_oid
    );
}
