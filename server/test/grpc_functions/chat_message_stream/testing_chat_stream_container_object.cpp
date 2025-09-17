//
// Created by jeremiah on 7/21/22.
//
#include <fstream>
#include <mongocxx/collection.hpp>
#include <collection_names.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include <send_messages_implementation.h>
#include <chat_stream_container.h>
#include <google/protobuf/util/message_differencer.h>
#include <generate_multiple_random_accounts.h>
#include <account_objects.h>
#include <setup_login_info.h>
#include <generate_random_messages.h>
#include <async_server.h>
#include <start_server.h>
#include <testing_client.h>
#include <connection_pool_global_variable.h>
#include <chat_rooms_objects.h>
#include <start_bi_di_stream.h>
#include <chat_room_message_keys.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"

#include "chat_stream_container_object.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

/**NOTE: Most of the functions inside ChatStreamContainerObject
 * are tested directly or indirectly through the tests inside
 * folder 'test/utility/async_server'. **/

class ChatStreamContainerObjectBasicTests : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

class RequestFullMessageInfoTests : public ::testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection chat_room_collection;

    bsoncxx::oid generated_account_oid;
    UserAccountDoc generated_user_account;

    std::string chat_room_id;

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);
        generated_user_account.getFromCollection(generated_account_oid);

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

        chat_room_id = create_chat_room_response.chat_room_id();
        chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void invalidMessageUuid(int num_messages, int wrong_oid_index) {

        grpc_stream_chat::ChatToServerRequest request;
        request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

        const auto amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

        const std::string invalid_message_uuid = "invalid_uuid";

        for(int i = 0; i < num_messages; ++i) {
            const std::string message_uuid = generateUUID();

            generateRandomTextMessage(
                    generated_account_oid,
                    generated_user_account.logged_in_token,
                    generated_user_account.installation_ids.front(),
                    chat_room_id,
                    message_uuid,
                    gen_random_alpha_numeric_string(
                            rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
                            )
                            );

            auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
            messages_list_request->set_amount_of_messages_to_request(amount_of_message);

            if(i == wrong_oid_index) {
                messages_list_request->set_message_uuid(invalid_message_uuid);
            } else {
                messages_list_request->set_message_uuid(message_uuid);
            }

            //sleep so messages are guaranteed at least 1 ms apart
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        ChatStreamContainerObject chat_stream_container_object;
        chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
        chat_stream_container_object.requestFullMessageInfo(
                request,
                reply_vector
                );

        ASSERT_EQ(reply_vector.size(), 1);
        EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), num_messages);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

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
                << chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kTextMessage
                << finalize,
                opts
                );

        ExtractUserInfoObjects extractUserInfoObjects(
                mongo_cpp_client, accounts_db, user_accounts_collection
                );

        int index = 0;
        for(const bsoncxx::document::view& val : messages) {
            ASSERT_LT(index, reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size());

            ChatMessageToClient response;

            if(index == wrong_oid_index) {
                response.set_return_status(ReturnStatus::VALUE_NOT_SET);
                response.set_message_uuid(invalid_message_uuid);
            } else {
                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        val,
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        &response,
                        amount_of_message,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                        );
                EXPECT_TRUE(return_val);

                response.set_return_status(ReturnStatus::SUCCESS);
            }

            bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                    response,
                    reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[index]
                    );

            if(!equivalent) {
                std::cout << "response\n" << response.DebugString() << '\n';
                std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[index].DebugString() << '\n';
            }

            EXPECT_TRUE(equivalent);

            index++;
        }

    }

    void validMessageUuidMessageDoesNotExist(int num_messages, int wrong_oid_index) {

        grpc_stream_chat::ChatToServerRequest request;
        request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

        const auto amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

        const std::string not_saved_uuid = generateUUID();

        for(int i = 0; i < num_messages; ++i) {
            std::string message_uuid;

            if(i != wrong_oid_index) {
                message_uuid = generateUUID();
                generateRandomTextMessage(
                        generated_account_oid,
                        generated_user_account.logged_in_token,
                        generated_user_account.installation_ids.front(),
                        chat_room_id,
                        message_uuid,
                        gen_random_alpha_numeric_string(
                                rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
                                )
                                );
            } else {
                message_uuid = not_saved_uuid;
            }

            auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
            messages_list_request->set_amount_of_messages_to_request(amount_of_message);

            messages_list_request->set_message_uuid(message_uuid);

            //sleep so messages are guaranteed at least 1 ms apart
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        ChatStreamContainerObject chat_stream_container_object;
        chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
        chat_stream_container_object.requestFullMessageInfo(
                request,
                reply_vector
                );

        ASSERT_EQ(reply_vector.size(), 1);
        EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), num_messages);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

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
                << chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kTextMessage
                << finalize,
                opts
                );

        ExtractUserInfoObjects extractUserInfoObjects(
                mongo_cpp_client, accounts_db, user_accounts_collection
                );

        auto it = messages.begin();
        for(int i = 0; i < num_messages && it != messages.end(); ++i) {

            ASSERT_LT(i, reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size());

            ChatMessageToClient response;

            if(i == wrong_oid_index) {
                response.set_return_status(ReturnStatus::VALUE_NOT_SET);
                response.set_message_uuid(not_saved_uuid);
            } else {
                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        *it,
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        &response,
                        amount_of_message,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                        );
                EXPECT_TRUE(return_val);

                response.set_return_status(ReturnStatus::SUCCESS);
                ++it;
            }

            bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                    response,
                    reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i]
                    );

            if(!equivalent) {
                std::cout << "response\n" << response.DebugString() << '\n';
                std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i].DebugString() << '\n';
            }

            EXPECT_TRUE(equivalent);
        }
    }

    void invalidAmountOfMessage(int num_messages, int wrong_oid_index) {

        grpc_stream_chat::ChatToServerRequest request;
        request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

        const auto amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;
        std::string invalid_amount_message_uuid;

        for(int i = 0; i < num_messages; ++i) {
            const std::string message_uuid = generateUUID();

            generateRandomTextMessage(
                    generated_account_oid,
                    generated_user_account.logged_in_token,
                    generated_user_account.installation_ids.front(),
                    chat_room_id,
                    message_uuid,
                    gen_random_alpha_numeric_string(
                            rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
                            )
                            );

            auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
            messages_list_request->set_message_uuid(message_uuid);

            if(i == wrong_oid_index) {
                invalid_amount_message_uuid = message_uuid;
                messages_list_request->set_amount_of_messages_to_request(AmountOfMessage(-1));
            } else {
                messages_list_request->set_amount_of_messages_to_request(amount_of_message);
            }

            //sleep so messages are guaranteed at least 1 ms apart
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        ChatStreamContainerObject chat_stream_container_object;
        chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
        chat_stream_container_object.requestFullMessageInfo(
                request,
                reply_vector
                );

        ASSERT_EQ(reply_vector.size(), 1);
        EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), num_messages);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

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
                << chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kTextMessage
            << finalize,
            opts
        );

        ExtractUserInfoObjects extractUserInfoObjects(
                mongo_cpp_client, accounts_db, user_accounts_collection
                );

        int index = 0;
        for(const bsoncxx::document::view& val : messages) {
            ASSERT_LT(index, reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size());

            ChatMessageToClient response;

            if(index == wrong_oid_index) {
                response.set_return_status(ReturnStatus::UNKNOWN);
                response.set_message_uuid(invalid_amount_message_uuid);
            } else {
                bool return_val = convertChatMessageDocumentToChatMessageToClient(
                        val,
                        chat_room_id,
                        generated_account_oid.to_string(),
                        false,
                        &response,
                        amount_of_message,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects
                        );
                EXPECT_TRUE(return_val);

                response.set_return_status(ReturnStatus::SUCCESS);
            }

            bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                    response,
                    reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[index]
                    );

            if(!equivalent) {
                std::cout << "response\n" << response.DebugString() << '\n';
                std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[index].DebugString() << '\n';
            }

            EXPECT_TRUE(equivalent);

            index++;
        }

    }

    void duplicateMessageUuidSentSameAmountOfMessage(int num_messages, int duplicate_message_index) {
        grpc_stream_chat::ChatToServerRequest request;
        request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

        const auto amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

        std::string previous_message;
        std::string duplicate_message_uuid;
        for(int i = 0; i < num_messages; ++i) {
            std::string text_message_uuid = generateUUID();
            if(i == duplicate_message_index) {
                text_message_uuid = previous_message;
                duplicate_message_uuid = previous_message;
            }
            else {
                previous_message = text_message_uuid;

                generateRandomTextMessage(
                        generated_account_oid,
                        generated_user_account.logged_in_token,
                        generated_user_account.installation_ids.front(),
                        chat_room_id,
                        text_message_uuid,
                        gen_random_alpha_numeric_string(
                                rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
                                )
                                );
            }

            auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
            previous_message = text_message_uuid;

            messages_list_request->set_message_uuid(text_message_uuid);
            messages_list_request->set_amount_of_messages_to_request(amount_of_message);

            //sleep so messages are guaranteed at least 1 ms apart
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        ChatStreamContainerObject chat_stream_container_object;
        chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
        chat_stream_container_object.requestFullMessageInfo(
                request,
                reply_vector
                );

        ASSERT_EQ(reply_vector.size(), 1);
        EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), num_messages);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

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
                << chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kTextMessage
                << finalize,
                opts
                );

        ExtractUserInfoObjects extractUserInfoObjects(
                mongo_cpp_client, accounts_db, user_accounts_collection
                );

        //uuid->message doc
        std::unordered_map<std::string, bsoncxx::document::value> map_of_messages;

        for(const bsoncxx::document::view& val : messages) {
            map_of_messages.insert(
                    std::pair(
                            val["_id"].get_string().value.to_string(),
                            val
                            )
                            );
        }

        for(int i = 0; i < reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(); ++i) {

            ChatMessageToClient response;

            if(i == duplicate_message_index) {
                response.set_return_status(ReturnStatus::UNKNOWN);
                response.set_message_uuid(duplicate_message_uuid);

                bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i]
                        );

                if(!equivalent) {
                    std::cout << "response\n" << response.DebugString() << '\n';
                    std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i].DebugString() << '\n';
                }

                EXPECT_TRUE(equivalent);
            } else {
                auto map_ptr = map_of_messages.find(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list(i).message_uuid());

                EXPECT_NE(map_ptr, map_of_messages.end());
                if(map_ptr != map_of_messages.end()) {

                    bool return_val = convertChatMessageDocumentToChatMessageToClient(
                            map_ptr->second,
                            chat_room_id,
                            generated_account_oid.to_string(),
                            false,
                            &response,
                            amount_of_message,
                            DifferentUserJoinedChatRoomAmount::SKELETON,
                            false,
                            &extractUserInfoObjects
                            );

                    response.set_return_status(ReturnStatus::SUCCESS);

                    EXPECT_TRUE(return_val);

                    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                            response,
                            reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i]
                            );

                    if(!equivalent) {
                        std::cout << "response\n" << response.DebugString() << '\n';
                        std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i].DebugString() << '\n';
                    }

                    EXPECT_TRUE(equivalent);
                }
            }
        }
    }

    void duplicateMessageUuidSentDifferentAmount(
            int num_messages,
            int duplicate_message_index,
            AmountOfMessage amount_of_message,
            AmountOfMessage duplicate_amount_of_message
            ) {

        grpc_stream_chat::ChatToServerRequest request;
        request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

        int index_set_to_unknown = duplicate_message_index - (amount_of_message < duplicate_amount_of_message);

        std::string previous_message;
        std::string duplicate_message_uuid;
        for(int i = 0; i < num_messages; ++i) {
            std::string text_message_uuid = generateUUID();

            auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();

            if(i == duplicate_message_index) {
                text_message_uuid = previous_message;
                duplicate_message_uuid = previous_message;
                messages_list_request->set_amount_of_messages_to_request(duplicate_amount_of_message);
            }
            else {
                previous_message = text_message_uuid;

                generateRandomTextMessage(
                        generated_account_oid,
                        generated_user_account.logged_in_token,
                        generated_user_account.installation_ids.front(),
                        chat_room_id,
                        text_message_uuid,
                        gen_random_alpha_numeric_string(
                                rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
                                )
                                );
                messages_list_request->set_amount_of_messages_to_request(amount_of_message);
            }

            messages_list_request->set_message_uuid(text_message_uuid);

            //sleep so messages are guaranteed at least 1 ms apart
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        ChatStreamContainerObject chat_stream_container_object;
        chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
        chat_stream_container_object.requestFullMessageInfo(
                request,
                reply_vector
                );

        ASSERT_EQ(reply_vector.size(), 1);
        EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), num_messages);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
        EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

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
                << chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kTextMessage
                << finalize,
                opts
                );

        ExtractUserInfoObjects extractUserInfoObjects(
                mongo_cpp_client, accounts_db, user_accounts_collection
                );

        //uuid->message doc
        std::unordered_map<std::string, bsoncxx::document::value> map_of_messages;

        for(const bsoncxx::document::view& val : messages) {
            map_of_messages.insert(
                    std::pair(
                            val["_id"].get_string().value.to_string(),
                            val
                            )
                            );
        }

        for(int i = 0; i < reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(); ++i) {

            ChatMessageToClient response;

            if(i == index_set_to_unknown) {
                response.set_return_status(ReturnStatus::UNKNOWN);
                response.set_message_uuid(duplicate_message_uuid);

                bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                        response,
                        reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i]
                        );

                if(!equivalent) {
                    std::cout << "response\n" << response.DebugString() << '\n';
                    std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i].DebugString() << '\n';
                }

                EXPECT_TRUE(equivalent);
            } else {
                auto map_ptr = map_of_messages.find(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list(i).message_uuid());

                EXPECT_NE(map_ptr, map_of_messages.end());
                if(map_ptr != map_of_messages.end()) {

                    AmountOfMessage requested_amount;
                    if(i==duplicate_message_index) {
                        requested_amount = duplicate_amount_of_message;
                    } else {
                        requested_amount = amount_of_message;
                    }

                    bool return_val = convertChatMessageDocumentToChatMessageToClient(
                            map_ptr->second,
                            chat_room_id,
                            generated_account_oid.to_string(),
                            false,
                            &response,
                            requested_amount,
                            DifferentUserJoinedChatRoomAmount::SKELETON,
                            false,
                            &extractUserInfoObjects
                            );

                    response.set_return_status(ReturnStatus::SUCCESS);

                    EXPECT_TRUE(return_val);

                    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                            response,
                            reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i]
                            );

                    if(!equivalent) {
                        std::cout << "response\n" << response.DebugString() << '\n';
                        std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[i].DebugString() << '\n';
                    }

                    EXPECT_TRUE(equivalent);
                }
            }
        }
    }
};

//Check differentUserJoined message type message works correctly with cleanedSaveMessagesToMap().
TEST_F(ChatStreamContainerObjectBasicTests, cleanedSaveMessagesToMap_userJoined) {
    ChatStreamContainerObject chat_stream_container_object;

    const std::string current_user_account_oid = bsoncxx::oid{}.to_string();
    chat_stream_container_object.current_user_account_oid_str = current_user_account_oid;
    const std::string chat_room_id = "01234567";

    //user joins
    auto retrieve_message_function = [&](
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
        std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
        auto added_message = response->mutable_return_new_chat_message()->add_messages_list();

        added_message->set_sent_by_account_id(current_user_account_oid);
        added_message->mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
                chat_room_id);
        added_message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

        reply_vector.emplace_back(std::move(response));
    };

    chat_stream_container_object.writes_waiting_to_process.push_and_size_no_coroutine(
            std::make_pair(
                    retrieve_message_function,
                    PushedToQueueFromLocation::PUSHED_FROM_INJECTION
            ),
            []() {}
    );

    auto handle = chat_stream_container_object.cleanedSaveMessagesToMap().handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(chat_stream_container_object.chat_room_ids_user_is_part_of.size(), 1);

    EXPECT_NE(
            chat_stream_container_object.chat_room_ids_user_is_part_of.find(chat_room_id),
            chat_stream_container_object.chat_room_ids_user_is_part_of.end()
    );

}

//Check multiple messages type message works correctly with cleanedSaveMessagesToMap().
TEST_F(ChatStreamContainerObjectBasicTests, cleanedSaveMessagesToMap_multipleInQueue) {
    ChatStreamContainerObject chat_stream_container_object;

    const std::string current_user_account_oid = bsoncxx::oid{}.to_string();
    chat_stream_container_object.current_user_account_oid_str = current_user_account_oid;
    const std::string chat_room_id_one = "01234567";
    const std::string chat_room_id_two = "12345678";

    {
        //user {joins -> leaves} chat room ONE
        auto retrieve_message_function_join = [&](
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
            auto added_message = response->mutable_return_new_chat_message()->add_messages_list();

            added_message->mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
                    chat_room_id_one);
            added_message->mutable_message()->mutable_message_specifics()->mutable_user_banned_message()->set_banned_account_oid(
                    current_user_account_oid);

            reply_vector.emplace_back(std::move(response));
        };

        chat_stream_container_object.writes_waiting_to_process.push_and_size_no_coroutine(
                std::make_pair(
                        retrieve_message_function_join,
                        PushedToQueueFromLocation::PUSHED_FROM_INJECTION
                ),
                []() {}
        );

        auto retrieve_message_function_leave = [&](
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
            auto added_message = response->mutable_return_new_chat_message()->add_messages_list();

            added_message->set_sent_by_account_id(current_user_account_oid);
            added_message->mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
                    chat_room_id_one);
            added_message->mutable_message()->mutable_message_specifics()->mutable_different_user_left_message();

            reply_vector.emplace_back(std::move(response));
        };

        chat_stream_container_object.writes_waiting_to_process.push_and_size_no_coroutine(
                std::make_pair(
                        retrieve_message_function_leave,
                        PushedToQueueFromLocation::PUSHED_FROM_INJECTION
                ),
                []() {}
        );

    }

    {
        //user {joins -> kicked from -> joins} chat room TWO
        auto retrieve_message_function = [&](
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
            auto added_message_join_one = response->mutable_return_new_chat_message()->add_messages_list();

            added_message_join_one->set_sent_by_account_id(current_user_account_oid);
            added_message_join_one->mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
                    chat_room_id_two);
            added_message_join_one->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

            auto added_message_join_two = response->mutable_return_new_chat_message()->add_messages_list();

            added_message_join_two->mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
                    chat_room_id_two);
            added_message_join_two->mutable_message()->mutable_message_specifics()->mutable_user_banned_message()->set_banned_account_oid(
                    current_user_account_oid);

            reply_vector.emplace_back(std::move(response));

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response_two = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            auto added_message_join_three = response_two->mutable_return_new_chat_message()->add_messages_list();

            added_message_join_three->set_sent_by_account_id(current_user_account_oid);
            added_message_join_three->mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
                    chat_room_id_two);
            added_message_join_three->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

            reply_vector.emplace_back(std::move(response_two));
        };

        chat_stream_container_object.writes_waiting_to_process.push_and_size_no_coroutine(
                std::make_pair(
                        retrieve_message_function,
                        PushedToQueueFromLocation::PUSHED_FROM_INJECTION
                ),
                []() {}
        );
    }

    auto handle = chat_stream_container_object.cleanedSaveMessagesToMap().handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(chat_stream_container_object.chat_room_ids_user_is_part_of.size(), 1);

    EXPECT_NE(
            chat_stream_container_object.chat_room_ids_user_is_part_of.find(chat_room_id_two),
            chat_stream_container_object.chat_room_ids_user_is_part_of.end()
    );
}

//Properly removes itself from map_of_chat_rooms_to_users & user_open_chat_streams.
TEST_F(ChatStreamContainerObjectBasicTests, cleanUpObject) {

    //will 'delete' this in cleanupObject, need to create with 'new' here
    auto* chat_stream_container_object = new ChatStreamContainerObject();
    chat_stream_container_object->current_index_value = 1;

    const std::string current_user_account_oid = bsoncxx::oid{}.to_string();
    chat_stream_container_object->current_user_account_oid_str = current_user_account_oid;

    const std::string chat_room_id_one = "01234578";
    const std::string chat_room_id_two = "12345789";

    const std::string second_oid = bsoncxx::oid{}.to_string();

    insertUserOIDToChatRoomId(
            chat_room_id_one,
            current_user_account_oid,
            chat_stream_container_object->current_index_value
    );

    insertUserOIDToChatRoomId(
            chat_room_id_one,
            second_oid,
            chat_stream_container_object->current_index_value
    );

    insertUserOIDToChatRoomId(
            chat_room_id_two,
            current_user_account_oid,
            chat_stream_container_object->current_index_value
    );

    bool successful = false;

    auto handle = user_open_chat_streams.upsert(
            current_user_account_oid,
            chat_stream_container_object,
            successful,
            [](ChatStreamContainerObject*) {}
    ).handle;

    handle.resume();
    handle.destroy();

    EXPECT_TRUE(successful);

    //only remove the user from chat room one
    chat_stream_container_object->chat_room_ids_user_is_part_of.insert(chat_room_id_one);

    handle = chat_stream_container_object->cleanUpObject().handle;

    handle.resume();
    handle.destroy();

    auto chat_rooms_ptr = map_of_chat_rooms_to_users.find(chat_room_id_one);
    EXPECT_NE(chat_rooms_ptr, map_of_chat_rooms_to_users.end());
    if (chat_rooms_ptr != map_of_chat_rooms_to_users.end()) {
        EXPECT_EQ(chat_rooms_ptr->second.map.size(), 1);
        auto user_account_oid_ptr = chat_rooms_ptr->second.map.find(second_oid);
        EXPECT_NE(user_account_oid_ptr, chat_rooms_ptr->second.map.end());
    }

    chat_rooms_ptr = map_of_chat_rooms_to_users.find(chat_room_id_two);
    EXPECT_NE(chat_rooms_ptr, map_of_chat_rooms_to_users.end());
    if (chat_rooms_ptr != map_of_chat_rooms_to_users.end()) {
        EXPECT_EQ(chat_rooms_ptr->second.map.size(), 1);
        auto user_account_oid_ptr = chat_rooms_ptr->second.map.find(current_user_account_oid);
        EXPECT_NE(user_account_oid_ptr, chat_rooms_ptr->second.map.end());
    }

    auto user_value = user_open_chat_streams.find(current_user_account_oid);

    EXPECT_EQ(user_value, nullptr);
}

//valid meta data empty CHAT_ROOM_VALUES
TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_validMetaData_emptyChatRooms) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
            )
    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
    );

    EXPECT_TRUE(return_val);

    grpc_stream_chat::InitialLoginMessageRequest generated_request;

    generated_request.mutable_login_info()->set_admin_info_used(false);
    generated_request.mutable_login_info()->set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    generated_request.mutable_login_info()->set_current_account_id(current_account_oid);
    generated_request.mutable_login_info()->set_logged_in_token(logged_in_token);
    generated_request.mutable_login_info()->set_installation_id(installation_id);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_request,
                    passed_request
            )
    );
}

//valid meta data with multiple values for CHAT_ROOM_VALUES
TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_validMetaData_withChatRooms) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const std::string first_chat_room_id = "01234567";
    const long first_last_time_updated = rand() % current_timestamp;
    const long first_last_time_viewed = rand() % current_timestamp;
    const std::string first_most_recent_message_uuid = generateUUID();

    const std::string second_chat_room_id = "12345678";
    const long second_last_time_updated = rand() % current_timestamp;
    const long second_last_time_viewed = -1;
    const std::string second_most_recent_message_uuid = generateUUID();

    const std::string third_chat_room_id = "23456789";
    const long third_last_time_updated = -1;
    const long third_last_time_viewed = rand() % current_timestamp;
    const std::string third_most_recent_message_uuid;

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    std::vector<std::pair<std::string, grpc::string_ref>> holding;

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
                    )
                    );

    std::string chat_rooms_string = std::string(first_chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(first_last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(first_last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(first_most_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    chat_rooms_string += std::string(second_chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(second_last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(second_last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(second_most_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    chat_rooms_string += std::string(third_chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(third_last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(third_last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(third_most_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_TRUE(return_val);

    grpc_stream_chat::InitialLoginMessageRequest generated_request;

    generated_request.mutable_login_info()->set_admin_info_used(false);
    generated_request.mutable_login_info()->set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    generated_request.mutable_login_info()->set_current_account_id(current_account_oid);
    generated_request.mutable_login_info()->set_logged_in_token(logged_in_token);
    generated_request.mutable_login_info()->set_installation_id(installation_id);

    auto first_request_chat_room = generated_request.add_chat_room_values();
    first_request_chat_room->set_chat_room_id(first_chat_room_id);
    first_request_chat_room->set_chat_room_last_time_updated(first_last_time_updated);
    first_request_chat_room->set_chat_room_last_time_viewed(first_last_time_viewed);
    first_request_chat_room->add_most_recent_message_uuids(first_most_recent_message_uuid);

    auto second_request_chat_room = generated_request.add_chat_room_values();
    second_request_chat_room->set_chat_room_id(second_chat_room_id);
    second_request_chat_room->set_chat_room_last_time_updated(second_last_time_updated);
    second_request_chat_room->set_chat_room_last_time_viewed(second_last_time_viewed);
    second_request_chat_room->add_most_recent_message_uuids(second_most_recent_message_uuid);

    auto third_request_chat_room = generated_request.add_chat_room_values();
    third_request_chat_room->set_chat_room_id(third_chat_room_id);
    third_request_chat_room->set_chat_room_last_time_updated(third_last_time_updated);
    third_request_chat_room->set_chat_room_last_time_viewed(third_last_time_viewed);
    third_request_chat_room->add_most_recent_message_uuids(third_most_recent_message_uuid);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_request,
                    passed_request
                    )
                    );
}

//invalid CURRENT_ACCOUNT_ID passed
TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidCurrentAccountId) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};


    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_FALSE(return_val);

}

//invalid LOGGED_IN_TOKEN passed
TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidLoggedInToken) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_FALSE(return_val);
}

//invalid LETS_GO_VERSION passed
TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidLetsGoVersion) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = "12#3";
    const std::string installation_id = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_FALSE(return_val);
}

//invalid INSTALLATION_ID passed
TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidInstallationId) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_FALSE(return_val);
}

TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_validMetaData_noMessages) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const std::string chat_room_id = "01234567";
    const long last_time_updated = rand() % current_timestamp;
    const long last_time_viewed = rand() % current_timestamp;

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
            )
    );

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(0)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
            )
    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
    );

    EXPECT_TRUE(return_val);
}

TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_validMetaData_multipleMessages) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const std::string chat_room_id = "01234567";
    const long last_time_updated = rand() % current_timestamp;
    const long last_time_viewed = rand() % current_timestamp;
    const std::string first_recent_message_uuid = generateUUID();
    const std::string second_recent_message_uuid = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
            )
    );

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(2)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(first_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(second_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
            )
    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
    );

    EXPECT_TRUE(return_val);
}

TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedChatRoomId) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const long last_time_updated = rand() % current_timestamp;
    const long last_time_viewed = rand() % current_timestamp;
    const std::string most_recent_message_uuid = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
                    )
                    );

    std::string chat_rooms_string =
            std::to_string(last_time_updated).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(most_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_FALSE(return_val);
}

TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedLastTimeUpdated) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const std::string chat_room_id = "01234567";
    const long last_time_viewed = rand() % current_timestamp;
    const std::string most_recent_message_uuid = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
                    )
                    );

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
//            append(std::to_string(last_time_updated)).
//            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(most_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_FALSE(return_val);
}

TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedLastTimeViewed) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const std::string chat_room_id = "01234567";
    const long last_time_updated = rand() % current_timestamp;
    const std::string most_recent_message_uuid = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
                    )
                    );

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
//            append(std::to_string(last_time_viewed)).
//            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(most_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_FALSE(return_val);
}

TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedNumberMessageUuids) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const std::string chat_room_id = "01234567";
    const long last_time_updated = rand() % current_timestamp;
    const long last_time_viewed = rand() % current_timestamp;
    const std::string most_recent_message_uuid = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
            )
    );

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
//            append(std::to_string(1)).
//            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);
            append(most_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
            )
    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
    );

    EXPECT_FALSE(return_val);
}

TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedMostRecentMessageUuid) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const std::string chat_room_id = "01234567";
    const long last_time_updated = rand() % current_timestamp;
    const long last_time_viewed = rand() % current_timestamp;
    const std::string most_recent_message_uuid = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
                    )
                    );

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);
//            append(most_recent_message_uuid).
//            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_FALSE(return_val);
}

TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_incorrectNumberMessageUuid) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const std::string chat_room_id = "01234567";
    const long last_time_updated = rand() % current_timestamp;
    const long last_time_viewed = rand() % current_timestamp;
    const std::string most_recent_message_uuid = generateUUID();
    const std::string most_recent_message_uuid_two = generateUUID();

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
            )
    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
            )
    );

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(most_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(most_recent_message_uuid_two).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
            )
    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
    );

    EXPECT_FALSE(return_val);
}

TEST_F(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_mostRecentMessageUuidTooLong) {
    ChatStreamContainerObject chat_stream_container_object;

    grpc_stream_chat::InitialLoginMessageRequest passed_request;
    std::multimap<grpc::string_ref, grpc::string_ref> passed_meta_data;

    const long current_timestamp = getCurrentTimestamp().count();

    const std::string current_account_oid = bsoncxx::oid{}.to_string();
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string lets_go_version = std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
    const std::string installation_id = generateUUID();

    const std::string chat_room_id = "01234567";
    const long last_time_updated = rand() % current_timestamp;
    const long last_time_viewed = rand() % current_timestamp;
    const std::string most_recent_message_uuid = generateUUID() + 'a';

    grpc::string_ref global_current_account_id_ref{chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID};
    grpc::string_ref current_account_oid_ref{current_account_oid};

    grpc::string_ref global_logged_in_token_ref{chat_stream_container::initial_metadata::LOGGED_IN_TOKEN};
    grpc::string_ref logged_in_token_ref{logged_in_token};

    grpc::string_ref global_lets_go_version_ref{chat_stream_container::initial_metadata::LETS_GO_VERSION};
    grpc::string_ref lets_go_version_ref{lets_go_version};

    grpc::string_ref global_installation_id_ref{chat_stream_container::initial_metadata::INSTALLATION_ID};
    grpc::string_ref installation_id_ref{installation_id};

    grpc::string_ref global_chat_room_values_ref{chat_stream_container::initial_metadata::CHAT_ROOM_VALUES};

    passed_meta_data.insert(
            std::pair(
                    global_current_account_id_ref,
                    current_account_oid_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_logged_in_token_ref,
                    logged_in_token_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_lets_go_version_ref,
                    lets_go_version_ref
                    )
                    );

    passed_meta_data.insert(
            std::pair(
                    global_installation_id_ref,
                    installation_id_ref
                    )
                    );

    std::string chat_rooms_string = std::string(chat_room_id).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_updated)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(last_time_viewed)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(std::to_string(1)).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
            append(most_recent_message_uuid).
            append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    grpc::string_ref chat_rooms_string_ref{chat_rooms_string};

    passed_meta_data.insert(
            std::pair(
                    global_chat_room_values_ref,
                    chat_rooms_string_ref
                    )
                    );

    bool return_val = ChatStreamContainerObject::extractLoginInfoFromMetaData(
            passed_meta_data,
            passed_request
            );

    EXPECT_FALSE(return_val);
}

TEST_F(RequestFullMessageInfoTests, validInfoWithMultipleMessages) {

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

    const int num_messages = 5;
    const auto amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    for(int i = 0; i < num_messages; ++i) {
        const std::string text_message_uuid = generateUUID();

        generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            text_message_uuid,
            gen_random_alpha_numeric_string(
                rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            )
        );

        auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
        messages_list_request->set_message_uuid(text_message_uuid);
        messages_list_request->set_amount_of_messages_to_request(amount_of_message);

        //sleep so messages are guaranteed at least 1 ms apart
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    ChatStreamContainerObject chat_stream_container_object;
    chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
    chat_stream_container_object.requestFullMessageInfo(
        request,
        reply_vector
    );

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), num_messages);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

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
                << chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kTextMessage
            << finalize,
            opts
        );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
            );

    int index = 0;
    for(const bsoncxx::document::view& val : messages) {
        ASSERT_LT(index, reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size());

        ChatMessageToClient response;

        bool return_val = convertChatMessageDocumentToChatMessageToClient(
                val,
                chat_room_id,
                generated_account_oid.to_string(),
                false,
                &response,
                amount_of_message,
                DifferentUserJoinedChatRoomAmount::SKELETON,
                false,
                &extractUserInfoObjects
                );

        response.set_return_status(ReturnStatus::SUCCESS);

        EXPECT_TRUE(return_val);

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                response,
                reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[index]
                );

        if(!equivalent) {
            std::cout << "response\n" << response.DebugString() << '\n';
            std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[index].DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);

        index++;
    }
}

TEST_F(RequestFullMessageInfoTests, invalidChatRoomId) {
    const std::string invalid_chat_room_id = "123";

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(invalid_chat_room_id);

    const int num_messages = 5;
    const auto amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    std::vector<std::string> message_uuids(num_messages);

    for(int i = 0; i < num_messages; ++i) {
        message_uuids[i] = generateUUID();

        generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            message_uuids[i],
            gen_random_alpha_numeric_string(
                rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            )
        );

        auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
        messages_list_request->set_message_uuid(message_uuids[i]);
        messages_list_request->set_amount_of_messages_to_request(amount_of_message);

        //sleep so messages are guaranteed at least 1 ms apart
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    ChatStreamContainerObject chat_stream_container_object;
    chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
    chat_stream_container_object.requestFullMessageInfo(
        request,
        reply_vector
    );

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_full_messages());
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().error_messages().message_uuid_list_size(), num_messages);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_LG_SERVER_ERROR);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), invalid_chat_room_id);

    for(int i = 0; i < (int)message_uuids.size(); ++i) {
        ASSERT_LT(i, reply_vector[0]->request_full_message_info_response().error_messages().message_uuid_list_size());

        EXPECT_EQ(
                message_uuids[i],
                reply_vector[0]->request_full_message_info_response().error_messages().message_uuid_list(i)
                );
    }

}

TEST_F(RequestFullMessageInfoTests, invalidMessageUuid_beginningOfMessagesList) {
    invalidMessageUuid(5, 0);
}

TEST_F(RequestFullMessageInfoTests, invalidMessageUuid_middleOfMessagesList) {
    invalidMessageUuid(5, 2);
}

TEST_F(RequestFullMessageInfoTests, invalidMessageUuid_endOfMessagesList) {
    invalidMessageUuid(5, 4);
}

TEST_F(RequestFullMessageInfoTests, validMessageUuidMessageDoesNotExist_beginning) {
    validMessageUuidMessageDoesNotExist(5, 0);
}

TEST_F(RequestFullMessageInfoTests, validMessageUuidMessageDoesNotExist_middle) {
    validMessageUuidMessageDoesNotExist(5, 2);
}

TEST_F(RequestFullMessageInfoTests, validMessageUuidMessageDoesNotExist_end) {
    validMessageUuidMessageDoesNotExist(5, 4);
}

TEST_F(RequestFullMessageInfoTests, noMessagesSent) {

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    ChatStreamContainerObject chat_stream_container_object;
    chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
    chat_stream_container_object.requestFullMessageInfo(
            request,
            reply_vector
            );

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), 0);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

}

TEST_F(RequestFullMessageInfoTests, invalidAmountOfMessage_begin) {
    invalidAmountOfMessage(5, 0);
}

TEST_F(RequestFullMessageInfoTests, invalidAmountOfMessage_middle) {
    invalidAmountOfMessage(5, 2);
}

TEST_F(RequestFullMessageInfoTests, invalidAmountOfMessage_end) {
    invalidAmountOfMessage(5, 4);
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_begin) {
    //this will make the first 2 index values the same
    duplicateMessageUuidSentSameAmountOfMessage(5, 1);
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_middle) {
    duplicateMessageUuidSentSameAmountOfMessage(5, 3);
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_end) {
    duplicateMessageUuidSentSameAmountOfMessage(5, 4);
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_differentAmounts_begin) {
    duplicateMessageUuidSentDifferentAmount(
            5,
            1,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            AmountOfMessage::COMPLETE_MESSAGE_INFO
    );
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_differentAmounts_middle) {
    duplicateMessageUuidSentDifferentAmount(
            5,
            3,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            AmountOfMessage::COMPLETE_MESSAGE_INFO
            );
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_differentAmounts_end) {
    duplicateMessageUuidSentDifferentAmount(
            5,
            4,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            AmountOfMessage::COMPLETE_MESSAGE_INFO
            );
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_differentAmounts_largestFirst_begin) {
    duplicateMessageUuidSentDifferentAmount(
            5,
            1,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE
            );
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_differentAmounts_largestFirst_middle) {
    duplicateMessageUuidSentDifferentAmount(
            5,
            3,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE
            );
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_differentAmounts_largestFirst_skeleton) {
    duplicateMessageUuidSentDifferentAmount(
            5,
            4,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE
            );
}

TEST_F(RequestFullMessageInfoTests, duplicateMessageUuid_differentAmounts_requestSkeleton) {
    duplicateMessageUuidSentDifferentAmount(
            5,
            3,
            AmountOfMessage::ONLY_SKELETON,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE
            );
}

TEST_F(RequestFullMessageInfoTests, userNotInChatRoom) {

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

    const int num_messages = 5;
    const auto amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    std::vector<std::string> requested_message_uuids;

    for(int i = 0; i < num_messages; ++i) {
        requested_message_uuids.emplace_back(generateUUID());

        generateRandomTextMessage(
                generated_account_oid,
                generated_user_account.logged_in_token,
                generated_user_account.installation_ids.front(),
                chat_room_id,
                requested_message_uuids.back(),
                gen_random_alpha_numeric_string(
                        rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
                        )
                        );

        auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
        messages_list_request->set_message_uuid(requested_message_uuids.back());
        messages_list_request->set_amount_of_messages_to_request(amount_of_message);

        //sleep so messages are guaranteed at least 1 ms apart
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
    grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

    setupUserLoginInfo(
            leave_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front()
            );

    leave_chat_room_request.set_chat_room_id(chat_room_id);

    leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

    ASSERT_EQ(leave_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    ChatStreamContainerObject chat_stream_container_object;
    chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
    chat_stream_container_object.requestFullMessageInfo(
            request,
            reply_vector
            );

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_full_messages());
    ASSERT_EQ(reply_vector[0]->request_full_message_info_response().error_messages().message_uuid_list_size(), requested_message_uuids.size());
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_USER_NOT_A_MEMBER_OF_CHAT_ROOM);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

    for(int i = 0; i < reply_vector[0]->request_full_message_info_response().error_messages().message_uuid_list_size(); ++i) {
        EXPECT_EQ(requested_message_uuids[i], reply_vector[0]->request_full_message_info_response().error_messages().message_uuid_list(i));
    }
}

TEST_F(RequestFullMessageInfoTests, onlyInvalidMessageUuid) {

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

    const int num_messages = 1;
    const auto amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    const std::string empty_message_uuid;
    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
    messages_list_request->set_message_uuid(empty_message_uuid);
    messages_list_request->set_amount_of_messages_to_request(amount_of_message);

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    ChatStreamContainerObject chat_stream_container_object;
    chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
    chat_stream_container_object.requestFullMessageInfo(
            request,
            reply_vector
            );

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
    ASSERT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), num_messages);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

    ChatMessageToClient response;

    response.set_return_status(ReturnStatus::VALUE_NOT_SET);
    response.set_message_uuid(empty_message_uuid);

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            response,
            reply_vector[0]->request_full_message_info_response().full_messages().full_message_list(0)
            );

    if(!equivalent) {
        std::cout << "response\n" << response.DebugString() << '\n';
        std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list(0).DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}

TEST_F(RequestFullMessageInfoTests, requestZeroMessages) {
    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    ChatStreamContainerObject chat_stream_container_object;
    chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
    chat_stream_container_object.requestFullMessageInfo(
            request,
            reply_vector
            );

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), 0);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);
}

TEST_F(RequestFullMessageInfoTests, requestMoreThanMaxNumberMessages) {

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

    const int num_messages = chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST * 3;
    const auto amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    std::vector<std::string> requested_message_uuids;

    for(int i = 0; i < num_messages; ++i) {
        requested_message_uuids.emplace_back(generateUUID());

        generateRandomTextMessage(
                generated_account_oid,
                generated_user_account.logged_in_token,
                generated_user_account.installation_ids.front(),
                chat_room_id,
                requested_message_uuids.back(),
                gen_random_alpha_numeric_string(
                        rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
                        )
                        );

        auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
        messages_list_request->set_message_uuid(requested_message_uuids.back());
        messages_list_request->set_amount_of_messages_to_request(amount_of_message);

        //sleep so messages are guaranteed at least 1 ms apart
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    ChatStreamContainerObject chat_stream_container_object;
    chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
    chat_stream_container_object.requestFullMessageInfo(
            request,
            reply_vector
            );

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), requested_message_uuids.size());
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

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
                << chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kTextMessage
            << finalize,
            opts
        );

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
            );

    int index = 0;
    for(const bsoncxx::document::view& val : messages) {
        ASSERT_LT(index, reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size());
        ASSERT_LT(index, requested_message_uuids.size());

        ChatMessageToClient response;

        if(index >= chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST) {
            response.set_return_status(ReturnStatus::UNKNOWN);
            response.set_message_uuid(requested_message_uuids[index]);
        } else {
            bool return_val = convertChatMessageDocumentToChatMessageToClient(
                    val,
                    chat_room_id,
                    generated_account_oid.to_string(),
                    false,
                    &response,
                    amount_of_message,
                    DifferentUserJoinedChatRoomAmount::SKELETON,
                    false,
                    &extractUserInfoObjects
                    );

            response.set_return_status(ReturnStatus::SUCCESS);

            EXPECT_TRUE(return_val);
        }

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                response,
                reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[index]
                );

        if(!equivalent) {
            std::cout << "response\n" << response.DebugString() << '\n';
            std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list()[index].DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);

        index++;
    }
}

TEST_F(RequestFullMessageInfoTests, requestInvalidTypes) {

    //make sure kDifferentUserJoinedMessage message is stored
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    mongocxx::options::find opts;

    auto different_user_joined_message = chat_room_collection.find_one(
        document{}
            << "_id" << open_document
                << "$ne" << "iDe"
            << close_document
            << chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
        << finalize,
        opts
    );

    ASSERT_TRUE(different_user_joined_message);

    grpc_stream_chat::ChatToServerRequest request;
    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);

    const std::string message_uuid = different_user_joined_message->view()["_id"].get_string().value.to_string();

    auto message_uuid_ele = request.mutable_request_full_message_info()->add_message_uuid_list();
    message_uuid_ele->set_amount_of_messages_to_request(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    message_uuid_ele->set_message_uuid(message_uuid);

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    ChatStreamContainerObject chat_stream_container_object;
    chat_stream_container_object.current_user_account_oid_str = generated_account_oid.to_string();
    chat_stream_container_object.requestFullMessageInfo(
            request,
            reply_vector
    );

    ASSERT_EQ(reply_vector.size(), 1);
    EXPECT_FALSE(reply_vector[0]->request_full_message_info_response().has_error_messages());
    ASSERT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), 1);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().request_status(), grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus_FINAL_MESSAGE_LIST);
    EXPECT_EQ(reply_vector[0]->request_full_message_info_response().chat_room_id(), chat_room_id);

    ChatMessageToClient response;
    response.set_return_status(ReturnStatus::UNKNOWN);
    response.set_message_uuid(message_uuid);

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            response,
            reply_vector[0]->request_full_message_info_response().full_messages().full_message_list(0)
    );

    if(!equivalent) {
        std::cout << "response\n" << response.DebugString() << '\n';
        std::cout << "reply_vector\n" << reply_vector[0]->request_full_message_info_response().full_messages().full_message_list(0).DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);

}
