//
// Created by jeremiah on 6/24/22.
//

#include <utility_general_functions.h>
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>
#include <account_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "utility_chat_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include "grpc_mock_stream/mock_stream.h"
#include <setup_login_info.h>
#include <chat_room_message_keys.h>
#include <generate_random_messages.h>
#include <extract_thumbnail_from_verified_doc.h>
#include <helper_functions/messages_to_client.h>
#include <chat_stream_container.h>
#include <chat_rooms_objects.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class StreamInitializationMessagesToClient : public ::testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection chat_room_collection;

    bsoncxx::oid first_account_oid;

    UserAccountDoc first_user_account_doc;

    std::string chat_room_id;

    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    bsoncxx::builder::stream::document user_account_doc;

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        first_account_oid = insertRandomAccounts(1, 0);

        first_user_account_doc.getFromCollection(first_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                first_account_oid,
                first_user_account_doc.logged_in_token,
                first_user_account_doc.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();

        chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

        first_user_account_doc.convertToDocument(user_account_doc);

        //sleep to make sure the chat room cap message is before the potentially next messages
        // 3 ms is because the cap_message is +1ms and the differentUserJoined message is +2ms
        std::this_thread::sleep_for(std::chrono::milliseconds{3});
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runMessagesTestForCheckMessageAmount(
            std::unordered_set<MessageSpecifics::MessageBodyCase> message_types_copy,
            const AmountOfMessage& amount_of_message,
            const std::string& second_account_oid_str
    ) {
        reply_vector.clear();
        StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
                reply_vector);

        const bool called_from_chat_stream_initialization = false;
        const bool only_store_message_bool_value = true;
        const bool request_final_few_messages_in_full = false;

        bsoncxx::builder::basic::array empty_array_builder;

        bool return_val = streamInitializationMessagesToClient(
                mongo_cpp_client,
                accounts_db,
                chat_room_collection,
                user_accounts_collection,
                user_account_doc.view(),
                chat_room_id,
                second_account_oid_str,
                &storeAndSendMessagesToClient,
                amount_of_message,
                DifferentUserJoinedChatRoomAmount::SKELETON,
                called_from_chat_stream_initialization,
                only_store_message_bool_value,
                request_final_few_messages_in_full,
                std::chrono::milliseconds{-1L},
                empty_array_builder
        );

        EXPECT_TRUE(return_val);

        storeAndSendMessagesToClient.finalCleanup();

        ASSERT_EQ(reply_vector.size(), 1);
        EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 19);

        mongocxx::options::find opts;

        opts.sort(
                document{}
                        << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                        << finalize
        );

        auto messages = chat_room_collection.find(
                document{}
                        << "_id" << open_document
                        << "$ne" << "iDe"
                        << close_document
                        << chat_room_message_keys::MESSAGE_TYPE << open_document
                        << "$nin" << open_array
                        << (int) MessageSpecifics::MessageBodyCase::kEditedMessage
                        << (int) MessageSpecifics::MessageBodyCase::kDeletedMessage
                        << close_array
                        << close_document
                        << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "."
                           + chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT + "."
                           +
                           chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << open_document
                        << "$ne" << DeleteType::DELETE_FOR_ALL_USERS
                        << close_document
                        << finalize,
                opts
        );

        ExtractUserInfoObjects extractUserInfoObjects(
                mongo_cpp_client, accounts_db, user_accounts_collection
        );

        int index = 0;
        for (const bsoncxx::document::view& val: messages) {
            ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

            ChatMessageToClient response;

            return_val = convertChatMessageDocumentToChatMessageToClient(
                    val,
                    chat_room_id,
                    second_account_oid_str,
                    only_store_message_bool_value,
                    &response,
                    amount_of_message,
                    DifferentUserJoinedChatRoomAmount::SKELETON,
                    called_from_chat_stream_initialization,
                    &extractUserInfoObjects,
                    true
            );

            EXPECT_TRUE(return_val);

            message_types_copy.erase(response.message().message_specifics().message_body_case());

            bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                    response,
                    reply_vector[0]->return_new_chat_message().messages_list()[index]
            );

            if (!equivalent) {
                std::cout << "response\n" << response.DebugString() << '\n';
                std::cout << "reply_vector\n" << reply_vector[0]->return_new_chat_message().messages_list()[index].DebugString() << '\n';
            }

            EXPECT_TRUE(equivalent);

            index++;
        }

        EXPECT_TRUE(message_types_copy.empty());
    }
};

TEST_F(StreamInitializationMessagesToClient,
       streamInitializationMessagesToClient_timeOfMessageToCompare_noMessagesExtracted) {

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = true;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(), //request no messages
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            getCurrentTimestamp(),
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    EXPECT_TRUE(reply_vector.empty());
}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_inviteFromCurrentUser) {

    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    grpc_chat_commands::CreateChatRoomRequest invite_create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse invite_create_chat_room_response;

    setupUserLoginInfo(
            invite_create_chat_room_request.mutable_login_info(),
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front()
    );

    createChatRoom(&invite_create_chat_room_request, &invite_create_chat_room_response);

    const std::string invite_message_uuid = generateUUID();

    generateRandomInviteTypeMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            invite_message_uuid,
            second_account_oid.to_string(),
            second_user_account_doc.first_name,
            invite_create_chat_room_response.chat_room_id(),
            invite_create_chat_room_response.chat_room_name(),
            invite_create_chat_room_response.chat_room_password()
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = true;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{-1L},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 4);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                response,
                reply_vector[0]->return_new_chat_message().messages_list()[index]
        );

        if (!equivalent) {
            std::cout << "response\n" << response.DebugString() << '\n';
            std::cout << "reply_vector\n" << reply_vector[0]->return_new_chat_message().messages_list()[index].DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);

        index++;
    }

}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_inviteToCurrentUser) {

    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }
    grpc_chat_commands::CreateChatRoomRequest invite_create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse invite_create_chat_room_response;

    setupUserLoginInfo(
            invite_create_chat_room_request.mutable_login_info(),
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front()
    );

    createChatRoom(&invite_create_chat_room_request, &invite_create_chat_room_response);

    const std::string invite_message_uuid = generateUUID();

    generateRandomInviteTypeMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            invite_message_uuid,
            first_account_oid.to_string(),
            first_user_account_doc.first_name,
            invite_create_chat_room_response.chat_room_id(),
            invite_create_chat_room_response.chat_room_name(),
            invite_create_chat_room_response.chat_room_password()
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = true;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{-1L},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 4);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }
}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_inviteIgnored) {

    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    grpc_chat_commands::CreateChatRoomRequest invite_create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse invite_create_chat_room_response;

    setupUserLoginInfo(
            invite_create_chat_room_request.mutable_login_info(),
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front()
    );

    createChatRoom(&invite_create_chat_room_request, &invite_create_chat_room_response);

    const std::string invite_message_uuid = generateUUID();

    generateRandomInviteTypeMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            invite_message_uuid,
            bsoncxx::oid{}.to_string(),
            "dummy_user",
            invite_create_chat_room_response.chat_room_id(),
            invite_create_chat_room_response.chat_room_name(),
            invite_create_chat_room_response.chat_room_password()
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = true;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{-1L},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 3);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    //skip extracting the invite message
    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << chat_room_message_keys::MESSAGE_TYPE << open_document
                    << "$ne" << (int) MessageSpecifics::MessageBodyCase::kInviteMessage
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }

}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_deleteForAll) {

    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    const std::string text_message_uuid = generateUUID();

    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string delete_message_uuid = generateUUID();

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            DeleteType::DELETE_FOR_ALL_USERS
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{delete_message_response.timestamp_stored()},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 1);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << chat_room_message_keys::MESSAGE_TYPE << open_document
                    << "$eq" << (int) MessageSpecifics::MessageBodyCase::kDeletedMessage
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }

}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_deleteForCurrentUser) {

    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    const std::string text_message_uuid = generateUUID();

    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string delete_message_uuid = generateUUID();

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            DeleteType::DELETE_FOR_SINGLE_USER
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{delete_message_response.timestamp_stored()},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 1);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << chat_room_message_keys::MESSAGE_TYPE << open_document
                    << "$eq" << (int) MessageSpecifics::MessageBodyCase::kDeletedMessage
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }

}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_deleteIgnored) {

    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    const std::string text_message_uuid = generateUUID();

    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string delete_message_uuid = generateUUID();

    auto [delete_message_request, delete_message_response] = generateRandomDeletedMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            DeleteType::DELETE_FOR_SINGLE_USER
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{delete_message_response.timestamp_stored()},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    EXPECT_TRUE(reply_vector.empty());

}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_deletedMessageGrouped) {

    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    const std::string text_message_uuid = generateUUID();

    generateRandomTextMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string first_delete_message_uuid = generateUUID();

    //user delete for self
    auto [first_delete_message_request, first_delete_message_response] = generateRandomDeletedMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            first_delete_message_uuid,
            text_message_uuid,
            DeleteType::DELETE_FOR_SINGLE_USER
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string second_delete_message_uuid = generateUUID();

    //admin deletes for all
    auto [second_delete_message_request, second_delete_message_response] = generateRandomDeletedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            second_delete_message_uuid,
            text_message_uuid,
            DeleteType::DELETE_FOR_ALL_USERS
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            second_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{first_delete_message_response.timestamp_stored()},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 1);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << chat_room_message_keys::MESSAGE_TYPE << (int) MessageSpecifics::MessageBodyCase::kDeletedMessage
                    << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{
                    std::chrono::milliseconds{second_delete_message_response.timestamp_stored()}}
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                second_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }
}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_editedMessageGrouped) {

    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    const std::string text_message_uuid = generateUUID();

    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string first_delete_message_uuid = generateUUID();

    auto [first_edited_message_request, first_edited_message_response] = generateRandomEditedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            first_delete_message_uuid,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string second_delete_message_uuid = generateUUID();

    auto [second_edited_message_request, second_edited_message_response] = generateRandomEditedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            first_delete_message_uuid,
            text_message_uuid
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{first_edited_message_response.timestamp_stored()},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 1);
    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << chat_room_message_keys::MESSAGE_TYPE << (int) MessageSpecifics::MessageBodyCase::kDeletedMessage
                    << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{
                    std::chrono::milliseconds{second_edited_message_response.timestamp_stored()}}
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }
}

TEST_F(StreamInitializationMessagesToClient,
       streamInitializationMessagesToClient_deletedMessageNotExtractedWithTargetMessage) {

    //If the target message (TextMessage in this case) is extracted, the deleted message should NOT be extracted. All deleted
    // info is stored inside the target.

    const std::string text_message_uuid = generateUUID();

    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string delete_message_uuid = generateUUID();

    generateRandomDeletedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            delete_message_uuid,
            text_message_uuid,
            DeleteType::DELETE_FOR_ALL_USERS
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{-1L},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 2);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << chat_room_message_keys::MESSAGE_TYPE << open_document
                    << "$nin" << open_array
                    << (int) MessageSpecifics::MessageBodyCase::kDeletedMessage
                    << (int) MessageSpecifics::MessageBodyCase::kTextMessage
                    << close_array
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }
}

TEST_F(StreamInitializationMessagesToClient,
       streamInitializationMessagesToClient_editedMessageNotExtractedWithTargetMessage) {

    //If the target message (TextMessage in this case) is extracted, the edited message should NOT be extracted. All edited
    // info is stored inside the target.

    const std::string text_message_uuid = generateUUID();

    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string edited_message_uuid = generateUUID();

    generateRandomEditedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            edited_message_uuid,
            text_message_uuid
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{-1L},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 3);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << chat_room_message_keys::MESSAGE_TYPE << open_document
                    << "$ne" << (int) MessageSpecifics::MessageBodyCase::kEditedMessage
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }
}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_onlyStoreMessageBoolValue_true) {
    //Tests if deleted & edited messages are skipped when onlyStoreMessageBoolValue==true.

    const std::string text_message_uuid = generateUUID();

    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string edited_message_uuid = generateUUID();

    auto [first_edited_message_request, first_edited_message_response] = generateRandomEditedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            edited_message_uuid,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string deleted_message_uuid = generateUUID();

    generateRandomDeletedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            deleted_message_uuid,
            text_message_uuid,
            DeleteType::DELETE_FOR_ALL_USERS
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = true;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{first_edited_message_response.timestamp_stored()},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 0);
}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_onlyStoreMessageBoolValue_false) {
    //Test if deleted & edited messages are extracted when onlyStoreMessageBoolValue==false.

    const std::string text_message_uuid = generateUUID();

    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string deleted_message_uuid = generateUUID();

    auto [first_deleted_message_request, first_deleted_message_response] = generateRandomDeletedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            deleted_message_uuid,
            text_message_uuid,
            DeleteType::DELETE_FOR_ALL_USERS
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string edited_message_uuid = generateUUID();

    //Putting the edited message in first, it handles a specific situation where the deleted and edited messages
    // used to be combined (the bug should be fixed, however the test is still here to make sure).
    generateRandomEditedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            edited_message_uuid,
            text_message_uuid
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{first_deleted_message_response.timestamp_stored()},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 2);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << chat_room_message_keys::MESSAGE_TYPE << open_document
                    << "$in" << open_array
                    << (int) MessageSpecifics::MessageBodyCase::kEditedMessage
                    << (int) MessageSpecifics::MessageBodyCase::kDeletedMessage
                    << close_array
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }

}

TEST_F(StreamInitializationMessagesToClient,
       streamInitializationMessagesToClient_messagesExtracted_checkAmountOfMessage) {
    //Test and make sure that each message type can be extracted at different amountOfMessage values. This is mostly done
    // b/c of the projection that can happen sometimes.

    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid third_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc second_user_account_doc(second_account_oid);
    UserAccountDoc third_user_account_doc(third_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    join_chat_room_request.Clear();

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            third_account_oid,
            third_user_account_doc.logged_in_token,
            third_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    join_mock_server_writer.write_params.clear();
    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    grpc_chat_commands::CreateChatRoomRequest invite_create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse invite_create_chat_room_response;

    setupUserLoginInfo(
            invite_create_chat_room_request.mutable_login_info(),
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front()
    );

    createChatRoom(&invite_create_chat_room_request, &invite_create_chat_room_response);

    const std::string text_message_uuid = generateUUID();
    const std::string message_text = gen_random_alpha_numeric_string(rand() % 100 + 50);

    //TextChatMessage = 1
    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            text_message_uuid
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request;
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    setupUserLoginInfo(
            client_message_to_server_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    client_message_to_server_request.set_message_uuid(generateUUID());
    client_message_to_server_request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = client_message_to_server_request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = client_message_to_server_request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(true);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            first_account_oid.to_string());
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            text_message_uuid);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_text_reply()->set_message_text(
            message_text.substr(0, server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TEXT_MESSAGE_REPLY));

    message_specifics->mutable_text_message()->set_message_text(gen_random_alpha_numeric_string(rand() % 100 + 50));
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    //make a message with a reply
    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //PictureChatMessage = 2
    generateRandomPictureMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //LocationChatMessage = 3
    generateRandomLocationMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //MimeTypeChatMessage = 4
    generateRandomMimeTypeMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //InviteChatMessage invite_message = 5
    generateRandomInviteTypeMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID(),
            second_account_oid.to_string(),
            second_user_account_doc.first_name,
            invite_create_chat_room_response.chat_room_id(),
            invite_create_chat_room_response.chat_room_name(),
            invite_create_chat_room_response.chat_room_password()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //EditedMessageChatMessage = 6;
    generateRandomEditedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID(),
            text_message_uuid
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    const std::string second_text_message_uuid = generateUUID();

    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            second_text_message_uuid
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //DeletedMessageChatMessage = 7;
    generateRandomDeletedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID(),
            second_text_message_uuid,
            DeleteType::DELETE_FOR_ALL_USERS
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //NewAdminPromotedChatMessage = 20;
    generateRandomNewAdminPromotedUpdatedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID(),
            second_account_oid.to_string()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //UserKickedChatMessage user_kicked_message = 8;
    generateRandomUserKickedMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID(),
            third_account_oid.to_string()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    join_mock_server_writer.write_params.clear();
    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    //UserBannedChatMessage user_banned_message = 9;
    generateRandomUserBannedMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID(),
            third_account_oid.to_string()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //DifferentUserJoinedChatRoomChatMessage = 10; already inside

    //DifferentUserLeftChatRoomChatMessage = 11;
    grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
    grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

    setupUserLoginInfo(
            leave_chat_room_request.mutable_login_info(),
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front()
    );

    leave_chat_room_request.set_chat_room_id(chat_room_id);

    leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

    EXPECT_EQ(leave_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    join_chat_room_request.Clear();

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    join_mock_server_writer.write_params.clear();
    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    //Not actually stored
    //UpdateObservedTimeChatMessage = 12;
    //ThisUserJoinedChatRoomStartChatMessage = 13;
    //ThisUserJoinedChatRoomMemberChatMessage = 14;
    //ThisUserJoinedChatRoomFinishedChatMessage = 15;
    //ThisUserLeftChatRoomChatMessage = 16;

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //UserActivityDetectedChatMessage = 17;
    generateRandomUserActivityDetectedMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //ChatRoomNameUpdatedChatMessage = 18;
    generateRandomChatRoomNameUpdatedMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    //ChatRoomPasswordUpdatedChatMessage = 19;
    generateRandomChatRoomPasswordUpdatedMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    //ChatRoomCapMessage = 21; already inside

    const std::unordered_set<MessageSpecifics::MessageBodyCase> message_types{
            MessageSpecifics::MessageBodyCase::kTextMessage,
            MessageSpecifics::MessageBodyCase::kPictureMessage,
            MessageSpecifics::MessageBodyCase::kLocationMessage,
            MessageSpecifics::MessageBodyCase::kMimeTypeMessage,
            MessageSpecifics::MessageBodyCase::kInviteMessage,
            //        MessageSpecifics::MessageBodyCase::kEditedMessage,
            //        MessageSpecifics::MessageBodyCase::kDeletedMessage,
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            MessageSpecifics::MessageBodyCase::kUserBannedMessage,
            MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage,
            MessageSpecifics::MessageBodyCase::kDifferentUserLeftMessage,
            //        MessageSpecifics::MessageBodyCase::kUpdateObservedTimeMessage, (not actually stored, just updates time in user account)
            //        MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomStartMessage,
            //        MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomMemberMessage,
            //        MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomFinishedMessage,
            //        MessageSpecifics::MessageBodyCase::kThisUserLeftChatRoomMessage,
            MessageSpecifics::MessageBodyCase::kUserActivityDetectedMessage,
            MessageSpecifics::MessageBodyCase::kChatRoomNameUpdatedMessage,
            MessageSpecifics::MessageBodyCase::kChatRoomPasswordUpdatedMessage,
            MessageSpecifics::MessageBodyCase::kNewAdminPromotedMessage,
            MessageSpecifics::MessageBodyCase::kChatRoomCapMessage
    };

    runMessagesTestForCheckMessageAmount(
            message_types,
            AmountOfMessage::ONLY_SKELETON,
            second_account_oid.to_string()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    runMessagesTestForCheckMessageAmount(
            message_types,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            second_account_oid.to_string()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    runMessagesTestForCheckMessageAmount(
            message_types,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            second_account_oid.to_string()
    );
}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_multipleUserActivityDetected) {
    //Test multiple userActivityDetected messages and only final one is sent back.

    generateRandomUserActivityDetectedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID()
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    generateRandomUserActivityDetectedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID()
    );

    //sleep so messages are not sent at the same time
    std::this_thread::sleep_for(std::chrono::milliseconds{5});

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomUserActivityDetectedMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID()
    );

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{-1L},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 3);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << chat_room_message_keys::MESSAGE_TYPE << open_document
                    << "$ne" << (int) MessageSpecifics::MessageBodyCase::kUserActivityDetectedMessage
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    int num_user_activity_messages = 0;
    long largest_user_activity_message_time = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        if (reply_vector[0]->return_new_chat_message().messages_list()[index].message().message_specifics().message_body_case()
            == MessageSpecifics::MessageBodyCase::kUserActivityDetectedMessage) {
            largest_user_activity_message_time = std::max(
                    reply_vector[0]->return_new_chat_message().messages_list()[index].timestamp_stored(),
                    largest_user_activity_message_time);
            num_user_activity_messages++;
        } else {

            ChatMessageToClient response;

            return_val = convertChatMessageDocumentToChatMessageToClient(
                    val,
                    chat_room_id,
                    first_account_oid.to_string(),
                    only_store_message_bool_value,
                    &response,
                    amount_of_message,
                    different_user_joined_chat_room_amount,
                    called_from_chat_stream_initialization,
                    &extractUserInfoObjects,
                    true
            );

            EXPECT_TRUE(return_val);

            EXPECT_TRUE(
                    google::protobuf::util::MessageDifferencer::Equivalent(
                            response,
                            reply_vector[0]->return_new_chat_message().messages_list()[index]
                    )
            );
        }

        index++;
    }

    for (int i = index; i < reply_vector[0]->return_new_chat_message().messages_list_size(); i++) {
        if (reply_vector[0]->return_new_chat_message().messages_list()[i].message().message_specifics().message_body_case()
            == MessageSpecifics::MessageBodyCase::kUserActivityDetectedMessage) {
            largest_user_activity_message_time = std::max(
                    reply_vector[0]->return_new_chat_message().messages_list()[i].timestamp_stored(),
                    largest_user_activity_message_time);
            num_user_activity_messages++;
        }
    }

    //make sure only 1 activity message was returned AND it was the final one
    EXPECT_EQ(num_user_activity_messages, 1);
    EXPECT_EQ(largest_user_activity_message_time, client_message_to_server_response.timestamp_stored());

}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_requestFinalFewMessagesInFull) {
    //Test if requestFinalFewMessagesInFull overrides amountOfMessage, and properly sends back the messages.

    const int num_messages_inserted = chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST + 5;
    const int total_num_messages = num_messages_inserted + 2;

    for (int i = 0; i < num_messages_inserted; i++) {

        //Insert text messages long enough that COMPLETE_MESSAGE_INFO is required for extraction.
        generateRandomTextMessage(
                first_account_oid,
                first_user_account_doc.logged_in_token,
                first_user_account_doc.installation_ids.front(),
                chat_room_id,
                generateUUID(),
                gen_random_alpha_numeric_string(
                        rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
        );

        //sleep so messages are not sent at the same time
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = false;
    const bool request_final_few_messages_in_full = true;

    bsoncxx::builder::basic::array empty_array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            AmountOfMessage::ONLY_SKELETON, //should be ignored
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            std::chrono::milliseconds{-1L},
            empty_array_builder
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), total_num_messages);

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                    << finalize
    );

    auto messages = chat_room_collection.find(
            document{}
                    << "_id" << open_document
                    << "$ne" << "iDe"
                    << close_document
                    << finalize,
            opts
    );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
    );

    int index = 0;
    for (const bsoncxx::document::view& val: messages) {
        ASSERT_LT(index, reply_vector[0]->return_new_chat_message().messages_list_size());

        AmountOfMessage amount_of_message;

        if (total_num_messages - index > chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST) {
            amount_of_message = AmountOfMessage::ONLY_SKELETON;
        } else {
            amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
        }

        ChatMessageToClient response;

        return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                first_account_oid.to_string(),
                only_store_message_bool_value,
                &response,
                amount_of_message,
                different_user_joined_chat_room_amount,
                called_from_chat_stream_initialization,
                &extractUserInfoObjects,
                true
        );

        EXPECT_TRUE(return_val);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->return_new_chat_message().messages_list()[index]
                )
        );

        index++;
    }

}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_skipPassedMessageUuid) {

    const std::chrono::milliseconds timestamp_before_messages = getCurrentTimestamp();

    std::vector<std::string> message_uuids{
            generateUUID(),
            generateUUID()
    };

    for (auto & message_uuid : message_uuids) {

        //Insert text messages long enough that COMPLETE_MESSAGE_INFO is required for extraction.
        generateRandomTextMessage(
                first_account_oid,
                first_user_account_doc.logged_in_token,
                first_user_account_doc.installation_ids.front(),
                chat_room_id,
                message_uuid,
                gen_random_alpha_numeric_string(100)
        );

        //sleep so messages are not sent at the same time
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = true;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array array_builder;

    array_builder.append(message_uuids[0]);

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(), //request inserted messages
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            timestamp_before_messages,
            array_builder //skip first message
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    for (auto & i : reply_vector) {
        for (int j = 0; j < (int) i->return_new_chat_message().messages_list_size(); ++j) {
            std::cout << i->return_new_chat_message().messages_list(j).DebugString() << '\n';
        }
    }

    EXPECT_EQ(reply_vector.size(), 1);
    if (!reply_vector.empty()) {
        EXPECT_TRUE(reply_vector[0]->has_return_new_chat_message());
        if (reply_vector[0]->has_return_new_chat_message()) {
            EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 1);
            if (!reply_vector[0]->return_new_chat_message().messages_list().empty()) {
                ChatRoomMessageDoc message_doc(message_uuids[1], chat_room_id);

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                std::cout << "makePrettyJson\n" << makePrettyJson(message_doc_builder) << '\n';

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                ChatMessageToClient generated_message;

                return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        first_account_oid.to_string(),
                        false,
                        &generated_message,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);
                if (return_val) {
                    google::protobuf::util::MessageDifferencer::Equivalent(
                            generated_message,
                            reply_vector[0]->return_new_chat_message().messages_list(0)
                    );
                }
            }
        }
    }
}

TEST_F(StreamInitializationMessagesToClient, streamInitializationMessagesToClient_requestBeforeTime) {

    const std::chrono::milliseconds timestamp_before_messages = getCurrentTimestamp();

    std::vector<std::string> message_uuids{
            generateUUID(),
            generateUUID()
    };

    std::vector<std::chrono::milliseconds> message_timestamps;

    for (auto & message_uuid : message_uuids) {

        //Insert text messages long enough that COMPLETE_MESSAGE_INFO is required for extraction.
        auto [text_request, text_response] = generateRandomTextMessage(
                first_account_oid,
                first_user_account_doc.logged_in_token,
                first_user_account_doc.installation_ids.front(),
                chat_room_id,
                message_uuid,
                gen_random_alpha_numeric_string(100)
        );

        message_timestamps.emplace_back(text_response.timestamp_stored());

        //sleep so messages are not sent at the same time
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    const DifferentUserJoinedChatRoomAmount different_user_joined_chat_room_amount = DifferentUserJoinedChatRoomAmount::SKELETON;
    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
    const bool called_from_chat_stream_initialization = false;
    const bool only_store_message_bool_value = true;
    const bool request_final_few_messages_in_full = false;

    bsoncxx::builder::basic::array array_builder;

    bool return_val = streamInitializationMessagesToClient(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_accounts_collection,
            user_account_doc.view(),
            chat_room_id,
            first_account_oid.to_string(), //request inserted messages
            &storeAndSendMessagesToClient,
            amount_of_message,
            different_user_joined_chat_room_amount,
            called_from_chat_stream_initialization,
            only_store_message_bool_value,
            request_final_few_messages_in_full,
            timestamp_before_messages,
            array_builder,
            message_timestamps[1] - std::chrono::milliseconds{1} //skip index 1
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    for (auto & i : reply_vector) {
        for (auto & j : i->return_new_chat_message().messages_list()) {
            std::cout << j.DebugString() << '\n';
        }
    }

    EXPECT_EQ(reply_vector.size(), 1);
    if (!reply_vector.empty()) {
        EXPECT_TRUE(reply_vector[0]->has_return_new_chat_message());
        if (reply_vector[0]->has_return_new_chat_message()) {
            EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 1);
            if (!reply_vector[0]->return_new_chat_message().messages_list().empty()) {
                ChatRoomMessageDoc message_doc(message_uuids[0], chat_room_id);

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                std::cout << "makePrettyJson\n" << makePrettyJson(message_doc_builder) << '\n';

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                ChatMessageToClient generated_message;

                return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        first_account_oid.to_string(),
                        false,
                        &generated_message,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);
                if (return_val) {
                    google::protobuf::util::MessageDifferencer::Equivalent(
                            generated_message,
                            reply_vector[0]->return_new_chat_message().messages_list(0)
                    );
                }
            }
        }
    }
}