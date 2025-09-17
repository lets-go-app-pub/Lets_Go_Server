//
// Created by jeremiah on 6/16/22.
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
#include <move_user_account_statistics_document_test.h>
#include <reports_objects.h>
#include <generate_randoms.h>
#include <report_helper_functions_test.h>
#include <mongocxx/exception/logic_error.hpp>
#include <deleted_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include <chat_rooms_objects.h>
#include "utility_chat_functions_test.h"
#include <user_account_keys.h>
#include <admin_account_keys.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>
#include <user_pictures_keys.h>

#include <admin_functions_for_request_values.h>
#include <generate_random_messages.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class AdminFunctionsForRequestValues : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(AdminFunctionsForRequestValues, extractSingleMessageAtTimeStamp) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    UserAccountDoc generated_user_account(generated_account_oid);

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front()
            );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            create_chat_room_response.chat_room_id()
    );

    const std::string& message_uuid = client_message_to_server_request.message_uuid();

    ChatRoomMessageDoc chat_room_message(
            message_uuid,
            create_chat_room_response.chat_room_id()
            );

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + create_chat_room_response.chat_room_id()];

    ChatMessageToClient returned_chat_message_to_client;

    auto error_func = [](const std::string& error_str) {
        EXPECT_TRUE(false);
        std::cout << "error_str: " << error_str << '\n';
    };

    bool return_val = extractSingleMessageAtTimeStamp(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            chat_room_collection,
            create_chat_room_response.chat_room_id(),
            message_uuid, chat_room_message.shared_properties.timestamp.value,
            &returned_chat_message_to_client,
            error_func
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    returned_chat_message_to_client.message(),
                    client_message_to_server_request.message()
                    )
            );

    EXPECT_EQ(returned_chat_message_to_client.timestamp_stored(), chat_room_message.shared_properties.timestamp.value.count());
    EXPECT_EQ(returned_chat_message_to_client.message_uuid(), message_uuid);
    EXPECT_EQ(returned_chat_message_to_client.sent_by_account_id(), generated_account_oid.to_string());

}

//This test will send a text message, send 2 edits, then extract the first edit
TEST_F(AdminFunctionsForRequestValues, extractSingleMessageAtTimeStamp_messageEdited) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    UserAccountDoc generated_user_account(generated_account_oid);

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front()
            );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    auto [client_message_to_server_request, client_message_to_server_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            create_chat_room_response.chat_room_id()
            );

    const std::string& message_uuid = client_message_to_server_request.message_uuid();

    grpc_chat_commands::ClientMessageToServerRequest edited_message_to_server_request;
    grpc_chat_commands::ClientMessageToServerResponse edited_message_to_server_response;

    setupUserLoginInfo(
            edited_message_to_server_request.mutable_login_info(),
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front()
            );

    edited_message_to_server_request.set_message_uuid(generateUUID());
    edited_message_to_server_request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = edited_message_to_server_request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(create_chat_room_response.chat_room_id());

    std::string first_edited_message = gen_random_alpha_numeric_string((rand() % 10) + 5);

    MessageSpecifics* message_specifics = edited_message_to_server_request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_edited_message()->set_new_message(first_edited_message);
    message_specifics->mutable_edited_message()->set_message_uuid(message_uuid);

    clientMessageToServer(&edited_message_to_server_request, &edited_message_to_server_response);

    ASSERT_EQ(edited_message_to_server_response.return_status(), ReturnStatus::SUCCESS);

    edited_message_to_server_request.set_message_uuid(generateUUID());
    edited_message_to_server_request.set_timestamp_observed(getCurrentTimestamp().count());

    message_specifics = edited_message_to_server_request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_edited_message()->set_new_message(gen_random_alpha_numeric_string((rand() % 10) + 5));
    message_specifics->mutable_edited_message()->set_message_uuid(message_uuid);

    clientMessageToServer(&edited_message_to_server_request, &edited_message_to_server_response);

    ASSERT_EQ(edited_message_to_server_response.return_status(), ReturnStatus::SUCCESS);

    ChatRoomMessageDoc chat_room_message(
        message_uuid,
        create_chat_room_response.chat_room_id()
    );

    auto extracted_message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(chat_room_message.message_specifics_document);

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + create_chat_room_response.chat_room_id()];

    ChatMessageToClient returned_chat_message_to_client;

    auto error_func = [](const std::string& error_str) {
        EXPECT_TRUE(false);
        std::cout << "error_str: " << error_str << '\n';
    };

    bool return_val = extractSingleMessageAtTimeStamp(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            chat_room_collection,
            create_chat_room_response.chat_room_id(),
            message_uuid,
            //this simulates that the 'report' was sent before the final edit occurred
            std::chrono::milliseconds{extracted_message->text_edited_time.value.count()-1},
            &returned_chat_message_to_client,
            error_func
            );

    EXPECT_TRUE(return_val);

    client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(first_edited_message);
    client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(true);
    client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(extracted_message->text_edited_time);

//    std::cout << "returned_chat_message_to_client\n" << returned_chat_message_to_client.message().DebugString() << '\n';
//    std::cout << "client_message_to_server_request\n" << client_message_to_server_request.message().DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    returned_chat_message_to_client.message(),
                    client_message_to_server_request.message()
                    )
                    );

    EXPECT_EQ(returned_chat_message_to_client.timestamp_stored(), chat_room_message.shared_properties.timestamp.value.count());
    EXPECT_EQ(returned_chat_message_to_client.message_uuid(), message_uuid);
    EXPECT_EQ(returned_chat_message_to_client.sent_by_account_id(), generated_account_oid.to_string());

}

TEST_F(AdminFunctionsForRequestValues, extractUserInfo) {
    //NOTE: Leaving this empty, extractUserInfo() is mostly just for displaying things to
    // admins on the desktop interface, it will be fairly obvious if anything is wrong.
}