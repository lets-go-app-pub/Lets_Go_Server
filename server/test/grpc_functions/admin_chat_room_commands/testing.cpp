//
// Created by jeremiah on 7/28/22.
//

#include <utility_general_functions.h>
#include <general_values.h>
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <database_names.h>
#include <account_objects.h>
#include <reports_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "admin_chat_room_commands.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "generate_random_messages.h"
#include "connection_pool_global_variable.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>
#include "utility_chat_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class AdminChatRoomCommandsTests : public ::testing::Test {
protected:

    std::string chat_room_id;
    std::string message_uuid;
    std::chrono::milliseconds message_timestamp_stored{-1};

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::unique_ptr<ExtractUserInfoObjects> extractUserInfoObjects;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
        const std::string generated_account_oid_str = generated_account_oid.to_string();
        UserAccountDoc generated_user_account(generated_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                generated_account_oid,
                generated_user_account.logged_in_token,
                generated_user_account.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();
        message_uuid = generateUUID();

        //sleep while capMessage and kDifferentUserJoined are sent
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

        auto [text_message_request, text_message_response] = generateRandomTextMessage(
                generated_account_oid,
                generated_user_account.logged_in_token,
                generated_user_account.installation_ids.front(),
                chat_room_id,
                message_uuid,
                gen_random_alpha_numeric_string(
                        rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
                )
        );

        ASSERT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

        message_timestamp_stored = std::chrono::milliseconds{text_message_response.timestamp_stored()};

        createTempAdminAccount(AdminLevelEnum::PRIMARY_DEVELOPER);

        extractUserInfoObjects = std::make_unique<ExtractUserInfoObjects>(mongo_cpp_client, accounts_db, user_accounts_collection);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(AdminChatRoomCommandsTests, extractExistingMessage) {

    admin_chat_commands::GetSingleMessageRequest request;
    admin_chat_commands::GetSingleMessageResponse returned_response;

    setupAdminLoginInfo(
            request.mutable_login_info(),
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    request.set_chat_room_id(chat_room_id);
    request.set_message_uuid(message_uuid);
    request.set_timestamp_of_message(message_timestamp_stored.count());

    getSingleMessage(&request, &returned_response);

    returned_response.message();

    ChatRoomMessageDoc message(message_uuid, chat_room_id);

    bsoncxx::builder::stream::document message_doc;
    message.convertToDocument(message_doc);

    ChatMessageToClient extracted_message;

    convertChatMessageDocumentToChatMessageToClient(
            message_doc.view(),
            chat_room_id,
            general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT,
            false,
            &extracted_message,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extractUserInfoObjects.get()
    );

    EXPECT_TRUE(returned_response.successful());
    EXPECT_TRUE(returned_response.error_message().empty());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            returned_response.message(),
            extracted_message
    );

    if(!equivalent) {
        std::cout << "returned_response.message()\n" << returned_response.message().DebugString() << '\n';
        std::cout << "extracted_message\n" << extracted_message.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}

TEST_F(AdminChatRoomCommandsTests, messageUuidDoesNotExist) {
    admin_chat_commands::GetSingleMessageRequest request;
    admin_chat_commands::GetSingleMessageResponse returned_response;

    setupAdminLoginInfo(
            request.mutable_login_info(),
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    request.set_chat_room_id(chat_room_id);
    request.set_message_uuid(generateUUID());
    request.set_timestamp_of_message(message_timestamp_stored.count());

    getSingleMessage(&request, &returned_response);

    ChatMessageToClient empty_message;

    EXPECT_FALSE(returned_response.successful());
    EXPECT_FALSE(returned_response.error_message().empty());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            returned_response.message(),
            empty_message
    );

    if(!equivalent) {
        std::cout << "returned_response.message()\n" << returned_response.message().DebugString() << '\n';
        std::cout << "extracted_message\n" << empty_message.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);

}

TEST_F(AdminChatRoomCommandsTests, invalidChatRoomId) {
    admin_chat_commands::GetSingleMessageRequest request;
    admin_chat_commands::GetSingleMessageResponse returned_response;

    setupAdminLoginInfo(
            request.mutable_login_info(),
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    request.set_chat_room_id("123");
    request.set_message_uuid(message_uuid);
    request.set_timestamp_of_message(message_timestamp_stored.count());

    getSingleMessage(&request, &returned_response);

    ChatMessageToClient empty_message;

    EXPECT_FALSE(returned_response.successful());
    EXPECT_FALSE(returned_response.error_message().empty());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            returned_response.message(),
            empty_message
    );

    if(!equivalent) {
        std::cout << "returned_response.message()\n" << returned_response.message().DebugString() << '\n';
        std::cout << "extracted_message\n" << empty_message.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}

TEST_F(AdminChatRoomCommandsTests, invalidMessageUuid) {
    admin_chat_commands::GetSingleMessageRequest request;
    admin_chat_commands::GetSingleMessageResponse returned_response;

    setupAdminLoginInfo(
            request.mutable_login_info(),
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    request.set_chat_room_id(chat_room_id);
    request.set_message_uuid("123");
    request.set_timestamp_of_message(message_timestamp_stored.count());

    getSingleMessage(&request, &returned_response);

    ChatMessageToClient empty_message;

    EXPECT_FALSE(returned_response.successful());
    EXPECT_FALSE(returned_response.error_message().empty());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            returned_response.message(),
            empty_message
    );

    if(!equivalent) {
        std::cout << "returned_response.message()\n" << returned_response.message().DebugString() << '\n';
        std::cout << "extracted_message\n" << empty_message.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}

TEST_F(AdminChatRoomCommandsTests, invalidTimestampOfMessage) {
    admin_chat_commands::GetSingleMessageRequest request;
    admin_chat_commands::GetSingleMessageResponse returned_response;

    setupAdminLoginInfo(
            request.mutable_login_info(),
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    request.set_chat_room_id(chat_room_id);
    request.set_message_uuid(message_uuid);
    request.set_timestamp_of_message(-1);

    getSingleMessage(&request, &returned_response);

    ChatMessageToClient empty_message;

    EXPECT_FALSE(returned_response.successful());
    EXPECT_FALSE(returned_response.error_message().empty());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            returned_response.message(),
            empty_message
    );

    if(!equivalent) {
        std::cout << "returned_response.message()\n" << returned_response.message().DebugString() << '\n';
        std::cout << "extracted_message\n" << empty_message.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}

TEST_F(AdminChatRoomCommandsTests, invalidLoginInfo) {

    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info)->bool {
                admin_chat_commands::GetSingleMessageRequest request;
                admin_chat_commands::GetSingleMessageResponse returned_response;

                request.mutable_login_info()->CopyFrom(login_info);

                request.set_chat_room_id(chat_room_id);
                request.set_message_uuid(message_uuid);
                request.set_timestamp_of_message(-1);

                getSingleMessage(&request, &returned_response);

                return returned_response.successful();
            }
    );
}

TEST_F(AdminChatRoomCommandsTests, adminDoesNotHavePrivilegeLevel) {

    AdminAccountDoc admin_account(TEMP_ADMIN_ACCOUNT_NAME, TEMP_ADMIN_ACCOUNT_PASSWORD);

    admin_account.privilege_level = AdminLevelEnum::NO_ADMIN_ACCESS;

    admin_account.setIntoCollection();

    admin_chat_commands::GetSingleMessageRequest request;
    admin_chat_commands::GetSingleMessageResponse returned_response;

    setupAdminLoginInfo(
            request.mutable_login_info(),
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    request.set_chat_room_id(chat_room_id);
    request.set_message_uuid(message_uuid);
    request.set_timestamp_of_message(message_timestamp_stored.count());

    getSingleMessage(&request, &returned_response);

    ChatMessageToClient empty_message;

    EXPECT_FALSE(returned_response.successful());
    EXPECT_FALSE(returned_response.error_message().empty());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            returned_response.message(),
            empty_message
    );

    if(!equivalent) {
        std::cout << "returned_response.message()\n" << returned_response.message().DebugString() << '\n';
        std::cout << "extracted_message\n" << empty_message.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}

TEST_F(AdminChatRoomCommandsTests, adminNotFound) {
    admin_chat_commands::GetSingleMessageRequest request;
    admin_chat_commands::GetSingleMessageResponse returned_response;

    setupAdminLoginInfo(
            request.mutable_login_info(),
            "does_not_exist",
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    request.set_chat_room_id(chat_room_id);
    request.set_message_uuid(message_uuid);
    request.set_timestamp_of_message(-1);

    getSingleMessage(&request, &returned_response);

    ChatMessageToClient empty_message;

    EXPECT_FALSE(returned_response.successful());
    EXPECT_FALSE(returned_response.error_message().empty());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            returned_response.message(),
            empty_message
    );

    if(!equivalent) {
        std::cout << "returned_response.message()\n" << returned_response.message().DebugString() << '\n';
        std::cout << "extracted_message\n" << empty_message.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}
