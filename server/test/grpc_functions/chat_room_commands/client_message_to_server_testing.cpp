//
// Created by jeremiah on 7/31/22.
//

#include <utility_general_functions.h>
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
#include "generate_random_messages.h"
#include "connection_pool_global_variable.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>
#include "utility_chat_functions.h"

#include "accepted_mime_types.h"
#include "build_match_made_chat_room.h"
#include "report_values.h"
#include "grpc_mock_stream/mock_stream.h"
#include "setup_client_message_to_server.h"
#include "generate_randoms.h"
#include "add_reply_to_request.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ClientMessageToServerTests : public ::testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp{-1};

    bsoncxx::oid generated_account_oid;

    UserAccountDoc user_account;

    ChatRoomHeaderDoc original_chat_room_header;

    std::string chat_room_id;
    std::string chat_room_password;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(generated_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                generated_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();
        chat_room_password = create_chat_room_response.chat_room_password();

        original_chat_room_header.getFromCollection(chat_room_id);

        //sleep to guarantee current_timestamp is after the last activity time of the chat room
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

        current_timestamp = getCurrentTimestamp();

        //need the copy with chat room added here
        user_account.getFromCollection(generated_account_oid);

        ASSERT_EQ(user_account.chat_rooms.size(), 1);
    }

    ChatRoomMessageDoc compareClientMessageToServerMessageSent(
            const std::string& message_uuid,
            const grpc_chat_commands::ClientMessageToServerRequest& request,
            const grpc_chat_commands::ClientMessageToServerResponse& response,
            const bool set_chat_room_last_active_time = true
            ) {

        UserAccountDoc extracted_user_account(generated_account_oid);

        EXPECT_EQ(extracted_user_account.chat_rooms.size(), 1);
        if(!extracted_user_account.chat_rooms.empty()) {
            user_account.chat_rooms.front().last_time_viewed = extracted_user_account.chat_rooms.front().last_time_viewed;
            EXPECT_EQ(extracted_user_account, user_account);
        }

        if(set_chat_room_last_active_time)
            original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp_stored()}};

        EXPECT_EQ(original_chat_room_header.accounts_in_chat_room.size(), 1);
        if(!original_chat_room_header.accounts_in_chat_room.empty()) {
            original_chat_room_header.accounts_in_chat_room.front().last_activity_time =
                    bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp_stored()}};
        }

        ChatRoomHeaderDoc extracted_original_chat_room_header(chat_room_id);

        EXPECT_EQ(extracted_original_chat_room_header, original_chat_room_header);

        mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

        ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

        bsoncxx::builder::stream::document message_builder_doc;
        message_doc.convertToDocument(message_builder_doc);

        ChatMessageToClient response_msg;

        ExtractUserInfoObjects extractUserInfoObjects(
                mongo_cpp_client, accounts_db, user_accounts_collection
        );

        bool return_val = convertChatMessageDocumentToChatMessageToClient(
                message_builder_doc,
                chat_room_id,
                generated_account_oid.to_string(),
                false,
                &response_msg,
                AmountOfMessage::COMPLETE_MESSAGE_INFO,
                DifferentUserJoinedChatRoomAmount::SKELETON,
                false,
                &extractUserInfoObjects
        );

        response_msg.set_return_status(ReturnStatus::SUCCESS);

        EXPECT_TRUE(return_val);

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                response_msg.message(),
                request.message()
        );

        EXPECT_EQ(response_msg.message_uuid(), request.message_uuid());

        if(!equivalent) {
            std::cout << "response_msg\n" << response_msg.message().DebugString() << '\n';
            std::cout << "request\n" << request.message().DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);

        return message_doc;
    }

    std::string setUpSecondChatRoomForInvite(
        grpc_chat_commands::CreateChatRoomRequest& create_chat_room_request,
        grpc_chat_commands::CreateChatRoomResponse& create_chat_room_response,
        bsoncxx::oid& new_generated_account_oid,
        const UserAccountDoc& new_user_account
    ) {

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                generated_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        create_chat_room_request.set_chat_room_name("name");

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        std::string invite_to_chat_room_id = create_chat_room_response.chat_room_id();

        grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

        setupUserLoginInfo(
                join_chat_room_request.mutable_login_info(),
                new_generated_account_oid,
                new_user_account.logged_in_token,
                new_user_account.installation_ids.front()
        );

        join_chat_room_request.set_chat_room_id(chat_room_id);
        join_chat_room_request.set_chat_room_password(chat_room_password);

        grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

        joinChatRoom(&join_chat_room_request, &mock_server_writer);

        EXPECT_EQ(mock_server_writer.write_params.size(), 2);

        if(mock_server_writer.write_params.size() >= 2) {
            EXPECT_EQ(mock_server_writer.write_params.front().msg.messages_list_size(), 1);
            if(mock_server_writer.write_params.front().msg.messages_list_size() >= 1) {
                EXPECT_EQ(mock_server_writer.write_params.front().msg.messages_list()[0].primer(), true);
                EXPECT_EQ(mock_server_writer.write_params.front().msg.messages_list()[0].return_status(),
                          ReturnStatus::SUCCESS);
            }
        }

        ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
        original_chat_room_header.chat_room_last_active_time = extracted_chat_room_header.chat_room_last_active_time;

        return invite_to_chat_room_id;
    }

    void compareClientMessageToServerMessageSentForInvite(
            const std::string& message_uuid,
            const grpc_chat_commands::ClientMessageToServerRequest& request,
            const grpc_chat_commands::ClientMessageToServerResponse& response,
            bsoncxx::oid& new_generated_account_oid,
            const std::string& invite_to_chat_room_id,
            const UserAccountDoc& new_user_account,
            const ChatRoomHeaderDoc& new_chat_room_header
            ) {

        UserAccountDoc extracted_user_account(generated_account_oid);

        EXPECT_EQ(extracted_user_account.chat_rooms.size(), 2);
        if(extracted_user_account.chat_rooms.size() >= 2) {
            user_account.chat_rooms.front().last_time_viewed = extracted_user_account.chat_rooms.front().last_time_viewed;

            //second chat room is irrelevant to this test
            user_account.chat_rooms.emplace_back(
                    extracted_user_account.chat_rooms.back()
            );

            EXPECT_EQ(extracted_user_account, user_account);
        }

        EXPECT_EQ(original_chat_room_header.accounts_in_chat_room.size(), 1);
        if(!original_chat_room_header.accounts_in_chat_room.empty()) {
            original_chat_room_header.accounts_in_chat_room.front().last_activity_time =
                    bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp_stored()}};
        }

        ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);

        EXPECT_EQ(extracted_chat_room_header.accounts_in_chat_room.size(), 2);
        if(extracted_chat_room_header.accounts_in_chat_room.size() >= 2) {
            original_chat_room_header.accounts_in_chat_room.emplace_back(
                    extracted_chat_room_header.accounts_in_chat_room.back()
            );
        }

        EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

        //make sure new user did not change
        UserAccountDoc extracted_new_user_account(new_generated_account_oid);
        EXPECT_EQ(new_user_account, extracted_new_user_account);

        //make sure new chat room did not change
        ChatRoomHeaderDoc extracted_new_chat_room_header(invite_to_chat_room_id);
        EXPECT_EQ(new_chat_room_header, extracted_new_chat_room_header);

        mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

        ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

        bsoncxx::builder::stream::document message_builder_doc;
        message_doc.convertToDocument(message_builder_doc);

        ChatMessageToClient response_msg;

        ExtractUserInfoObjects extractUserInfoObjects(
                mongo_cpp_client, accounts_db, user_accounts_collection
        );

        bool return_val = convertChatMessageDocumentToChatMessageToClient(
                message_builder_doc,
                chat_room_id,
                generated_account_oid.to_string(),
                false,
                &response_msg,
                AmountOfMessage::COMPLETE_MESSAGE_INFO,
                DifferentUserJoinedChatRoomAmount::SKELETON,
                false,
                &extractUserInfoObjects
        );

        response_msg.set_return_status(ReturnStatus::SUCCESS);

        EXPECT_TRUE(return_val);

        //forcing this to match the request
        response_msg.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                response_msg.message(),
                request.message()
        );

        EXPECT_EQ(response_msg.message_uuid(), request.message_uuid());

        if(!equivalent) {
            std::cout << "response_msg\n" << response_msg.message().DebugString() << '\n';
            std::cout << "request\n" << request.message().DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(ClientMessageToServerTests, kTextMessage) {

    std::string message_uuid = generateUUID();

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            )
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

    compareClientMessageToServerMessageSent(
        message_uuid,
        text_message_request,
        text_message_response
    );
}

TEST_F(ClientMessageToServerTests, kTextMessage_trimTrailingWhitespace) {

    std::string message_uuid = generateUUID();

    const std::string text_message = gen_random_alpha_numeric_string(
            rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
    );

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid,
            text_message + "   \n"
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

    text_message_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(text_message);
    compareClientMessageToServerMessageSent(
            message_uuid,
            text_message_request,
            text_message_response
    );
}

TEST_F(ClientMessageToServerTests, kPictureMessage) {

    std::string message_uuid = generateUUID();
    std::string picture_in_bytes = gen_random_alpha_numeric_string(
            rand() % 1000 + 1000
    );
    auto [request, response] = generateRandomPictureMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid,
            picture_in_bytes
    );

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.user_not_in_chat_room(), false);
    EXPECT_GT(response.timestamp_stored(), 0);

    //picture was moved out, need to set it here
    request.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_picture_file_in_bytes(picture_in_bytes);

    ChatRoomMessageDoc message_doc = compareClientMessageToServerMessageSent(
            message_uuid,
            request,
            response
    );

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::PictureMessageSpecifics>(message_doc.message_specifics_document);

    EXPECT_EQ(response.picture_oid(), message->picture_oid);
}

TEST_F(ClientMessageToServerTests, kPictureMessage_messageAlreadyExisted) {

    std::string message_uuid = generateUUID();
    std::string picture_in_bytes = gen_random_alpha_numeric_string(
            rand() % 1000 + 1000
    );
    auto [request, response] = generateRandomPictureMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid,
            picture_in_bytes
    );

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.user_not_in_chat_room(), false);
    EXPECT_GT(response.timestamp_stored(), 0);

    //picture was moved out, need to set it here
    request.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_picture_file_in_bytes(picture_in_bytes);

    ChatRoomMessageDoc message_doc = compareClientMessageToServerMessageSent(
            message_uuid,
            request,
            response
    );

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::PictureMessageSpecifics>(message_doc.message_specifics_document);

    EXPECT_EQ(response.picture_oid(), message->picture_oid);

    //Second duplicate picture
    picture_in_bytes = gen_random_alpha_numeric_string(
            rand() % 1000 + 1000
    );

    auto [second_request, second_response] = generateRandomPictureMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid,
            picture_in_bytes
    );

    EXPECT_EQ(second_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(second_response.user_not_in_chat_room(), false);
    EXPECT_EQ(second_response.picture_oid(), response.picture_oid()); //already stored picture oid should be returned
    EXPECT_GT(second_response.timestamp_stored(), 0);

    //picture was moved out, need to set it here
    second_request.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_picture_file_in_bytes(picture_in_bytes);

    //compare with first request again, the message should not have changed
    compareClientMessageToServerMessageSent(
            message_uuid,
            request,
            response
    );
}

TEST_F(ClientMessageToServerTests, kLocationMessage) {

    std::string message_uuid = generateUUID();

    auto [request, response] = generateRandomLocationMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid
    );

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.user_not_in_chat_room(), false);
    EXPECT_GT(response.timestamp_stored(), 0);
    EXPECT_TRUE(response.picture_oid().empty());

    request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

    compareClientMessageToServerMessageSent(
            message_uuid,
            request,
            response
    );
}

TEST_F(ClientMessageToServerTests, kMimeTypeMessage) {

    std::string message_uuid = generateUUID();

    auto [request, response] = generateRandomMimeTypeMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid
    );

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.user_not_in_chat_room(), false);
    EXPECT_GT(response.timestamp_stored(), 0);
    EXPECT_TRUE(response.picture_oid().empty());

    request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

    compareClientMessageToServerMessageSent(
            message_uuid,
            request,
            response
    );
}

TEST_F(ClientMessageToServerTests, kInviteMessage) {

    bsoncxx::oid new_generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc new_user_account(new_generated_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    const std::string invite_to_chat_room_id = setUpSecondChatRoomForInvite(
            create_chat_room_request,
            create_chat_room_response,
            new_generated_account_oid,
            new_user_account
    );

    ChatRoomHeaderDoc new_chat_room_header(invite_to_chat_room_id);

    //extract user with updated info
    new_user_account.getFromCollection(new_generated_account_oid);

    //sleep to make sure no timestamp conflicts with last active times
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    std::string message_uuid = generateUUID();

    auto [request, response] = generateRandomInviteTypeMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid,
            new_generated_account_oid.to_string(),
            new_user_account.first_name,
            create_chat_room_response.chat_room_id(),
            create_chat_room_request.chat_room_name(),
            create_chat_room_response.chat_room_password()
    );

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.user_not_in_chat_room(), false);
    EXPECT_GT(response.timestamp_stored(), 0);
    EXPECT_TRUE(response.picture_oid().empty());

    compareClientMessageToServerMessageSentForInvite(
        message_uuid,
        request,
        response,
        new_generated_account_oid,
        invite_to_chat_room_id,
        new_user_account,
        new_chat_room_header
    );
}

TEST_F(ClientMessageToServerTests, kInviteMessage_invalidChatRoomInfo) {

    bsoncxx::oid new_generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc new_user_account(new_generated_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    const std::string invite_to_chat_room_id = setUpSecondChatRoomForInvite(
            create_chat_room_request,
            create_chat_room_response,
            new_generated_account_oid,
            new_user_account
    );

    ChatRoomHeaderDoc new_chat_room_header(invite_to_chat_room_id);

    //extract user with updated info
    new_user_account.getFromCollection(new_generated_account_oid);

    //sleep to make sure no timestamp conflicts with last active times
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    std::string message_uuid = generateUUID();

    auto [request, response] = generateRandomInviteTypeMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid,
            new_generated_account_oid.to_string(),
            new_user_account.first_name,
            create_chat_room_response.chat_room_id(),
            create_chat_room_request.chat_room_name(),
            create_chat_room_response.chat_room_password() + "a" //invalid password
    );

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.user_not_in_chat_room(), false);
    EXPECT_GT(response.timestamp_stored(), 0);
    EXPECT_TRUE(response.picture_oid().empty());

    UserAccountDoc extracted_user_account(generated_account_oid);

    EXPECT_EQ(extracted_user_account.chat_rooms.size(), 2);
    if(extracted_user_account.chat_rooms.size() >= 2) {
        user_account.chat_rooms.front().last_time_viewed = extracted_user_account.chat_rooms.front().last_time_viewed;

        //second chat room is irrelevant to this test
        user_account.chat_rooms.emplace_back(
                extracted_user_account.chat_rooms.back()
        );

        EXPECT_EQ(extracted_user_account, user_account);
    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_chat_room_header.accounts_in_chat_room.size(), 2);
    if(extracted_chat_room_header.accounts_in_chat_room.size() >= 2) {
        original_chat_room_header.accounts_in_chat_room.emplace_back(
                extracted_chat_room_header.accounts_in_chat_room.back()
        );
    }

    EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

    //make sure new user did not change
    UserAccountDoc extracted_new_user_account(new_generated_account_oid);
    EXPECT_EQ(new_user_account, extracted_new_user_account);

    //make sure new chat room did not change
    ChatRoomHeaderDoc extracted_new_chat_room_header(invite_to_chat_room_id);
    EXPECT_EQ(new_chat_room_header, extracted_new_chat_room_header);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());

}

TEST_F(ClientMessageToServerTests, kEditedMessage_sentByCurrentUser) {

    std::string text_message_uuid = generateUUID();
    std::string edited_message_uuid = generateUUID();

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string edited_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            original_message_text
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);
    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};


    auto [edited_message_request, edited_message_response] = generateRandomEditedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            edited_message_uuid,
            text_message_uuid,
            edited_message_text
    );

    EXPECT_EQ(edited_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(edited_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(edited_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(edited_message_response.picture_oid().empty());

    compareClientMessageToServerMessageSent(
            edited_message_uuid,
            edited_message_request,
            edited_message_response,
            false
    );

    ChatRoomMessageDoc edited_message_doc(edited_message_uuid, chat_room_id);

    auto edited_message_specifics = std::static_pointer_cast<ChatRoomMessageDoc::EditedMessageSpecifics>(edited_message_doc.message_specifics_document);

    EXPECT_EQ(edited_message_specifics->edited_previous_message_text, original_message_text);

    ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(message_doc.message_specifics_document);

    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_type_key, DeleteType::DELETE_TYPE_NOT_SET);
    EXPECT_TRUE(message->active_message_info.chat_room_message_deleted_accounts.empty());
    EXPECT_EQ(message->text_message, edited_message_text);
    EXPECT_EQ(message->text_is_edited, true);
    EXPECT_EQ(message->text_edited_time, edited_message_response.timestamp_stored());

}

TEST_F(ClientMessageToServerTests, kEditedMessage_notSentByCurrentUser) {

    std::string text_message_uuid = generateUUID();
    std::string edited_message_uuid = generateUUID();

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string edited_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    const bsoncxx::oid other_user_oid{};

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            original_message_text
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //make sure message is sent by a different user (note that the user was never in the chat room)
    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);
    text_message_doc.message_sent_by = other_user_oid;
    text_message_doc.setIntoCollection();

    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};

    auto [edited_message_request, edited_message_response] = generateRandomEditedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            edited_message_uuid,
            text_message_uuid,
            edited_message_text
    );

    EXPECT_EQ(edited_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(edited_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(edited_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(edited_message_response.picture_oid().empty());

    ChatRoomMessageDoc edited_message_doc(edited_message_uuid, chat_room_id);

    EXPECT_TRUE(edited_message_doc.id.empty());

    ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(message_doc.message_specifics_document);

    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_type_key, DeleteType::DELETE_TYPE_NOT_SET);
    EXPECT_TRUE(message->active_message_info.chat_room_message_deleted_accounts.empty());
    EXPECT_EQ(message->text_message, original_message_text);
    EXPECT_EQ(message->text_is_edited, false);

}

TEST_F(ClientMessageToServerTests, kEditedMessage_messageNotFoundToEdit) {
    std::string text_message_uuid = generateUUID();
    std::string edited_message_uuid = generateUUID();

    const std::string edited_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [edited_message_request, edited_message_response] = generateRandomEditedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            edited_message_uuid,
            text_message_uuid,
            edited_message_text
    );

    EXPECT_EQ(edited_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(edited_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(edited_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(edited_message_response.picture_oid().empty());

    UserAccountDoc extracted_user_account(generated_account_oid);

    EXPECT_EQ(extracted_user_account.chat_rooms.size(), 1);
    if(!extracted_user_account.chat_rooms.empty()) {
        user_account.chat_rooms.front().last_time_viewed = extracted_user_account.chat_rooms.front().last_time_viewed;
        EXPECT_EQ(extracted_user_account, user_account);
    }

    ChatRoomHeaderDoc extracted_original_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_original_chat_room_header, original_chat_room_header);

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    ChatRoomMessageDoc edited_message_doc(edited_message_uuid, chat_room_id);

    EXPECT_TRUE(edited_message_doc.id.empty());

    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);

    EXPECT_TRUE(text_message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, kEditedMessage_duplicateMessageFound) {

    std::string text_message_uuid = generateUUID();
    std::string edited_message_uuid = generateUUID();

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            )
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);
    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};

    const std::string edited_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [edited_message_request, edited_message_response] = generateRandomEditedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            edited_message_uuid,
            text_message_uuid,
            edited_message_text
    );

    EXPECT_EQ(edited_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(edited_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(edited_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(edited_message_response.picture_oid().empty());

    const std::string second_edited_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [second_edited_message_request, second_edited_message_response] = generateRandomEditedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            edited_message_uuid,
            text_message_uuid,
            second_edited_message_text
    );

    EXPECT_EQ(second_edited_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(second_edited_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(second_edited_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(second_edited_message_response.picture_oid().empty());

    ChatRoomMessageDoc edited_message_doc(edited_message_uuid, chat_room_id);

    auto edited_message_specifics = std::static_pointer_cast<ChatRoomMessageDoc::EditedMessageSpecifics>(edited_message_doc.message_specifics_document);

    //make sure old uuid was not updated
    EXPECT_EQ(edited_message_specifics->edited_new_message_text, edited_message_text);

    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);

    auto text_message_specifics = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(text_message_doc.message_specifics_document);

    //make sure message was not updated
    EXPECT_EQ(text_message_specifics->text_message, edited_message_text);
    EXPECT_EQ(text_message_specifics->text_is_edited, true);
    EXPECT_EQ(text_message_specifics->text_edited_time, edited_message_response.timestamp_stored());

}

TEST_F(ClientMessageToServerTests, kDeletedMessage_deletedForSingle_userAdminAndSentMessage) {

    std::string text_message_uuid = generateUUID();
    std::string delete_message_uuid = generateUUID();
    const DeleteType delete_type = DeleteType::DELETE_FOR_SINGLE_USER;

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            original_message_text
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);
    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(delete_message_response.picture_oid().empty());

    delete_message_request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

    compareClientMessageToServerMessageSent(
            delete_message_uuid,
            delete_message_request,
            delete_message_response,
            false
    );

    ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(message_doc.message_specifics_document);

    EXPECT_EQ(message->text_is_edited, false);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_type_key, delete_type);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_accounts.size(), 1);
    if(!message->active_message_info.chat_room_message_deleted_accounts.empty()) {
        EXPECT_EQ(message->active_message_info.chat_room_message_deleted_accounts[0], generated_account_oid.to_string());
    }

}

//Deleting for a single user can happen if user is not admin and did not send the message (it is a personal delete only). So
// this test should still properly delete the message.
TEST_F(ClientMessageToServerTests, kDeletedMessage_deletedForSingle_userNotAdminAndDidNotSendMessage) {

    std::string text_message_uuid = generateUUID();
    std::string delete_message_uuid = generateUUID();

    bsoncxx::oid other_user_oid{};
    const DeleteType delete_type = DeleteType::DELETE_FOR_SINGLE_USER;

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            original_message_text
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //make sure message is sent by a different user (note that the user was never in the chat room)
    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);
    text_message_doc.message_sent_by = other_user_oid;
    text_message_doc.setIntoCollection();

    //make sure user is not admin (note that this does break the chat room a bit, it now has no admin)
    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};
    original_chat_room_header.accounts_in_chat_room.front().state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;
    original_chat_room_header.setIntoCollection();

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(delete_message_response.picture_oid().empty());

    delete_message_request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

    compareClientMessageToServerMessageSent(
            delete_message_uuid,
            delete_message_request,
            delete_message_response,
            false
    );

    text_message_doc.getFromCollection(text_message_uuid, chat_room_id);

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(text_message_doc.message_specifics_document);

    EXPECT_EQ(message->text_is_edited, false);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_type_key, delete_type);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_accounts.size(), 1);
    if(!message->active_message_info.chat_room_message_deleted_accounts.empty()) {
        EXPECT_EQ(message->active_message_info.chat_room_message_deleted_accounts[0], generated_account_oid.to_string());
    }
}

TEST_F(ClientMessageToServerTests, kDeletedMessage_deletedForSingle_messageNotFoundToDelete) {

    std::string text_message_uuid = generateUUID();
    std::string delete_message_uuid = generateUUID();

    const bsoncxx::oid other_user_oid{};
    const DeleteType delete_type = DeleteType::DELETE_FOR_SINGLE_USER;

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(delete_message_response.picture_oid().empty());

    ChatRoomHeaderDoc extracted_original_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_original_chat_room_header, original_chat_room_header);

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    ChatRoomMessageDoc edited_message_doc(delete_message_uuid, chat_room_id);

    EXPECT_TRUE(edited_message_doc.id.empty());

    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);

    EXPECT_TRUE(text_message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, kDeletedMessage_deletedForSingle_duplicateDeletedMessage) {

    std::string text_message_uuid = generateUUID();
    std::string delete_message_uuid = generateUUID();

    const bsoncxx::oid other_user_oid{};
    const DeleteType delete_type = DeleteType::DELETE_FOR_SINGLE_USER;

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            original_message_text
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //make sure message is sent by a different user (note that the user was never in the chat room)
    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);
    text_message_doc.message_sent_by = other_user_oid;
    text_message_doc.setIntoCollection();

    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(delete_message_response.picture_oid().empty());

    auto [second_delete_message_request, second_delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(second_delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(second_delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(second_delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(second_delete_message_response.picture_oid().empty());

    second_delete_message_request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

    compareClientMessageToServerMessageSent(
            delete_message_uuid,
            second_delete_message_request,
            second_delete_message_response,
            false
    );

    text_message_doc.getFromCollection(text_message_uuid, chat_room_id);

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(text_message_doc.message_specifics_document);

    EXPECT_EQ(message->text_is_edited, false);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_type_key, delete_type);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_accounts.size(), 1);
    if(!message->active_message_info.chat_room_message_deleted_accounts.empty()) {
        EXPECT_EQ(message->active_message_info.chat_room_message_deleted_accounts[0], generated_account_oid.to_string());
    }
}

//Should delete message properly.
TEST_F(ClientMessageToServerTests, kDeletedMessage_deletedForAll_userIsAdminAndDidNotSendMessage) {

    std::string text_message_uuid = generateUUID();
    std::string delete_message_uuid = generateUUID();

    const bsoncxx::oid other_user_oid{};
    const DeleteType delete_type = DeleteType::DELETE_FOR_ALL_USERS;

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            original_message_text
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //make sure message is sent by a different user (note that the user was never in the chat room)
    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);
    text_message_doc.message_sent_by = other_user_oid;
    text_message_doc.setIntoCollection();

    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(delete_message_response.picture_oid().empty());

    delete_message_request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

    compareClientMessageToServerMessageSent(
            delete_message_uuid,
            delete_message_request,
            delete_message_response,
            false
    );

    text_message_doc.getFromCollection(text_message_uuid, chat_room_id);

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(text_message_doc.message_specifics_document);

    EXPECT_EQ(message->text_is_edited, false);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_type_key, delete_type);
    EXPECT_TRUE(message->active_message_info.chat_room_message_deleted_accounts.empty());
}

//Should delete message properly.
TEST_F(ClientMessageToServerTests, kDeletedMessage_deletedForAll_userIsNotAdminAndDidSendMessage) {

    std::string text_message_uuid = generateUUID();
    std::string delete_message_uuid = generateUUID();

    const bsoncxx::oid other_user_oid{};
    const DeleteType delete_type = DeleteType::DELETE_FOR_ALL_USERS;

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            original_message_text
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //make sure user is not admin (note that this does break the chat room a bit, it now has no admin)
    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};
    original_chat_room_header.accounts_in_chat_room.front().state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;
    original_chat_room_header.setIntoCollection();

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(delete_message_response.picture_oid().empty());

    delete_message_request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

    compareClientMessageToServerMessageSent(
            delete_message_uuid,
            delete_message_request,
            delete_message_response,
            false
    );

    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(text_message_doc.message_specifics_document);

    EXPECT_EQ(message->text_is_edited, false);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_type_key, delete_type);
    EXPECT_TRUE(message->active_message_info.chat_room_message_deleted_accounts.empty());
}

//Should fail to delete message.
TEST_F(ClientMessageToServerTests, kDeletedMessage_deletedForAll_userIsNotAdminAndDidNotSendMessage) {
    std::string text_message_uuid = generateUUID();
    std::string delete_message_uuid = generateUUID();

    const bsoncxx::oid other_user_oid{};
    const DeleteType delete_type = DeleteType::DELETE_FOR_ALL_USERS;

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            original_message_text
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //make sure message is sent by a different user (note that the user was never in the chat room)
    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);
    text_message_doc.message_sent_by = other_user_oid;
    text_message_doc.setIntoCollection();

    //make sure user is not admin (note that this does break the chat room a bit, it now has no admin)
    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};
    original_chat_room_header.accounts_in_chat_room.front().state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;
    original_chat_room_header.setIntoCollection();

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(delete_message_response.picture_oid().empty());

    ChatRoomMessageDoc deleted_message_doc(delete_message_uuid, chat_room_id);

    //message should not exist
    EXPECT_TRUE(deleted_message_doc.id.empty());

    delete_message_request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

    text_message_doc.getFromCollection(text_message_uuid, chat_room_id);

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(text_message_doc.message_specifics_document);

    EXPECT_EQ(message->text_is_edited, false);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_type_key, DeleteType::DELETE_TYPE_NOT_SET);
    EXPECT_TRUE(message->active_message_info.chat_room_message_deleted_accounts.empty());
}

TEST_F(ClientMessageToServerTests, kDeletedMessage_deletedForAll_messageNotFoundToDelete) {

    std::string text_message_uuid = generateUUID();
    std::string delete_message_uuid = generateUUID();

    const bsoncxx::oid other_user_oid{};
    const DeleteType delete_type = DeleteType::DELETE_FOR_ALL_USERS;

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(delete_message_response.picture_oid().empty());

    ChatRoomHeaderDoc extracted_original_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_original_chat_room_header, original_chat_room_header);

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    ChatRoomMessageDoc edited_message_doc(delete_message_uuid, chat_room_id);

    EXPECT_TRUE(edited_message_doc.id.empty());

    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);

    EXPECT_TRUE(text_message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, kDeletedMessage_deletedForAll_duplicateDeletedMessage) {

    std::string text_message_uuid = generateUUID();
    std::string delete_message_uuid = generateUUID();

    const bsoncxx::oid other_user_oid{};
    const DeleteType delete_type = DeleteType::DELETE_FOR_ALL_USERS;

    const std::string original_message_text = gen_random_alpha_numeric_string(rand() % 100 + 5);

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            original_message_text
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //make sure message is sent by a different user (note that the user was never in the chat room)
    ChatRoomMessageDoc text_message_doc(text_message_uuid, chat_room_id);
    text_message_doc.message_sent_by = other_user_oid;
    text_message_doc.setIntoCollection();

    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(delete_message_response.picture_oid().empty());

    auto [second_delete_message_request, second_delete_message_response] = generateRandomDeletedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            delete_type
    );

    EXPECT_EQ(second_delete_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(second_delete_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(second_delete_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(second_delete_message_response.picture_oid().empty());

    second_delete_message_request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

    compareClientMessageToServerMessageSent(
            delete_message_uuid,
            second_delete_message_request,
            second_delete_message_response,
            false
    );

    text_message_doc.getFromCollection(text_message_uuid, chat_room_id);

    auto message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(text_message_doc.message_specifics_document);

    EXPECT_EQ(message->text_is_edited, false);
    EXPECT_EQ(message->active_message_info.chat_room_message_deleted_type_key, delete_type);
    EXPECT_TRUE(message->active_message_info.chat_room_message_deleted_accounts.empty());
}

TEST_F(ClientMessageToServerTests, kUpdateObservedTimeMessage) {

    std::string message_uuid = generateUUID();

    auto [observed_message_request, observed_message_response] = generateRandomUpdateObservedTimeMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid
    );

    EXPECT_EQ(observed_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(observed_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(observed_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(observed_message_response.picture_oid().empty());

    UserAccountDoc extracted_user_account(generated_account_oid);

    EXPECT_EQ(extracted_user_account.chat_rooms.size(), 1);
    if(!extracted_user_account.chat_rooms.empty()) {
        user_account.chat_rooms.front().last_time_viewed = bsoncxx::types::b_date{std::chrono::milliseconds{observed_message_response.timestamp_stored()}};
        EXPECT_EQ(extracted_user_account, user_account);
    }

    ChatRoomHeaderDoc extracted_original_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_original_chat_room_header, original_chat_room_header);

}

TEST_F(ClientMessageToServerTests, kUserActivityDetectedMessage) {

    std::string message_uuid = generateUUID();

    auto [request, response] = generateRandomUserActivityDetectedMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            message_uuid
    );

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.user_not_in_chat_room(), false);
    EXPECT_GT(response.timestamp_stored(), 0);
    EXPECT_TRUE(response.picture_oid().empty());

    request.clear_message_uuid();
    request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    compareClientMessageToServerMessageSent(
            message_uuid,
            request,
            response,
            false
    );
}

TEST_F(ClientMessageToServerTests, kTextReply) {

    std::string first_message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    auto [first_text_message_request, first_text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            first_message_uuid,
            first_text_message
    );

    EXPECT_EQ(first_text_message_response.return_status(), ReturnStatus::SUCCESS);

    std::string reply_message_uuid = generateUUID();

    auto [reply_text_message_request, reply_text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = reply_text_message_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addTextReplyToRequest(
            active_message_info,
            generated_account_oid,
            first_message_uuid,
            first_text_message
    );

    clientMessageToServer(&reply_text_message_request, &reply_text_message_response);

    EXPECT_EQ(reply_text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(reply_text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(reply_text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(reply_text_message_response.picture_oid().empty());

    compareClientMessageToServerMessageSent(
            reply_message_uuid,
            reply_text_message_request,
            reply_text_message_response
    );
}

TEST_F(ClientMessageToServerTests, kPictureReply) {

    std::string picture_message_uuid = generateUUID();
    std::string picture_in_bytes = gen_random_alpha_numeric_string(
            rand() % 1000 + 1000
    );

    auto [picture_message_request, picture_message_response] = generateRandomPictureMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            picture_message_uuid,
            picture_in_bytes
    );

    EXPECT_EQ(picture_message_response.return_status(), ReturnStatus::SUCCESS);

    std::string reply_message_uuid = generateUUID();

    auto [reply_text_message_request, reply_text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = reply_text_message_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    const std::string thumbnail_str = gen_random_alpha_numeric_string(
            rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
    );

    addPictureReplyToRequest(
            active_message_info,
            generated_account_oid,
            picture_message_uuid,
            thumbnail_str
    );

    clientMessageToServer(&reply_text_message_request, &reply_text_message_response);

    EXPECT_EQ(reply_text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(reply_text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(reply_text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(reply_text_message_response.picture_oid().empty());

    //thumbnail was moved out, it must be reset
    active_message_info->mutable_reply_info()->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_in_bytes(thumbnail_str);

    compareClientMessageToServerMessageSent(
            reply_message_uuid,
            reply_text_message_request,
            reply_text_message_response
    );

}

TEST_F(ClientMessageToServerTests, kMimeReply) {

    std::string mime_type_message_uuid = generateUUID();

    auto [mime_type_request, mime_type_response] = generateRandomMimeTypeMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            mime_type_message_uuid
    );

    EXPECT_EQ(mime_type_response.return_status(), ReturnStatus::SUCCESS);

    std::string reply_message_uuid = generateUUID();

    auto [reply_text_message_request, reply_text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = reply_text_message_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    const std::string thumbnail_str = gen_random_alpha_numeric_string(
            rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
    );

    addMimeTypeReplyToRequest(
            active_message_info,
            generated_account_oid,
            mime_type_message_uuid,
            thumbnail_str,
            accepted_mime_types.empty() ? "" : *accepted_mime_types.begin()
    );

    clientMessageToServer(&reply_text_message_request, &reply_text_message_response);

    EXPECT_EQ(reply_text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(reply_text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(reply_text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(reply_text_message_response.picture_oid().empty());

    //thumbnail was moved out, it must be reset
    active_message_info->mutable_reply_info()->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_in_bytes(thumbnail_str);

    compareClientMessageToServerMessageSent(
            reply_message_uuid,
            reply_text_message_request,
            reply_text_message_response
    );
}

TEST_F(ClientMessageToServerTests, kLocationReply) {

    std::string location_message_uuid = generateUUID();

    auto [location_message_request, location_message_response] = generateRandomLocationMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            location_message_uuid
    );

    EXPECT_EQ(location_message_response.return_status(), ReturnStatus::SUCCESS);

    std::string reply_message_uuid = generateUUID();

    auto [reply_text_message_request, reply_text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = reply_text_message_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addLocationReplyToRequest(
        active_message_info,
        generated_account_oid,
        location_message_uuid
    );

    clientMessageToServer(&reply_text_message_request, &reply_text_message_response);

    EXPECT_EQ(reply_text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(reply_text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(reply_text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(reply_text_message_response.picture_oid().empty());

    compareClientMessageToServerMessageSent(
            reply_message_uuid,
            reply_text_message_request,
            reply_text_message_response
    );
}

TEST_F(ClientMessageToServerTests, kInviteReply) {

    bsoncxx::oid new_generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc new_user_account(new_generated_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    const std::string invite_to_chat_room_id = setUpSecondChatRoomForInvite(
            create_chat_room_request,
            create_chat_room_response,
            new_generated_account_oid,
            new_user_account
    );

    ChatRoomHeaderDoc new_chat_room_header(invite_to_chat_room_id);

    //extract user with updated info
    new_user_account.getFromCollection(new_generated_account_oid);

    //sleep to make sure no timestamp conflicts with last active times
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    std::string invite_message_uuid = generateUUID();

    auto [invite_message_request, invite_message_response] = generateRandomInviteTypeMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            invite_message_uuid,
            new_generated_account_oid.to_string(),
            new_user_account.first_name,
            create_chat_room_response.chat_room_id(),
            create_chat_room_request.chat_room_name(),
            create_chat_room_response.chat_room_password()
    );

    EXPECT_EQ(invite_message_response.return_status(), ReturnStatus::SUCCESS);

    std::string reply_message_uuid = generateUUID();

    auto [reply_text_message_request, reply_text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = reply_text_message_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addInviteReplyToRequest(
        active_message_info,
        generated_account_oid,
        invite_message_uuid
    );

    clientMessageToServer(&reply_text_message_request, &reply_text_message_response);

    EXPECT_EQ(reply_text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(reply_text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(reply_text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(reply_text_message_response.picture_oid().empty());

    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{reply_text_message_response.timestamp_stored()}};

    compareClientMessageToServerMessageSentForInvite(
            reply_message_uuid,
            reply_text_message_request,
            reply_text_message_response,
            new_generated_account_oid,
            invite_to_chat_room_id,
            new_user_account,
            new_chat_room_header
    );
}

TEST_F(ClientMessageToServerTests, repliedToMessageDoesNotExist) {

    std::string first_message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [reply_text_message_request, reply_text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = reply_text_message_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addTextReplyToRequest(
            active_message_info,
            generated_account_oid,
            first_message_uuid,
            first_text_message
    );

    clientMessageToServer(&reply_text_message_request, &reply_text_message_response);

    EXPECT_EQ(reply_text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(reply_text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(reply_text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(reply_text_message_response.picture_oid().empty());

    active_message_info->Clear();

    compareClientMessageToServerMessageSent(
            reply_message_uuid,
            reply_text_message_request,
            reply_text_message_response
    );
}

TEST_F(ClientMessageToServerTests, repliedToMessageTypeDoesNotMatch) {
    std::string first_message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    auto [first_text_message_request, first_text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            first_message_uuid,
            first_text_message
    );

    EXPECT_EQ(first_text_message_response.return_status(), ReturnStatus::SUCCESS);

    std::string reply_message_uuid = generateUUID();

    auto [reply_text_message_request, reply_text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = reply_text_message_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    const std::string thumbnail_str = gen_random_alpha_numeric_string(
            rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
    );

    addPictureReplyToRequest(
        active_message_info,
        generated_account_oid,
        first_message_uuid,
        thumbnail_str
    );

    clientMessageToServer(&reply_text_message_request, &reply_text_message_response);

    EXPECT_EQ(reply_text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(reply_text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(reply_text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(reply_text_message_response.picture_oid().empty());

    active_message_info->Clear();

    compareClientMessageToServerMessageSent(
            reply_message_uuid,
            reply_text_message_request,
            reply_text_message_response
    );
}

TEST_F(ClientMessageToServerTests, removesMatchingAccountStatus) {

    bsoncxx::oid second_generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account(second_generated_account_oid);

    current_timestamp = getCurrentTimestamp();

    const std::string matching_chat_room_id = buildMatchMadeChatRoom(
            current_timestamp,
            generated_account_oid,
            second_generated_account_oid,
            user_account,
            second_user_account
    );

    user_account.getFromCollection(generated_account_oid);
    second_user_account.getFromCollection(second_generated_account_oid);
    original_chat_room_header.getFromCollection(matching_chat_room_id);

    std::string message_uuid = generateUUID();

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
        generated_account_oid,
        user_account.logged_in_token,
        user_account.installation_ids.front(),
        matching_chat_room_id,
        message_uuid,
        gen_random_alpha_numeric_string(
                rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
        )
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), false);
    EXPECT_GT(text_message_response.timestamp_stored(), 0);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

    UserAccountDoc second_extracted_user_account(second_generated_account_oid);

    EXPECT_EQ(second_user_account.other_accounts_matched_with.size(), 1);
    if(!second_user_account.other_accounts_matched_with.empty()) {
        second_user_account.other_accounts_matched_with.pop_back();
        EXPECT_EQ(second_extracted_user_account, second_user_account);
    }

    UserAccountDoc extracted_user_account(generated_account_oid);

    EXPECT_EQ(extracted_user_account.chat_rooms.size(), 2);
    EXPECT_EQ(user_account.chat_rooms.size(), 2);
    EXPECT_EQ(user_account.other_accounts_matched_with.size(), 1);
    if(extracted_user_account.chat_rooms.size() >= 2
       && user_account.chat_rooms.size() >= 2
       && !user_account.other_accounts_matched_with.empty()
            ) {
        user_account.other_accounts_matched_with.pop_back();
        user_account.chat_rooms[1].last_time_viewed = extracted_user_account.chat_rooms[1].last_time_viewed;
        EXPECT_EQ(extracted_user_account, user_account);
    }

    original_chat_room_header.matching_oid_strings = nullptr;
    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};

    EXPECT_EQ(original_chat_room_header.accounts_in_chat_room.size(), 2);
    if(original_chat_room_header.accounts_in_chat_room.size() >= 2) {
        original_chat_room_header.accounts_in_chat_room.front().last_activity_time =
                bsoncxx::types::b_date{std::chrono::milliseconds{text_message_response.timestamp_stored()}};
    }

    ChatRoomHeaderDoc extracted_original_chat_room_header(matching_chat_room_id);

    EXPECT_EQ(extracted_original_chat_room_header, original_chat_room_header);

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + matching_chat_room_id];

    ChatRoomMessageDoc message_doc(message_uuid, matching_chat_room_id);

    bsoncxx::builder::stream::document message_builder_doc;
    message_doc.convertToDocument(message_builder_doc);

    ChatMessageToClient response_msg;

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_builder_doc,
            matching_chat_room_id,
            generated_account_oid.to_string(),
            false,
            &response_msg,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            &extractUserInfoObjects
    );

    response_msg.set_return_status(ReturnStatus::SUCCESS);

    EXPECT_TRUE(return_val);

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            response_msg.message(),
            text_message_request.message()
    );

    EXPECT_EQ(response_msg.message_uuid(), text_message_request.message_uuid());

    if(!equivalent) {
        std::cout << "response_msg\n" << response_msg.message().DebugString() << '\n';
        std::cout << "request\n" << text_message_request.message().DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}

TEST_F(ClientMessageToServerTests, userNotInChatRoom) {

    grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
    grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

    setupUserLoginInfo(
            leave_chat_room_request.mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    leave_chat_room_request.set_chat_room_id(chat_room_id);

    leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

    ASSERT_EQ(leave_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    std::string text_message_uuid = generateUUID();

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            )
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), true);
    EXPECT_EQ(text_message_response.timestamp_stored(), -1);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_message_uuid) {

    std::string message_uuid = "invalid_message_uuid";
    std::string message_text = gen_random_alpha_numeric_string((rand() % 1000) + 5);

    grpc_chat_commands::ClientMessageToServerRequest text_message_request = setupClientTextMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            message_text
    );

    grpc_chat_commands::ClientMessageToServerResponse text_message_response;

    clientMessageToServer(&text_message_request, &text_message_response);

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), false);
    EXPECT_EQ(text_message_response.timestamp_stored(), -2);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

}

TEST_F(ClientMessageToServerTests, invalid_timestamp_observed) {

    std::string message_uuid = generateUUID();
    std::string message_text = gen_random_alpha_numeric_string((rand() % 1000) + 5);

    grpc_chat_commands::ClientMessageToServerRequest text_message_request = setupClientTextMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            message_text
    );

    grpc_chat_commands::ClientMessageToServerResponse text_message_response;

    //invalid timestamp observed
    const long invalid_timestamp_observed = getCurrentTimestamp().count()*100;
    text_message_request.set_timestamp_observed(invalid_timestamp_observed);

    clientMessageToServer(&text_message_request, &text_message_response);

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), false);
    EXPECT_NE(text_message_response.timestamp_stored(), invalid_timestamp_observed);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

    user_account.getFromCollection(generated_account_oid);

    EXPECT_EQ(user_account.chat_rooms.size(), 1);
    if(!user_account.chat_rooms.empty()) {
        EXPECT_NE(user_account.chat_rooms[0].last_time_viewed.value.count(), invalid_timestamp_observed);
    }

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_FALSE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_StandardMessageInfo_chatRoomId) {

    std::string message_uuid = generateUUID();
    std::string message_text = gen_random_alpha_numeric_string((rand() % 1000) + 5);

    grpc_chat_commands::ClientMessageToServerRequest text_message_request = setupClientTextMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            message_text
    );

    grpc_chat_commands::ClientMessageToServerResponse text_message_response;
    text_message_request.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from("123");

    clientMessageToServer(&text_message_request, &text_message_response);

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), false);
    EXPECT_EQ(text_message_response.timestamp_stored(), -2);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

}

TEST_F(ClientMessageToServerTests, invalid_kTextMessage_messageText_empty) {

    std::string message_uuid = generateUUID();
    std::string message_text;

    grpc_chat_commands::ClientMessageToServerRequest text_message_request = setupClientTextMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            message_text
    );
    grpc_chat_commands::ClientMessageToServerResponse text_message_response;

    clientMessageToServer(&text_message_request, &text_message_response);

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), false);
    EXPECT_EQ(text_message_response.timestamp_stored(), -2);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kTextMessage_messageText_tooLong) {

    std::string message_uuid = generateUUID();
    std::string message_text = gen_random_alpha_numeric_string((rand() % 1000) + server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_MESSAGE + 1);

    grpc_chat_commands::ClientMessageToServerRequest text_message_request = setupClientTextMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            message_text
    );
    grpc_chat_commands::ClientMessageToServerResponse text_message_response;

    clientMessageToServer(&text_message_request, &text_message_response);

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), false);
    EXPECT_EQ(text_message_response.timestamp_stored(), -2);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kPictureMessage_imageHeight) {

    std::string message_uuid = generateUUID();
    std::string picture_in_bytes = gen_random_alpha_numeric_string((rand() % 1000) + 5);

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientPictureMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            picture_in_bytes
        );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_height(0);

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kPictureMessage_imageWidth) {

    std::string message_uuid = generateUUID();
    std::string picture_in_bytes = gen_random_alpha_numeric_string((rand() % 1000) + 5);

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientPictureMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            picture_in_bytes
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_width(0);

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kPictureMessage_corruptPicture) {

    std::string message_uuid = generateUUID();
    std::string picture_in_bytes = gen_random_alpha_numeric_string((rand() % 1000) + 5);

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientPictureMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            picture_in_bytes
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_picture_file_size((int)picture_in_bytes.size() - 1);

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::CORRUPTED_FILE);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kPictureMessage_emptyFile) {

    std::string message_uuid = generateUUID();
    std::string picture_in_bytes;

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientPictureMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            picture_in_bytes
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kPictureMessage_pictureTooLarge) {
    std::string message_uuid = generateUUID();
    std::string picture_in_bytes = gen_random_alpha_numeric_string((rand() % 1000) + server_parameter_restrictions::MAXIMUM_CHAT_MESSAGE_PICTURE_SIZE_IN_BYTES + 1);

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientPictureMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            picture_in_bytes
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kLocationMessage_longitude) {

    std::string message_uuid = generateUUID();

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientLocationMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            185, //longitude must be a number -180 to 180
            -56 //latitude must be a number -90 to 90
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kLocationMessage_latitude) {

    std::string message_uuid = generateUUID();

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientLocationMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            0, //longitude must be a number -180 to 180
            -99 //latitude must be a number -90 to 90
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeTypeMessage_imageHeight) {
    std::string message_uuid = generateUUID();
    std::string url_of_download = "url_of_download";
    std::string mime_type = accepted_mime_types.empty() ? "" : *accepted_mime_types.begin();

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientMimeTypeMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            url_of_download,
            mime_type
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;
    client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_height(0);

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeTypeMessage_imageWidth) {
    std::string message_uuid = generateUUID();
    std::string url_of_download = "url_of_download";
    std::string mime_type = accepted_mime_types.empty() ? "" : *accepted_mime_types.begin();

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientMimeTypeMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            url_of_download,
            mime_type
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;
    client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_width(0);

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeTypeMessage_urlOfDownload_empty) {
    std::string message_uuid = generateUUID();
    std::string url_of_download;
    std::string mime_type = accepted_mime_types.empty() ? "" : *accepted_mime_types.begin();

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientMimeTypeMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            url_of_download,
            mime_type
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeTypeMessage_urlOfDownload_tooLarge) {
    std::string message_uuid = generateUUID();
    std::string url_of_download = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 + rand() % 100);
    std::string mime_type = accepted_mime_types.empty() ? "" : *accepted_mime_types.begin();

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientMimeTypeMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            url_of_download,
            mime_type
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeTypeMessage_mimeType_doesNotExist) {
    std::string message_uuid = generateUUID();
    std::string url_of_download = "url_of_download";
    std::string mime_type = "123$";

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientMimeTypeMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            url_of_download,
            mime_type
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeTypeMessage_mimeType_tooLarge) {
    std::string message_uuid = generateUUID();
    std::string url_of_download = "url_of_download";
    std::string mime_type = gen_random_alpha_numeric_string(largest_mime_type_size + 1);

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientMimeTypeMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            url_of_download,
            mime_type
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kInviteMessage_accountOid) {
    const std::string message_uuid = generateUUID();
    const std::string invited_user_account_oid = "invited_user_account_oid";
    const std::string invited_user_name = "username";
    const std::string invited_chat_room_id = generateRandomChatRoomId();
    const std::string invited_chat_room_name = "chat_room_name";
    const std::string invited_chat_room_password = "chat_room_password";

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientInviteMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            invited_user_account_oid,
            invited_user_name,
            invited_chat_room_id,
            invited_chat_room_name,
            invited_chat_room_password
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kInviteMessage_userName) {
    const std::string message_uuid = generateUUID();
    const std::string invited_user_account_oid = bsoncxx::oid{}.to_string();
    const std::string invited_user_name = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 + rand() % 100);
    const std::string invited_chat_room_id = generateRandomChatRoomId();
    const std::string invited_chat_room_name = "chat_room_name";
    const std::string invited_chat_room_password = "chat_room_password";

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientInviteMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            invited_user_account_oid,
            invited_user_name,
            invited_chat_room_id,
            invited_chat_room_name,
            invited_chat_room_password
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kInviteMessage_chatRoomId) {
    const std::string message_uuid = generateUUID();
    const std::string invited_user_account_oid = bsoncxx::oid{}.to_string();
    const std::string invited_user_name = "invited_user_name";
    const std::string invited_chat_room_id = "123";
    const std::string invited_chat_room_name = "chat_room_name";
    const std::string invited_chat_room_password = "chat_room_password";

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientInviteMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            invited_user_account_oid,
            invited_user_name,
            invited_chat_room_id,
            invited_chat_room_name,
            invited_chat_room_password
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kInviteMessage_chatRoomName) {
    const std::string message_uuid = generateUUID();
    const std::string invited_user_account_oid = bsoncxx::oid{}.to_string();
    const std::string invited_user_name = "invited_user_name";
    const std::string invited_chat_room_id = generateRandomChatRoomId();
    const std::string invited_chat_room_name = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 + rand() % 100);
    const std::string invited_chat_room_password = "chat_room_password";

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientInviteMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            invited_user_account_oid,
            invited_user_name,
            invited_chat_room_id,
            invited_chat_room_name,
            invited_chat_room_password
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kInviteMessage_chatRoomPassword) {
    const std::string message_uuid = generateUUID();
    const std::string invited_user_account_oid = bsoncxx::oid{}.to_string();
    const std::string invited_user_name = "invited_user_name";
    const std::string invited_chat_room_id = generateRandomChatRoomId();
    const std::string invited_chat_room_name = "chat_room_name";
    const std::string invited_chat_room_password = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 + rand() % 100);

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientInviteMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            invited_user_account_oid,
            invited_user_name,
            invited_chat_room_id,
            invited_chat_room_name,
            invited_chat_room_password
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kEditedMessage_newMessage_empty) {
    const std::string message_uuid = generateUUID();
    const std::string edited_message_uuid = generateUUID();
    const std::string edited_message_text;

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientEditedMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            edited_message_uuid,
            edited_message_text
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kEditedMessage_newMessage_tooLong) {
    const std::string message_uuid = generateUUID();
    const std::string edited_message_uuid = generateUUID();
    const std::string edited_message_text = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 + rand() % 100);

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientEditedMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            edited_message_uuid,
            edited_message_text
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kEditedMessage_messageUuid) {
    const std::string message_uuid = generateUUID();
    const std::string edited_message_uuid = "edited_message_uuid";
    const std::string edited_message_text = "edited_message_text";

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientEditedMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            edited_message_uuid,
            edited_message_text
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kDeletedMessage_deleteType) {
    const std::string message_uuid = generateUUID();
    const std::string deleted_message_uuid = generateUUID();
    const DeleteType delete_type = DeleteType::DELETED_ON_CLIENT;

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientDeletedMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            deleted_message_uuid,
            delete_type
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kDeletedMessage_messageUuid) {
    const std::string message_uuid = generateUUID();
    const std::string deleted_message_uuid = "invalid_uuid";
    const DeleteType delete_type = DeleteType::DELETE_FOR_ALL_USERS;

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientDeletedMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            deleted_message_uuid,
            delete_type
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_replyChatMessageInfo_sentFromUserOid) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addTextReplyToRequest(
        active_message_info,
        generated_account_oid,
        message_uuid,
        first_text_message
    );

    active_message_info->mutable_reply_info()->set_reply_is_sent_from_user_oid("invalid_oid");

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_replyChatMessageInfo_messageUuid) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addTextReplyToRequest(
            active_message_info,
            generated_account_oid,
            message_uuid,
            first_text_message
    );

    active_message_info->mutable_reply_info()->set_reply_is_to_message_uuid("invalid_uuid");

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kPictureReply_corrupt) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    const std::string thumbnail_in_bytes = gen_random_alpha_numeric_string(
            rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
    );

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addPictureReplyToRequest(
        active_message_info,
        generated_account_oid,
        message_uuid,
        thumbnail_in_bytes
    );

    active_message_info->mutable_reply_info()->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_file_size(thumbnail_in_bytes.size()-1);

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::CORRUPTED_FILE);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), 0);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kPictureReply_thumbnailInBytes_empty) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    const std::string thumbnail_in_bytes;

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addPictureReplyToRequest(
            active_message_info,
            generated_account_oid,
            message_uuid,
            thumbnail_in_bytes
    );

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kPictureReply_thumbnailInBytes_tooLarge) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    const std::string thumbnail_in_bytes = gen_random_alpha_numeric_string(
            server_parameter_restrictions::MAXIMUM_CHAT_MESSAGE_THUMBNAIL_SIZE_IN_BYTES + 1 + rand() % 100
            );

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addPictureReplyToRequest(
            active_message_info,
            generated_account_oid,
            message_uuid,
            thumbnail_in_bytes
    );

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeReply_thumbnailInBytes_corrupt) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    const std::string thumbnail_in_bytes = gen_random_alpha_numeric_string(
            1 + rand() % 100
    );
    const std::string mime_type = accepted_mime_types.empty() ? "" : *accepted_mime_types.begin();

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addMimeTypeReplyToRequest(
            active_message_info,
            generated_account_oid,
            message_uuid,
            thumbnail_in_bytes,
            mime_type
    );

    active_message_info->mutable_reply_info()->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_file_size(thumbnail_in_bytes.size()-1);

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::CORRUPTED_FILE);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), 0);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeReply_thumbnailInBytes_empty) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    const std::string thumbnail_in_bytes = gen_random_alpha_numeric_string(
            server_parameter_restrictions::MAXIMUM_CHAT_MESSAGE_THUMBNAIL_SIZE_IN_BYTES + 1 + rand() % 100
    );
    const std::string mime_type = accepted_mime_types.empty() ? "" : *accepted_mime_types.begin();

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addMimeTypeReplyToRequest(
            active_message_info,
            generated_account_oid,
            message_uuid,
            thumbnail_in_bytes,
            mime_type
    );

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeReply_thumbnailInBytes_tooLarge) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    const std::string thumbnail_in_bytes = gen_random_alpha_numeric_string(
            server_parameter_restrictions::MAXIMUM_CHAT_MESSAGE_THUMBNAIL_SIZE_IN_BYTES + 1 + rand() % 100
    );
    const std::string mime_type = accepted_mime_types.empty() ? "" : *accepted_mime_types.begin();

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addMimeTypeReplyToRequest(
            active_message_info,
            generated_account_oid,
            message_uuid,
            thumbnail_in_bytes,
            mime_type
    );

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeReply_mimeType_empty) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    const std::string thumbnail_in_bytes = gen_random_alpha_numeric_string(
            1 + rand() % 100
    );
    const std::string mime_type;

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addMimeTypeReplyToRequest(
            active_message_info,
            generated_account_oid,
            message_uuid,
            thumbnail_in_bytes,
            mime_type
    );

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_kMimeReply_mimeType_tooLarge) {

    std::string message_uuid = generateUUID();
    std::string first_text_message = gen_random_alpha_numeric_string(
            (rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE-1)) + 1
    );

    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    const std::string thumbnail_in_bytes = gen_random_alpha_numeric_string(
            1 + rand() % 100
    );
    const std::string mime_type = gen_random_alpha_numeric_string(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 + rand() % 100
            );

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addMimeTypeReplyToRequest(
            active_message_info,
            generated_account_oid,
            message_uuid,
            thumbnail_in_bytes,
            mime_type
    );

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_replyType) {

    std::string message_uuid = generateUUID();
    std::string reply_message_uuid = generateUUID();

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            chat_room_id,
            reply_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            ),
            false
    );

    auto active_message_info = client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    addLocationReplyToRequest(
            active_message_info,
            generated_account_oid,
            message_uuid
    );

    active_message_info->mutable_reply_info()->mutable_reply_specifics()->Clear();

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    EXPECT_EQ(client_message_to_server_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(client_message_to_server_response.user_not_in_chat_room(), false);
    EXPECT_EQ(client_message_to_server_response.timestamp_stored(), -2);
    EXPECT_TRUE(client_message_to_server_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_messageType) {

    std::string message_uuid = generateUUID();
    std::string message_text = gen_random_alpha_numeric_string(rand() % 100 + 1);

    grpc_chat_commands::ClientMessageToServerRequest text_message_request = setupClientTextMessageRequest(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            message_uuid,
            chat_room_id,
            message_text
    );
    grpc_chat_commands::ClientMessageToServerResponse text_message_response;

    text_message_request.mutable_message()->mutable_message_specifics()->Clear();

    clientMessageToServer(&text_message_request, &text_message_response);

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(text_message_response.user_not_in_chat_room(), false);
    EXPECT_EQ(text_message_response.timestamp_stored(), -2);
    EXPECT_TRUE(text_message_response.picture_oid().empty());

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);
    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(ClientMessageToServerTests, invalid_loginInfo) {
    checkLoginInfoClientOnly(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {

                std::string message_uuid = generateUUID();
                std::string message_text = gen_random_alpha_numeric_string(rand() % 100 + 1);

                grpc_chat_commands::ClientMessageToServerRequest text_message_request = setupClientTextMessageRequest(
                        generated_account_oid,
                        user_account.logged_in_token,
                        user_account.installation_ids.front(),
                        message_uuid,
                        chat_room_id,
                        message_text
                );

                text_message_request.mutable_login_info()->CopyFrom(login_info);

                grpc_chat_commands::ClientMessageToServerResponse text_message_response;

                clientMessageToServer(&text_message_request, &text_message_response);

                return text_message_response.return_status();
            }
    );
}