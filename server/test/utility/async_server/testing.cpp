//
// Created by jeremiah on 6/26/22.
//
#include <utility_general_functions.h>
#include <server_parameter_restrictions.h>
#include <android_specific_values.h>
#include <general_values.h>
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
#include <reports_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include <chat_rooms_objects.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "change_stream_utility.h"
#include "generate_randoms.h"
#include "chat_change_stream_helper_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include <generate_random_messages.h>
#include <async_server.h>
#include <start_server.h>
#include <testing_client.h>
#include <start_bi_di_stream.h>
#include <user_open_chat_streams.h>
#include <chat_stream_container_object.h>
#include <chat_room_message_keys.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

const inline long SLEEP_TIME_FOR_CHAT_STREAM_TO_START = 50;

class AsyncServer_SingleOperation : public ::testing::Test {
protected:

    std::chrono::milliseconds original_chat_change_stream_await_time{-1};

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        original_chat_change_stream_await_time = chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME;
        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = std::chrono::milliseconds{50};
    }

    void TearDown() override {
        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = original_chat_change_stream_await_time;
        clearDatabaseAndGlobalsForTesting();
    }

    //Sleep while the chat stream starts (required to extract from user_open_chat_streams).
    static void sleepForChatStreamToStart(int additiveValue = 0) {
        // search for startBiDiStream
        std::this_thread::sleep_for(std::chrono::milliseconds{SLEEP_TIME_FOR_CHAT_STREAM_TO_START + additiveValue});
    }

};

class AsyncServer_OperationCombinations : public ::testing::Test {
protected:

    std::chrono::milliseconds original_chat_change_stream_await_time{-1};

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        original_chat_change_stream_await_time = chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME;
        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = std::chrono::milliseconds{50};
    }

    void TearDown() override {
        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = original_chat_change_stream_await_time;
        clearDatabaseAndGlobalsForTesting();
    }

    //Sleep while the chat stream starts (required to extract from user_open_chat_streams).
    static void sleepForChatStreamToStart(int additiveValue = 0) {
        // search for startBiDiStream
        std::this_thread::sleep_for(std::chrono::milliseconds{SLEEP_TIME_FOR_CHAT_STREAM_TO_START + additiveValue});
    }
};

class AsyncServer_MultipleItemsInCompletionQueue : public ::testing::Test {
protected:

    std::chrono::milliseconds original_chat_change_stream_await_time{-1};

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        original_chat_change_stream_await_time = chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME;
        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = std::chrono::milliseconds{50};
    }

    void TearDown() override {
        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = original_chat_change_stream_await_time;
        clearDatabaseAndGlobalsForTesting();
    }

    //Sleep while the chat stream starts (required to extract from user_open_chat_streams).
    static void sleepForChatStreamToStart(int additiveValue = 0) {
        // search for startBiDiStream
        std::this_thread::sleep_for(std::chrono::milliseconds{SLEEP_TIME_FOR_CHAT_STREAM_TO_START + additiveValue});
    }
};

TEST_F(AsyncServer_SingleOperation, refreshChatStream) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    sleepForChatStreamToStart();

    long first_refresh_time;

    {
        auto chat_stream_container = user_open_chat_streams.find(generated_account_oid.to_string());
        first_refresh_time = chat_stream_container->ptr()->end_stream_time.time_since_epoch().count();
    }

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();

    client->sendBiDiMessage(request);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 1);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    long second_refresh_time;

    {
        auto chat_stream_container = user_open_chat_streams.find(generated_account_oid.to_string());
        second_refresh_time = chat_stream_container->ptr()->end_stream_time.time_since_epoch().count();
    }

    EXPECT_GT(second_refresh_time, first_refresh_time);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_SingleOperation, refreshChatStream_withDelay) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    long time_in_millis_to_delay = 100;

    std::chrono::milliseconds start_timestamp;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        std::chrono::milliseconds stop_timestamp = getCurrentTimestamp();

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_GE(stop_timestamp.count() - start_timestamp.count(), time_in_millis_to_delay);
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    sleepForChatStreamToStart();

    long first_refresh_time;

    {
        auto chat_stream_container = user_open_chat_streams.find(generated_account_oid.to_string());
        first_refresh_time = chat_stream_container->ptr()->end_stream_time.time_since_epoch().count();
    }

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();
    request.set_testing_time_for_request_to_run_in_millis(time_in_millis_to_delay);

    start_timestamp = getCurrentTimestamp();
    client->sendBiDiMessage(request);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{time_in_millis_to_delay + 20});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 1);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    long second_refresh_time;

    {
        auto chat_stream_container = user_open_chat_streams.find(generated_account_oid.to_string());
        second_refresh_time = chat_stream_container->ptr()->end_stream_time.time_since_epoch().count();
    }

    EXPECT_GT(second_refresh_time, first_refresh_time);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_SingleOperation, requestMessageUpdate) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    auto[text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse: {
                num_request_full_message_info_received++;

                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

                ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

                EXPECT_NE(message_doc.id, "");

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                grpc_stream_chat::ChatToClientResponse generated_response;

                generated_response.mutable_request_full_message_info_response()->set_request_status(
                        grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST);
                generated_response.mutable_request_full_message_info_response()->set_chat_room_id(chat_room_id);

                ChatMessageToClient* generated_message = generated_response.mutable_request_full_message_info_response()->mutable_full_messages()->add_full_message_list();

                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        generated_message,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);

                generated_message->set_return_status(ReturnStatus::SUCCESS);

//                std::cout << "response\n" << response.DebugString() << '\n';
//                std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

                EXPECT_TRUE(
                        google::protobuf::util::MessageDifferencer::Equivalent(
                                response,
                                generated_response
                        )
                );

                break;
            }
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);

    client->sendBiDiMessage(request);

    //sleep while the message processes
    sleepForChatStreamToStart();

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 1);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_SingleOperation, requestMessageUpdate_withDelay) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    auto[text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    long time_in_millis_to_delay = 100;

    std::chrono::milliseconds start_timestamp;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        std::chrono::milliseconds stop_timestamp = getCurrentTimestamp();

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse: {
                EXPECT_GE(stop_timestamp.count() - start_timestamp.count(), time_in_millis_to_delay);

                num_request_full_message_info_received++;

                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

                ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

                EXPECT_NE(message_doc.id, "");

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                grpc_stream_chat::ChatToClientResponse generated_response;

                generated_response.mutable_request_full_message_info_response()->set_request_status(
                        grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST);
                generated_response.mutable_request_full_message_info_response()->set_chat_room_id(chat_room_id);

                ChatMessageToClient* generated_message = generated_response.mutable_request_full_message_info_response()->mutable_full_messages()->add_full_message_list();

                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        generated_message,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);

                generated_message->set_return_status(ReturnStatus::SUCCESS);

                //                std::cout << "response\n" << response.DebugString() << '\n';
                //                std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

                EXPECT_TRUE(
                        google::protobuf::util::MessageDifferencer::Equivalent(
                                response,
                                generated_response
                        )
                );

                break;
            }
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    grpc_stream_chat::ChatToServerRequest request;
    request.set_testing_time_for_request_to_run_in_millis(time_in_millis_to_delay);
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);

    start_timestamp = getCurrentTimestamp();
    client->sendBiDiMessage(request);

    //sleep while the message processes
    sleepForChatStreamToStart(time_in_millis_to_delay + 100);

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 1);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_SingleOperation, returnMessageFromInjection) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

//        std::cout << std::string("Received message response type: ").
//        append(convertServerResponseCaseToString(response.server_response_case())).
//        append("\n");
//
//        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                EXPECT_FALSE(response.return_new_chat_message().messages_list().empty());
                if (!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(response.return_new_chat_message().messages_list()[0].message_uuid(),
                              generated_message_uuid);
                }

                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    sleepForChatStreamToStart();

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(generated_message_uuid);

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );

    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 1);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

//It is important that this goes through grpc itself in order to make sure that the 'send
// size' on streams is correct for the server settings.
TEST_F(AsyncServer_SingleOperation, returnMessageFromInjection_largestPossiblekDifferentUser) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    bsoncxx::oid large_generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc large_generated_user_account(large_generated_account_oid);

    generateRandomLargestPossibleUserAccount(large_generated_user_account);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                EXPECT_FALSE(response.return_new_chat_message().messages_list().empty());
                if (!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(
                        response.return_new_chat_message().messages_list()[0].message_uuid(),
                        generated_message_uuid
                    );
                }
                //This message will only return the thumbnail, so it will only be
                // ~server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES in size.
                std::cout << "response.ByteSizeLong(): " << response.ByteSizeLong() << '\n';
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    sleepForChatStreamToStart();

    {
        //object must go out of scope in order to release reference counter
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

        auto message = response->mutable_return_new_chat_message()->add_messages_list();

        message->set_message_uuid(generated_message_uuid);
        message->set_sent_by_account_id(large_generated_account_oid.to_string());
        message->set_timestamp_stored(1234);

        StandardChatMessageInfo* standardChatMessageInfo = message->mutable_message()->mutable_standard_message_info();
        standardChatMessageInfo->set_chat_room_id_message_sent_from(generateRandomChatRoomId());
        standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
        standardChatMessageInfo->set_message_has_complete_info(true);

        auto different_user_joined_chat_room_chat_message = message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

        different_user_joined_chat_room_chat_message->mutable_member_info()->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);
        different_user_joined_chat_room_chat_message->mutable_member_info()->set_account_last_activity_time(456);

        stream_container_object->ptr()->injectStreamResponse(
                buildDifferentUserMessageForReceivingUser(
                        response,
                        false
                ),
                stream_container_object
        );
    }

    //sleep while the message processes
    sleepForChatStreamToStart(450);

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 1);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_SingleOperation, returnMetaDataInvalidCredentials) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

//        std::cout << std::string("Received message response type: ").
//        append(convertServerResponseCaseToString(response.server_response_case())).
//        append("\n");
//
//        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    std::string invalid_login_string = "123";

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            invalid_login_string
    );

    sleepForChatStreamToStart();

    EXPECT_EQ(num_initial_connection_primer_received, 0);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 0);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::RETURN_STATUS_ATTACHED);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(AsyncServer_SingleOperation, returnMetaDataStreamTimeOut) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    const std::chrono::milliseconds original_time_chat_stream_stays_active = chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE;
    const std::chrono::milliseconds original_time_chat_stream_refresh_allowed = chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED;

    chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = std::chrono::milliseconds{100L};
    chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = std::chrono::milliseconds{95L};

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    //sleep while the message processes
    //NOTE: This must be LONGER than the new value of TIME_CHAT_STREAM_STAYS_ACTIVE
    sleepForChatStreamToStart(100);

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons, grpc_stream_chat::StreamDownReasons::STREAM_TIMED_OUT);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = original_time_chat_stream_stays_active;
    chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = original_time_chat_stream_refresh_allowed;

}

TEST_F(AsyncServer_SingleOperation, returnMetaDataCanceledByAnotherStream) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    //sleep while the stream initializes
    sleepForChatStreamToStart();

    std::shared_ptr<TestingClient> client_two = std::make_shared<TestingClient>();

    std::jthread bi_di_thread_two = startBiDiStream(
            client_two,
            generated_user_account,
            callback_when_message_received);

    //sleep while the stream initializes
    sleepForChatStreamToStart();

    EXPECT_EQ(num_initial_connection_primer_received, 2);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 2);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    //make sure original bi_di thread ends on its own
    bi_di_thread.join();

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread_two.join();

    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::STREAM_CANCELED_BY_ANOTHER_STREAM);
    EXPECT_EQ(client->trailing_meta_data_optional_info, generated_user_account.installation_ids.front());
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    EXPECT_EQ(client_two->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client_two->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client_two->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_SingleOperation, deadlineExceeded) {

    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    const std::chrono::milliseconds deadline_time = std::chrono::milliseconds{SLEEP_TIME_FOR_CHAT_STREAM_TO_START};

    TestingStreamOptions testingStreamOptions;
    testingStreamOptions.setRunDeadline(deadline_time);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            "",
            testingStreamOptions
    );

    sleepForChatStreamToStart(50);

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::StreamDownReasons_INT_MIN_SENTINEL_DO_NOT_USE_);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_SingleOperation, streamCanceled) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    TestingStreamOptions testingStreamOptions;
    testingStreamOptions.setCancelStream();

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            "",
            testingStreamOptions
    );

    sleepForChatStreamToStart(100);

    EXPECT_EQ(num_initial_connection_primer_received, 0);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 0);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::StreamDownReasons_INT_MIN_SENTINEL_DO_NOT_USE_);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_SingleOperation, droppedChannel) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

//        std::cout << std::string("Received message response type: ").
//        append(convertServerResponseCaseToString(response.server_response_case())).
//        append("\n");
//
//        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    TestingStreamOptions testingStreamOptions;
    testingStreamOptions.setDropChannel();

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            "",
            testingStreamOptions
    );

    sleepForChatStreamToStart(100);

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_SingleOperation, duplicateMessageTimestamps) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    std::array<std::string, 2> message_uuids{
        generateUUID(),
        generateUUID()
    };

    generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            message_uuids[0],
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
                    );

    generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            message_uuids[1],
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
                    );

    //Allow the messages to be inserted and timestamps generated BEFORE the generated timestamp.
    std::this_thread::sleep_for(std::chrono::milliseconds{5});

    const std::chrono::milliseconds synthetic_timestamp_created = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_collection = chat_rooms_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    auto update_result = chat_room_collection.update_many(
            document{}
                << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kTextMessage
            << finalize,
            document{}
                << "$set" << open_document
                    << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{synthetic_timestamp_created}
                << close_document
            << finalize
            );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
                    const grpc_stream_chat::ChatToClientResponse& response) {
        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                EXPECT_EQ(response.initial_connection_messages_response().messages_list().size(), 3);
                if (response.initial_connection_messages_response().messages_list().size() > 2) {
                    EXPECT_EQ(response.initial_connection_messages_response().messages_list(2).message_uuid(),
                              message_uuids[1]);
                }
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(synthetic_timestamp_created.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(synthetic_timestamp_created.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(message_uuids[0]).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string);

    sleepForChatStreamToStart();

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_SingleOperation, metaDataAcceptableSize) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    auto[text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse: {
                num_request_full_message_info_received++;

                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

                ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

                EXPECT_NE(message_doc.id, "");

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                grpc_stream_chat::ChatToClientResponse generated_response;

                generated_response.mutable_request_full_message_info_response()->set_request_status(
                        grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST);
                generated_response.mutable_request_full_message_info_response()->set_chat_room_id(chat_room_id);

                ChatMessageToClient* generated_message = generated_response.mutable_request_full_message_info_response()->mutable_full_messages()->add_full_message_list();

                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        generated_message,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);

                generated_message->set_return_status(ReturnStatus::SUCCESS);

//                std::cout << "response\n" << response.DebugString() << '\n';
//                std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

                EXPECT_TRUE(
                        google::protobuf::util::MessageDifferencer::Equivalent(
                                response,
                                generated_response
                        )
                );

                break;
            }
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::string chat_rooms;
    int num_chat_rooms = 0;
    while(true) {
        if(chat_rooms_string.size() + chat_rooms.size() >= grpc_values::MAXIMUM_META_DATA_SIZE_TO_SEND_FROM_CLIENT) {
            break;
        }
        chat_rooms
                .append(generateUUID())
                .append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);
        num_chat_rooms++;
    }

    chat_rooms_string.append(std::to_string(num_chat_rooms))
        .append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER)
        .append(chat_rooms);

    std::cout << "chat_rooms_string.size(): " << chat_rooms_string.size() << '\n';

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);

    client->sendBiDiMessage(request);

    //sleep while the message processes
    sleepForChatStreamToStart(2000);

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 1);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_SingleOperation, metaDataOverMaxSize) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    auto[text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse: {
                num_request_full_message_info_received++;

                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

                ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

                EXPECT_NE(message_doc.id, "");

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                grpc_stream_chat::ChatToClientResponse generated_response;

                generated_response.mutable_request_full_message_info_response()->set_request_status(
                        grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST);
                generated_response.mutable_request_full_message_info_response()->set_chat_room_id(chat_room_id);

                ChatMessageToClient* generated_message = generated_response.mutable_request_full_message_info_response()->mutable_full_messages()->add_full_message_list();

                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        generated_message,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);

                generated_message->set_return_status(ReturnStatus::SUCCESS);

//                std::cout << "response\n" << response.DebugString() << '\n';
//                std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

                EXPECT_TRUE(
                        google::protobuf::util::MessageDifferencer::Equivalent(
                                response,
                                generated_response
                        )
                );

                break;
            }
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::string chat_rooms;
    int num_chat_rooms = 0;
    //guarantee that the metadata string is too large
    while(chat_rooms_string.size() + chat_rooms.size() < grpc_values::MAXIMUM_RECEIVING_META_DATA_SIZE) {
        chat_rooms
                .append(generateUUID())
                .append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);
        num_chat_rooms++;
    }

    chat_rooms_string.append(std::to_string(num_chat_rooms))
            .append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER)
            .append(chat_rooms);

    TestingStreamOptions testingStreamOptions;
    testingStreamOptions.setSkipStatusCheck();

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string,
            testingStreamOptions
    );

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);

    client->sendBiDiMessage(request);

    //sleep while the message processes
    sleepForChatStreamToStart(2000);

    EXPECT_EQ(num_initial_connection_primer_received, 0);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 0);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::StreamDownReasons_INT_MIN_SENTINEL_DO_NOT_USE_);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_OperationCombinations, refreshDelayed_refreshNormal) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    sleepForChatStreamToStart();

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();
    request.set_testing_time_for_request_to_run_in_millis(50);

    client->sendBiDiMessage(request);

    grpc_stream_chat::ChatToServerRequest request_two;
    request_two.mutable_refresh_chat_stream();

    client->sendBiDiMessage(request_two);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 2);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, refreshDelayed_requestMessageNormal) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    auto[text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse: {
                num_request_full_message_info_received++;

                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

                ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

                EXPECT_NE(message_doc.id, "");

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                grpc_stream_chat::ChatToClientResponse generated_response;

                generated_response.mutable_request_full_message_info_response()->set_request_status(
                        grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST);
                generated_response.mutable_request_full_message_info_response()->set_chat_room_id(chat_room_id);

                ChatMessageToClient* generated_message = generated_response.mutable_request_full_message_info_response()->mutable_full_messages()->add_full_message_list();

                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        generated_message,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);

                generated_message->set_return_status(ReturnStatus::SUCCESS);

                std::cout << "response\n" << response.DebugString() << '\n';
                std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

                EXPECT_TRUE(
                        google::protobuf::util::MessageDifferencer::Equivalent(
                                response,
                                generated_response
                        )
                );

                break;
            }
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    //sleep while the message processes
    sleepForChatStreamToStart();

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();
    request.set_testing_time_for_request_to_run_in_millis(50);

    client->sendBiDiMessage(request);

    grpc_stream_chat::ChatToServerRequest request_two;
    request_two.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request_two.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);

    client->sendBiDiMessage(request_two);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 1);
    EXPECT_EQ(num_refresh_chat_stream_received, 1);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, refreshDelayed_endStream) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    sleepForChatStreamToStart();

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();
    request.set_testing_time_for_request_to_run_in_millis(200);

    client->sendBiDiMessage(request);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, refreshDelayed_injectMessageNormal) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                EXPECT_FALSE(response.return_new_chat_message().messages_list().empty());
                if (!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(response.return_new_chat_message().messages_list()[0].message_uuid(),
                              generated_message_uuid);
                }

                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    sleepForChatStreamToStart();

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();
    request.set_testing_time_for_request_to_run_in_millis(50);

    client->sendBiDiMessage(request);

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(generated_message_uuid);

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );

    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 1);
    EXPECT_EQ(num_return_new_chat_message_received, 1);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, requestMessageDelayed_refreshNormal) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    auto[text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse: {
                num_request_full_message_info_received++;

                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

                ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

                EXPECT_NE(message_doc.id, "");

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                grpc_stream_chat::ChatToClientResponse generated_response;

                generated_response.mutable_request_full_message_info_response()->set_request_status(
                        grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST);
                generated_response.mutable_request_full_message_info_response()->set_chat_room_id(chat_room_id);

                ChatMessageToClient* generated_message = generated_response.mutable_request_full_message_info_response()->mutable_full_messages()->add_full_message_list();

                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        generated_message,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);

                generated_message->set_return_status(ReturnStatus::SUCCESS);

                //                std::cout << "response\n" << response.DebugString() << '\n';
                //                std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

                EXPECT_TRUE(
                        google::protobuf::util::MessageDifferencer::Equivalent(
                                response,
                                generated_response
                        )
                );

                break;
            }
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    //sleep while the message processes
    sleepForChatStreamToStart();

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    request.set_testing_time_for_request_to_run_in_millis(50);

    client->sendBiDiMessage(request);

    grpc_stream_chat::ChatToServerRequest request_two;
    request_two.mutable_refresh_chat_stream();

    client->sendBiDiMessage(request_two);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{SLEEP_TIME_FOR_CHAT_STREAM_TO_START + 150});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 1);
    EXPECT_EQ(num_refresh_chat_stream_received, 1);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, requestMessageDelayed_requestMessageNormal) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    const std::string second_text_message_uuid = generateUUID();

    generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            second_text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received_error = 0;
    int num_request_full_message_info_received_no_error = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse: {

                if (
                        response.request_full_message_info_response().has_error_messages()
                        && !response.request_full_message_info_response().error_messages().message_uuid_list().empty()
                        ) {

                    num_request_full_message_info_received_error++;

                    EXPECT_EQ(
                            response.request_full_message_info_response().error_messages().message_uuid_list()[0],
                            second_text_message_uuid
                    );

                    EXPECT_EQ(
                            response.request_full_message_info_response().request_status(),
                            grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus::RequestFullMessageInfoResponse_RequestStatus_CURRENTLY_PROCESSING_UPDATE_REQUEST
                    );
                } else if (
                        response.request_full_message_info_response().has_full_messages()
                        && !response.request_full_message_info_response().full_messages().full_message_list().empty()
                        ) {

                    num_request_full_message_info_received_no_error++;

                    EXPECT_EQ(
                            response.request_full_message_info_response().full_messages().full_message_list()[0].message_uuid(),
                            text_message_uuid
                    );

                    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

                    ChatRoomMessageDoc message_doc(
                            response.request_full_message_info_response().full_messages().full_message_list()[0].message_uuid(),
                            chat_room_id
                    );

                    EXPECT_NE(message_doc.id, "");

                    bsoncxx::builder::stream::document message_doc_builder;
                    message_doc.convertToDocument(message_doc_builder);

                    ExtractUserInfoObjects extractUserInfoObjects(
                            mongo_cpp_client, accounts_db, user_accounts_collection
                    );

                    grpc_stream_chat::ChatToClientResponse generated_response;

                    generated_response.mutable_request_full_message_info_response()->set_request_status(
                            grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST);
                    generated_response.mutable_request_full_message_info_response()->set_chat_room_id(chat_room_id);

                    ChatMessageToClient* generated_message = generated_response.mutable_request_full_message_info_response()->mutable_full_messages()->add_full_message_list();

                    bool return_val = convertChatMessageDocumentToChatMessageToClient(
                            message_doc_builder.view(),
                            chat_room_id,
                            generated_account_oid.to_string(),
                            false,
                            generated_message,
                            AmountOfMessage::COMPLETE_MESSAGE_INFO,
                            DifferentUserJoinedChatRoomAmount::SKELETON,
                            false,
                            &extractUserInfoObjects
                    );

                    EXPECT_TRUE(return_val);

                    generated_message->set_return_status(ReturnStatus::SUCCESS);

                    //                std::cout << "response\n" << response.DebugString() << '\n';
                    //                std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

                    EXPECT_TRUE(
                            google::protobuf::util::MessageDifferencer::Equivalent(
                                    response,
                                    generated_response
                            )
                    );
                } else {
                    std::cout << response.DebugString() << '\n';
                    EXPECT_TRUE(false);
                }
                break;
            }
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    //sleep while the message processes
    sleepForChatStreamToStart();

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    request.set_testing_time_for_request_to_run_in_millis(50);

    client->sendBiDiMessage(request);

    grpc_stream_chat::ChatToServerRequest request_two;
    request_two.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto second_messages_list_request = request_two.mutable_request_full_message_info()->add_message_uuid_list();
    second_messages_list_request->set_message_uuid(second_text_message_uuid);
    second_messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);

    client->sendBiDiMessage(request_two);

    //sleep while the message processes
    sleepForChatStreamToStart(150);

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received_no_error, 1);
    EXPECT_EQ(num_request_full_message_info_received_error, 1);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, requestMessageDelayed_endStream) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    //sleep while the message processes
    sleepForChatStreamToStart();

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    request.set_testing_time_for_request_to_run_in_millis(100);

    client->sendBiDiMessage(request);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, requestMessageDelayed_injectMessageNormal) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                EXPECT_FALSE(response.return_new_chat_message().messages_list().empty());
                if (!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(response.return_new_chat_message().messages_list()[0].message_uuid(),
                              generated_message_uuid);
                }

                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    sleepForChatStreamToStart();

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    request.set_testing_time_for_request_to_run_in_millis(50);

    client->sendBiDiMessage(request);

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(generated_message_uuid);

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );
    }

    //sleep while the message processes
    sleepForChatStreamToStart(50);

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 1);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 1);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, injectMessageDelayed_refreshNormal) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response
    ) {

//        std::cout << std::string("Received message response type: ").
//        append(convertServerResponseCaseToString(response.server_response_case())).
//        append("\n");
//
//        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                EXPECT_TRUE(response.refresh_chat_stream_response().successfully_refreshed_time());
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                EXPECT_FALSE(response.return_new_chat_message().messages_list().empty());
                if (!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(response.return_new_chat_message().messages_list()[0].message_uuid(),
                              generated_message_uuid);
                }

                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    sleepForChatStreamToStart();

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::this_thread::sleep_for(std::chrono::milliseconds{50});

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(generated_message_uuid);

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );
    }

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();

    client->sendBiDiMessage(request);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 1);
    EXPECT_EQ(num_return_new_chat_message_received, 1);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, injectMessageDelayed_requestMessageNormal) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    auto[text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response
    ) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse: {
                num_request_full_message_info_received++;

                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

                ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

                EXPECT_NE(message_doc.id, "");

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                grpc_stream_chat::ChatToClientResponse generated_response;

                generated_response.mutable_request_full_message_info_response()->set_request_status(
                        grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST);
                generated_response.mutable_request_full_message_info_response()->set_chat_room_id(chat_room_id);

                ChatMessageToClient* generated_message = generated_response.mutable_request_full_message_info_response()->mutable_full_messages()->add_full_message_list();

                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        generated_message,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);

                generated_message->set_return_status(ReturnStatus::SUCCESS);

                //                std::cout << "response\n" << response.DebugString() << '\n';
                //                std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

                EXPECT_TRUE(
                        google::protobuf::util::MessageDifferencer::Equivalent(
                                response,
                                generated_response
                        )
                );

                break;
            }
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                EXPECT_FALSE(response.return_new_chat_message().messages_list().empty());
                if (!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(response.return_new_chat_message().messages_list()[0].message_uuid(),
                              generated_message_uuid);
                }
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    sleepForChatStreamToStart();

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::this_thread::sleep_for(std::chrono::milliseconds{50});

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(generated_message_uuid);

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );
    }

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);

    client->sendBiDiMessage(request);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{SLEEP_TIME_FOR_CHAT_STREAM_TO_START + 50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 1);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 1);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, injectMessageDelayed_endStream) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response
    ) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    sleepForChatStreamToStart();

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::this_thread::sleep_for(std::chrono::milliseconds{100});

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(generateUUID());

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, injectMessageDelayed_injectMessageNormal) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string first_generated_message_uuid = generateUUID();
    const std::string second_generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response
    ) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage: {
                num_return_new_chat_message_received++;

                EXPECT_FALSE(response.return_new_chat_message().messages_list().empty());
                if (!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_TRUE(
                            response.return_new_chat_message().messages_list()[0].message_uuid() ==
                            first_generated_message_uuid
                            || response.return_new_chat_message().messages_list()[0].message_uuid() ==
                               second_generated_message_uuid
                    );
                }
                break;
            }
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    sleepForChatStreamToStart();

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::this_thread::sleep_for(std::chrono::milliseconds{50});

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(first_generated_message_uuid);

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );
    }

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(second_generated_message_uuid);

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 2);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_OperationCombinations, injectMessage_duringInitialize) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response
    ) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                EXPECT_FALSE(response.return_new_chat_message().messages_list().empty());
                if (!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(response.return_new_chat_message().messages_list()[0].message_uuid(),
                              generated_message_uuid);
                }

                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };


    ChatStreamContainerObject::time_for_initialize_to_delay = std::chrono::milliseconds{50};

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    sleepForChatStreamToStart();

    ChatStreamContainerObject::time_for_initialize_to_delay = std::chrono::milliseconds{0};

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(generated_message_uuid);

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 1);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, startNewStreamWithSameOid_duringInitialize) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

    int num_messages_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse&
    ) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        num_messages_received++;
    };


    ChatStreamContainerObject::time_for_initialize_to_delay = std::chrono::milliseconds{50};

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    //sleep while the chat stream starts
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    ChatStreamContainerObject::time_for_initialize_to_delay = std::chrono::milliseconds{0};

    int second_num_initial_connection_primer_received = 0;
    int second_num_initial_connection_messages_received = 0;
    int second_num_initial_connection_messages_completed_received = 0;
    int second_num_request_full_message_info_received = 0;
    int second_num_refresh_chat_stream_received = 0;
    int second_num_return_new_chat_message_received = 0;
    int second_num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> second_callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response
    ) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                second_num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                second_num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                second_num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                second_num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                second_num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                second_num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                second_num_server_response_not_set_received++;
                break;
        }
    };

    std::shared_ptr<TestingClient> client_two = std::make_shared<TestingClient>();

    std::jthread bi_di_thread_two = startBiDiStream(
            client_two,
            generated_user_account,
            second_callback_when_message_received);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    EXPECT_EQ(num_messages_received, 0);

    EXPECT_EQ(second_num_initial_connection_primer_received, 1);
    EXPECT_EQ(second_num_initial_connection_messages_received, 0);
    EXPECT_EQ(second_num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(second_num_request_full_message_info_received, 0);
    EXPECT_EQ(second_num_refresh_chat_stream_received, 0);
    EXPECT_EQ(second_num_return_new_chat_message_received, 0);
    EXPECT_EQ(second_num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();
    bi_di_thread_two.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::STREAM_CANCELED_BY_ANOTHER_STREAM);
    EXPECT_EQ(client->trailing_meta_data_optional_info, generated_user_account.installation_ids.front());
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client_two->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client_two->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client_two->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, injectMessage_thenStartNewStreamWithSameOid_duringInitialize) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

    int num_messages_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse&
    ) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        num_messages_received++;
    };


    ChatStreamContainerObject::time_for_initialize_to_delay = std::chrono::milliseconds{50};

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received);

    //sleep while the chat stream starts
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    ChatStreamContainerObject::time_for_initialize_to_delay = std::chrono::milliseconds{0};

    int second_num_initial_connection_primer_received = 0;
    int second_num_initial_connection_messages_received = 0;
    int second_num_initial_connection_messages_completed_received = 0;
    int second_num_request_full_message_info_received = 0;
    int second_num_refresh_chat_stream_received = 0;
    int second_num_return_new_chat_message_received = 0;
    int second_num_server_response_not_set_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> second_callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response
    ) {
        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                second_num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                second_num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                second_num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                second_num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                second_num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                second_num_return_new_chat_message_received++;
                EXPECT_FALSE(response.return_new_chat_message().messages_list().empty());
                if (!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(response.return_new_chat_message().messages_list()[0].message_uuid(),
                              generated_message_uuid);
                }
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                second_num_server_response_not_set_received++;
                break;
        }
    };

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto message = response->mutable_return_new_chat_message()->add_messages_list();
            message->set_message_uuid(generated_message_uuid);

            reply_vector.emplace_back(std::move(response));
        };

        stream_container_object->ptr()->injectStreamResponse(
                function,
                stream_container_object
        );

        //wait to make sure message has been injected
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }

    std::shared_ptr<TestingClient> client_two = std::make_shared<TestingClient>();

    std::jthread bi_di_thread_two = startBiDiStream(
            client_two,
            generated_user_account,
            second_callback_when_message_received);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    EXPECT_EQ(num_messages_received, 0);

    EXPECT_EQ(second_num_initial_connection_primer_received, 1);
    EXPECT_EQ(second_num_initial_connection_messages_received, 0);
    EXPECT_EQ(second_num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(second_num_request_full_message_info_received, 0);
    EXPECT_EQ(second_num_refresh_chat_stream_received, 0);
    EXPECT_EQ(second_num_return_new_chat_message_received, 0);
    EXPECT_EQ(second_num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();
    bi_di_thread_two.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::STREAM_CANCELED_BY_ANOTHER_STREAM);
    EXPECT_EQ(client->trailing_meta_data_optional_info, generated_user_account.installation_ids.front());
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client_two->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client_two->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client_two->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_OperationCombinations, requestMessage_thenStartNewStreamWithSameOid_duringInitialize) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
    );

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response
    ) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    ChatStreamContainerObject::time_for_initialize_to_delay = std::chrono::milliseconds{50};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );
    //sleep while the chat stream starts
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    ChatStreamContainerObject::time_for_initialize_to_delay = std::chrono::milliseconds{0};

    int second_num_initial_connection_primer_received = 0;
    int second_num_initial_connection_messages_received = 0;
    int second_num_initial_connection_messages_completed_received = 0;
    int second_num_request_full_message_info_received = 0;
    int second_num_refresh_chat_stream_received = 0;
    int second_num_return_new_chat_message_received = 0;
    int second_num_server_response_not_set_received = 0;

    const std::string generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> second_callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response
    ) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                second_num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                second_num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                second_num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                second_num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                second_num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                second_num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                second_num_server_response_not_set_received++;
                break;
        }
    };

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(text_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);

    client->sendBiDiMessage(request);

    //wait to make sure message was received
    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    std::shared_ptr<TestingClient> client_two = std::make_shared<TestingClient>();

    std::jthread bi_di_thread_two = startBiDiStream(
            client_two,
            generated_user_account,
            second_callback_when_message_received,
            chat_rooms_string
    );

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    EXPECT_EQ(num_initial_connection_primer_received, 0);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 0);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    EXPECT_EQ(second_num_initial_connection_primer_received, 1);
    EXPECT_EQ(second_num_initial_connection_messages_received, 1);
    EXPECT_EQ(second_num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(second_num_request_full_message_info_received, 0);
    EXPECT_EQ(second_num_refresh_chat_stream_received, 0);
    EXPECT_EQ(second_num_return_new_chat_message_received, 0);
    EXPECT_EQ(second_num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    bi_di_thread.join();
    bi_di_thread_two.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::STREAM_CANCELED_BY_ANOTHER_STREAM);
    EXPECT_EQ(client->trailing_meta_data_optional_info, generated_user_account.installation_ids.front());
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client_two->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client_two->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client_two->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, injectMessagesWithDelay_streamTimeout) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string first_generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    const std::chrono::milliseconds original_time_chat_stream_stays_active = chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE;
    const std::chrono::milliseconds original_time_chat_stream_refresh_allowed = chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED;

    long function_pause_time = 50;
    int number_injects = 5;

    chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = std::chrono::milliseconds{function_pause_time - 10};
    chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = std::chrono::milliseconds{
            (long) ((double) (function_pause_time - 10) * .95)};

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        for (int i = 0; i < number_injects; i++) {
            auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

                //sleep while the chat stream starts (required for this test to extract from user_open_chat_streams)
                std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time});

                std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

                auto message = response->mutable_return_new_chat_message()->add_messages_list();
                message->set_message_uuid(i == 0 ? first_generated_message_uuid : generateUUID());

                reply_vector.emplace_back(std::move(response));
            };

            stream_container_object->ptr()->injectStreamResponse(
                    function,
                    stream_container_object
            );
        }
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{150});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{function_pause_time * number_injects + 50});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons, grpc_stream_chat::StreamDownReasons::STREAM_TIMED_OUT);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = original_time_chat_stream_stays_active;
    chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = original_time_chat_stream_refresh_allowed;

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, requestMessagesWithDelay_streamTimeout) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    std::vector<std::string> message_uuid_values;
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    for (int i = 0; i < 5; i++) {

        const std::string text_message_uuid = generateUUID();

        auto[text_message_request, text_message_response] = generateRandomTextMessage(
                generated_account_oid,
                generated_user_account.logged_in_token,
                generated_user_account.installation_ids.front(),
                chat_room_id,
                text_message_uuid,
                gen_random_alpha_numeric_string(
                        rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
        );

        message_uuid_values.emplace_back(text_message_uuid);
    }

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    const std::chrono::milliseconds original_time_chat_stream_stays_active = chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE;
    const std::chrono::milliseconds original_time_chat_stream_refresh_allowed = chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED;

    long function_pause_time = 50;

    chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = std::chrono::milliseconds{function_pause_time - 10};
    chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = std::chrono::milliseconds{
            (long) ((double) (function_pause_time - 10) * .95)};

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    for (const auto& message_uuid_value : message_uuid_values) {
        grpc_stream_chat::ChatToServerRequest request;
        request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
        auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
        messages_list_request->set_message_uuid(message_uuid_value);
        messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);
        request.set_testing_time_for_request_to_run_in_millis(function_pause_time);

        client->sendBiDiMessage(request);
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time + 50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(
            std::chrono::milliseconds{function_pause_time * message_uuid_values.size() + 50});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons, grpc_stream_chat::StreamDownReasons::STREAM_TIMED_OUT);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = original_time_chat_stream_stays_active;
    chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = original_time_chat_stream_refresh_allowed;
}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, refreshWithDelay_streamTimeout) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::chrono::milliseconds original_time_chat_stream_stays_active = chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE;
    const std::chrono::milliseconds original_time_chat_stream_refresh_allowed = chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED;

    long function_pause_time = 50;

    chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = std::chrono::milliseconds{function_pause_time - 10};
    chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = std::chrono::milliseconds{
            (long) ((double) (function_pause_time - 10) * .95)};

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();
    request.set_testing_time_for_request_to_run_in_millis(function_pause_time);

    client->sendBiDiMessage(request);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(
            std::chrono::milliseconds{function_pause_time + 50});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons, grpc_stream_chat::StreamDownReasons::STREAM_TIMED_OUT);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = original_time_chat_stream_stays_active;
    chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = original_time_chat_stream_refresh_allowed;

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, injectMessagesWithDelay_shutdownServer) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string first_generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    long function_pause_time = 100;
    int number_injects = 5;
    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string()
        );

        for (int i = 0; i < number_injects; i++) {
            auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

                //sleep while the chat stream starts (required for this test to extract from user_open_chat_streams)
                std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time});

                std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

                auto message = response->mutable_return_new_chat_message()->add_messages_list();
                message->set_message_uuid(i == 0 ? first_generated_message_uuid : generateUUID());

                reply_vector.emplace_back(std::move(response));
            };

            stream_container_object->ptr()->injectStreamResponse(
                    function,
                    stream_container_object
            );
        }
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    //cleanup calls each function before it completes, this means that it must wait for the functions to complete before it ends.
    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{function_pause_time * number_injects + 50});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, requestMessagesWithDelay_shutdownServer) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    std::vector<std::string> message_uuid_values;
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    for (int i = 0; i < 5; i++) {

        const std::string text_message_uuid = generateUUID();

        auto[text_message_request, text_message_response] = generateRandomTextMessage(
                generated_account_oid,
                generated_user_account.logged_in_token,
                generated_user_account.installation_ids.front(),
                chat_room_id,
                text_message_uuid,
                gen_random_alpha_numeric_string(
                        rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
        );

        message_uuid_values.emplace_back(text_message_uuid);
    }

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    long function_pause_time = 50;

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    for (const auto& message_uuid_value : message_uuid_values) {
        grpc_stream_chat::ChatToServerRequest request;
        request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
        auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
        messages_list_request->set_message_uuid(message_uuid_value);
        messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);
        request.set_testing_time_for_request_to_run_in_millis(function_pause_time);

        client->sendBiDiMessage(request);
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time - 10});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(
            std::chrono::milliseconds{function_pause_time * message_uuid_values.size() + 50});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, refreshWithDelay_shutdownServer) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    long function_pause_time = 50;

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();
    request.set_testing_time_for_request_to_run_in_millis(function_pause_time);

    client->sendBiDiMessage(request);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time / 2});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(
            std::chrono::milliseconds{function_pause_time + 50});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, injectMessagesWithDelay_endStreamHasBeenCalled) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string first_generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    long function_pause_time = 50;
    int number_injects = 5;
    const std::string optional_info = "optional_info";
    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        for (int i = 0; i < number_injects; i++) {
            auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

                //sleep while the chat stream starts (required for this test to extract from user_open_chat_streams)
                std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time});

                std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

                auto message = response->mutable_return_new_chat_message()->add_messages_list();
                message->set_message_uuid(i == 0 ? first_generated_message_uuid : generateUUID());

                reply_vector.emplace_back(std::move(response));
            };

            stream_container_object->ptr()->injectStreamResponse(
                    function,
                    stream_container_object
            );
        }

        stream_container_object->ptr()->endStream(
                grpc_stream_chat::StreamDownReasons::UNKNOWN_STREAM_STOP_REASON,
                optional_info
        );
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time - 10});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{function_pause_time * number_injects + 100});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::UNKNOWN_STREAM_STOP_REASON);
    EXPECT_EQ(client->trailing_meta_data_optional_info, optional_info);
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, requestMessagesWithDelay_endStreamHasBeenCalled) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    std::vector<std::string> message_uuid_values;
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    for (int i = 0; i < 5; i++) {

        const std::string text_message_uuid = generateUUID();

        auto[text_message_request, text_message_response] = generateRandomTextMessage(
                generated_account_oid,
                generated_user_account.logged_in_token,
                generated_user_account.installation_ids.front(),
                chat_room_id,
                text_message_uuid,
                gen_random_alpha_numeric_string(
                        rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
        );

        message_uuid_values.emplace_back(text_message_uuid);
    }

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    long function_pause_time = 50;

    const std::string optional_info = "optional_info";

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    //sleep while stream starts
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    for (const auto& message_uuid_value : message_uuid_values) {
        grpc_stream_chat::ChatToServerRequest request;
        request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
        auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
        messages_list_request->set_message_uuid(message_uuid_value);
        messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);
        request.set_testing_time_for_request_to_run_in_millis(function_pause_time);

        client->sendBiDiMessage(request);
    }

    //sleep while messages process
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        stream_container_object->ptr()->endStream(
                grpc_stream_chat::StreamDownReasons::UNKNOWN_STREAM_STOP_REASON,
                optional_info
        );
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time + 50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 1);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(
            std::chrono::milliseconds{function_pause_time * message_uuid_values.size() + 50});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::UNKNOWN_STREAM_STOP_REASON);
    EXPECT_EQ(client->trailing_meta_data_optional_info, optional_info);
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, refreshWithDelay_endStreamHasBeenCalled) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    long function_pause_time = 50;
    const std::string optional_info = "optional_info";

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();
    request.set_testing_time_for_request_to_run_in_millis(function_pause_time);

    client->sendBiDiMessage(request);

    //sleep while stream starts
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    {
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string());

        stream_container_object->ptr()->endStream(
                grpc_stream_chat::StreamDownReasons::UNKNOWN_STREAM_STOP_REASON,
                optional_info
        );
    }

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time - 10});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(
            std::chrono::milliseconds{function_pause_time + 50});
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::UNKNOWN_STREAM_STOP_REASON);
    EXPECT_EQ(client->trailing_meta_data_optional_info, optional_info);
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, injectMessagesWithDelay_afterShutdownHasStarted) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string first_generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    std::unique_ptr<std::jthread> shutdown_server = nullptr;

    long function_pause_time = 50;
    int number_injects = 5;
    {
        //must get reference before shutdown is started
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                generated_account_oid.to_string()
        );

        shutdown_server = std::make_unique<std::jthread>(
                [&] {
                    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
                }
        );

        std::this_thread::sleep_for(
                chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING +
                chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME +
                std::chrono::milliseconds{150}
                );

        for (int i = 0; i < number_injects; i++) {
            auto function = [&](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

                //sleep while the chat stream starts (required for this test to extract from user_open_chat_streams)
                std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time});

                std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

                auto message = response->mutable_return_new_chat_message()->add_messages_list();
                message->set_message_uuid(i == 0 ? first_generated_message_uuid : generateUUID());

                reply_vector.emplace_back(std::move(response));
            };

            stream_container_object->ptr()->injectStreamResponse(
                    function,
                    stream_container_object
            );
        }

    }

    std::cout << "Deallocating stream_container_object" << std::endl;

    //sleep while the message processes
    //if concurrency is limited, cleanup may not be done so sleep for a little extra (*2)
    std::this_thread::sleep_for(std::chrono::milliseconds{2 * function_pause_time * number_injects + 50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    shutdown_server->join();
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, requestMessagesWithDelay_afterShutdownHasStarted) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
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

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    std::vector<std::string> message_uuid_values;
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    for (int i = 0; i < 5; i++) {

        const std::string text_message_uuid = generateUUID();

        auto[text_message_request, text_message_response] = generateRandomTextMessage(
                generated_account_oid,
                generated_user_account.logged_in_token,
                generated_user_account.installation_ids.front(),
                chat_room_id,
                text_message_uuid,
                gen_random_alpha_numeric_string(
                        rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1)
        );

        message_uuid_values.emplace_back(text_message_uuid);
    }

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                //this could or could not be received here
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    long function_pause_time = 50;

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received,
            chat_rooms_string
    );

    //allow server to start
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    std::unique_ptr<std::jthread> shutdown_server = std::make_unique<std::jthread>(
            [&] {
                grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
            }
    );

    for (const auto& message_uuid_value : message_uuid_values) {
        grpc_stream_chat::ChatToServerRequest request;
        request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
        auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
        messages_list_request->set_message_uuid(message_uuid_value);
        messages_list_request->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);
        request.set_testing_time_for_request_to_run_in_millis(function_pause_time);

        client->sendBiDiMessage(request);
    }

    //sleep while the message processes (cleanup means all message will run)
    std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time * message_uuid_values.size() + 150});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    server_thread.join();
    bi_di_thread.join();
    shutdown_server->join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, refreshWithDelay_afterShutdownHasStarted) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc generated_user_account(generated_account_oid);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();;

    int num_initial_connection_primer_received = 0;
    int num_initial_connection_messages_received = 0;
    int num_initial_connection_messages_completed_received = 0;
    int num_request_full_message_info_received = 0;
    int num_refresh_chat_stream_received = 0;
    int num_return_new_chat_message_received = 0;
    int num_server_response_not_set_received = 0;

    const std::string first_generated_message_uuid = generateUUID();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        //        std::cout << std::string("Received message response type: ").
        //        append(convertServerResponseCaseToString(response.server_response_case())).
        //        append("\n");
        //
        //        std::cout << response.DebugString() << '\n';

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                num_server_response_not_set_received++;
                break;
        }
    };

    std::jthread bi_di_thread = startBiDiStream(
            client,
            generated_user_account,
            callback_when_message_received
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    std::unique_ptr<std::jthread> shutdown_server = std::make_unique<std::jthread>(
            [&] {
                grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
            }
    );

    long function_pause_time = 50;

    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_refresh_chat_stream();
    request.set_testing_time_for_request_to_run_in_millis(function_pause_time);

    client->sendBiDiMessage(request);

    //sleep while the message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{function_pause_time + 50});

    EXPECT_EQ(num_initial_connection_primer_received, 1);
    EXPECT_EQ(num_initial_connection_messages_received, 0);
    EXPECT_EQ(num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(num_request_full_message_info_received, 0);
    EXPECT_EQ(num_refresh_chat_stream_received, 0);
    EXPECT_EQ(num_return_new_chat_message_received, 0);
    EXPECT_EQ(num_server_response_not_set_received, 0);

    shutdown_server->join();
    server_thread.join();
    bi_di_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);
}

TEST_F(AsyncServer_MultipleItemsInCompletionQueue, messagePassedFromChatChangeStream) {

    const std::chrono::milliseconds original_chat_change_stream_await_time = chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME;
    const std::chrono::milliseconds original_delay_for_message_ordering = chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING;
    const std::chrono::milliseconds original_message_ordering_thread_sleep_time = chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME;

    //NOTE: If this time is too short, it will not work.
    chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = std::chrono::milliseconds{210};

    chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = std::chrono::milliseconds{1};
    chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = std::chrono::milliseconds{1};

    bsoncxx::oid first_generated_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid second_generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc first_generated_user_account(first_generated_account_oid);
    UserAccountDoc second_generated_user_account(second_generated_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            first_generated_account_oid,
            first_generated_user_account.logged_in_token,
            first_generated_user_account.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    join_chat_room_request.set_chat_room_id(create_chat_room_response.chat_room_id());
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_generated_account_oid,
            second_generated_user_account.logged_in_token,
            second_generated_user_account.installation_ids.front()
    );

    auto rw = grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse>();

    joinChatRoom(&join_chat_room_request, &rw);
    ASSERT_FALSE(rw.write_params.empty());
    ASSERT_EQ(rw.write_params.front().msg.chat_room_status(),
              grpc_chat_commands::ChatRoomStatus::SUCCESSFULLY_JOINED);

    std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

    //See inside function for why this needs to be done.
    spinUntilNextSecond();

    //Start chat change stream last AFTER chat room has been created and user's joined it. This way
    // it will not get the new chat room message.
    std::jthread chat_change_stream_thread = std::jthread(
            [&]() {
                beginChatChangeStream();
            }
    );

    std::jthread server_thread = startServer(grpcServerImpl);

    std::shared_ptr<TestingClient> first_client = std::make_shared<TestingClient>();
    std::shared_ptr<TestingClient> second_client = std::make_shared<TestingClient>();

    int first_num_initial_connection_primer_received = 0;
    int first_num_initial_connection_messages_received = 0;
    int first_num_initial_connection_messages_completed_received = 0;
    int first_num_request_full_message_info_received = 0;
    int first_num_refresh_chat_stream_received = 0;
    int first_num_return_new_chat_message_received = 0;
    int first_num_server_response_not_set_received = 0;

    int second_num_initial_connection_primer_received = 0;
    int second_num_initial_connection_messages_received = 0;
    int second_num_initial_connection_messages_completed_received = 0;
    int second_num_request_full_message_info_received = 0;
    int second_num_refresh_chat_stream_received = 0;
    int second_num_return_new_chat_message_received = 0;
    int second_num_server_response_not_set_received = 0;

    const std::string text_message_uuid = generateUUID();
    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> first_callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        std::chrono::milliseconds stop_timestamp = getCurrentTimestamp();

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                first_num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                first_num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                first_num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                first_num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                first_num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
                EXPECT_EQ(response.return_new_chat_message().messages_list_size(),1);
                if(!response.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(
                            response.return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                            MessageSpecifics::MessageBodyCase::kNewUpdateTimeMessage
                    );
                }
                first_num_return_new_chat_message_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                first_num_server_response_not_set_received++;
                break;
        }
    };

    const std::function<void(
            const grpc_stream_chat::ChatToClientResponse& response)> second_callback_when_message_received = [&](
            const grpc_stream_chat::ChatToClientResponse& response) {

        std::chrono::milliseconds stop_timestamp = getCurrentTimestamp();

        switch (response.server_response_case()) {
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
                second_num_initial_connection_primer_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
                second_num_initial_connection_messages_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
                second_num_initial_connection_messages_completed_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
                second_num_request_full_message_info_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
                second_num_refresh_chat_stream_received++;
                break;
            case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage: {
                second_num_return_new_chat_message_received++;

                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

                ChatRoomMessageDoc message_doc(text_message_uuid, chat_room_id);

                EXPECT_NE(message_doc.id, "");

                bsoncxx::builder::stream::document message_doc_builder;
                message_doc.convertToDocument(message_doc_builder);

                ExtractUserInfoObjects extractUserInfoObjects(
                        mongo_cpp_client, accounts_db, user_accounts_collection
                );

                grpc_stream_chat::ChatToClientResponse generated_response;

                ChatMessageToClient* generated_message = generated_response.mutable_return_new_chat_message()->add_messages_list();

                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        message_doc_builder.view(),
                        chat_room_id,
                        second_generated_account_oid.to_string(),
                        false,
                        generated_message,
                        AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                );

                EXPECT_TRUE(return_val);

//                std::cout << "response\n" << response.DebugString() << '\n';
//                std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

                EXPECT_TRUE(
                        google::protobuf::util::MessageDifferencer::Equivalent(
                                response,
                                generated_response
                        )
                );
                break;
            }
            case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
                second_num_server_response_not_set_received++;
                break;
        }
    };

    //NOTE: This actually needs to be done, declaring it in place or using
    // a default function parameter will not work (or a normal 'auto = [](){}');
    std::function<void(grpc_stream_chat::StreamDownReasons)> callback_when_finished = [](
            grpc_stream_chat::StreamDownReasons) {};

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(current_timestamp.count())).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::jthread first_bi_di_thread = startBiDiStream(
            first_client,
            first_generated_user_account,
            first_callback_when_message_received,
            chat_rooms_string
    );

    std::jthread second_bi_di_thread = startBiDiStream(
            second_client,
            second_generated_user_account,
            second_callback_when_message_received,
            chat_rooms_string
    );

    //sleep while the streams start
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    generateRandomTextMessage(
            first_generated_user_account.current_object_oid,
            first_generated_user_account.logged_in_token,
            first_generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                    rand() % 100 +
                    server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE +
                    1)
    );

    //sleep while message processes
    std::this_thread::sleep_for(std::chrono::milliseconds{150});

    EXPECT_EQ(first_num_initial_connection_primer_received, 1);
    EXPECT_EQ(first_num_initial_connection_messages_received, 1);
    EXPECT_EQ(first_num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(first_num_request_full_message_info_received, 0);
    EXPECT_EQ(first_num_refresh_chat_stream_received, 0);
    EXPECT_EQ(first_num_return_new_chat_message_received, 1);
    EXPECT_EQ(first_num_server_response_not_set_received, 0);

    EXPECT_EQ(second_num_initial_connection_primer_received, 1);
    EXPECT_EQ(second_num_initial_connection_messages_received, 1);
    EXPECT_EQ(second_num_initial_connection_messages_completed_received, 1);
    EXPECT_EQ(second_num_request_full_message_info_received, 0);
    EXPECT_EQ(second_num_refresh_chat_stream_received, 0);
    EXPECT_EQ(second_num_return_new_chat_message_received, 1);
    EXPECT_EQ(second_num_server_response_not_set_received, 0);

    grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{100});
    server_thread.join();
    first_bi_di_thread.join();
    second_bi_di_thread.join();
    chat_change_stream_thread.join();

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(first_client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(first_client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(first_client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    //metadata will not be received if something happens like the deadline is exceeded
    EXPECT_EQ(second_client->trailing_meta_data_stream_down_reasons,
              grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    EXPECT_EQ(second_client->trailing_meta_data_optional_info, "~");
    EXPECT_EQ(second_client->trailing_meta_data_return_status, ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_);

    chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = original_chat_change_stream_await_time;
    chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = original_delay_for_message_ordering;
    chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = original_message_ordering_thread_sleep_time;
}
