//
// Created by jeremiah on 8/2/22.
//

#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/collection.hpp>
#include <database_names.h>
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
#include "connection_pool_global_variable.h"
#include "chat_room_shared_keys.h"
#include "chat_room_header_keys.h"
#include "chat_room_message_keys.h"
#include "chat_room_commands_helper_functions.h"
#include "compare_equivalent_messages.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class CreateChatRoomTesting : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;

    UserAccountDoc initial_user_account;

    bsoncxx::oid user_thumbnail_oid;
    UserPictureDoc initial_user_thumbnail;

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    std::string chat_room_id;

    //Create two users and put them both inside a chat room
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        initial_user_account.getFromCollection(generated_account_oid);

        for(const auto& pic : initial_user_account.pictures) {
            if(pic.pictureStored()) {

                bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

                pic.getPictureReference(
                        user_thumbnail_oid,
                        timestamp_stored
                );

                initial_user_thumbnail.getFromCollection(user_thumbnail_oid);
                break;
            }
        }

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                generated_account_oid,
                initial_user_account.logged_in_token,
                initial_user_account.installation_ids.front()
        );
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void checkChatRoom(
            const std::string& chat_room_name
            ) {

        //allow time for the kDifferentUserMessage to be stored in the database
        std::this_thread::sleep_for(std::chrono::milliseconds{20});

        initial_user_account.chat_rooms.emplace_back(
                chat_room_id,
                bsoncxx::types::b_date{std::chrono::milliseconds{create_chat_room_response.last_activity_time_timestamp()}}
        );

        UserAccountDoc extracted_user_account(generated_account_oid);
        EXPECT_EQ(extracted_user_account, initial_user_account);

        initial_user_thumbnail.thumbnail_references.emplace_back(chat_room_id);

        UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
        EXPECT_EQ(initial_user_thumbnail, extracted_user_thumbnail);

        ChatRoomHeaderDoc chat_room_header(chat_room_id);

        EXPECT_EQ(chat_room_header.chat_room_id, chat_room_id);
        EXPECT_EQ(chat_room_header.chat_room_name, chat_room_name);
        EXPECT_EQ(chat_room_header.chat_room_password, create_chat_room_response.chat_room_password());
        EXPECT_EQ(chat_room_header.chat_room_last_active_time.value.count(), create_chat_room_response.last_activity_time_timestamp());
        EXPECT_EQ(chat_room_header.matching_oid_strings, nullptr);

        EXPECT_EQ(chat_room_header.accounts_in_chat_room.size(), 1);
        if(!chat_room_header.accounts_in_chat_room.empty()) {
            EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].account_oid.to_string(), generated_account_oid.to_string());
            EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].state_in_chat_room, AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN);
            EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].first_name, initial_user_account.first_name);
            EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].thumbnail_reference, user_thumbnail_oid.to_string());
            EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].thumbnail_timestamp.value.count(), create_chat_room_response.last_activity_time_timestamp());
            EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].thumbnail_size, initial_user_thumbnail.thumbnail_size_in_bytes);
            EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].last_activity_time.value.count(), create_chat_room_response.last_activity_time_timestamp());
            EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].times_joined_left.size(), 1);
            if(!chat_room_header.accounts_in_chat_room[0].times_joined_left.empty()) {
                EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].times_joined_left[0].value.count(), create_chat_room_response.last_activity_time_timestamp());
            }
        }

        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
        mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

        mongocxx::options::find opts;

        opts.sort(
                document{}
                        << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                        << finalize
        );

        auto find_result = chat_room_collection.find(
                document{}
                        << "_id" << open_document
                        << "$ne" << chat_room_header_keys::ID
                        << close_document
                        << finalize,
                opts
        );

        int index = 0;
        for(const bsoncxx::document::view& doc : find_result) {

            ChatMessageToClient chat_message;

            bool return_val = convertChatMessageDocumentToChatMessageToClient(
                    doc,
                    chat_room_id,
                    generated_account_oid.to_string(),
                    true,
                    &chat_message,
                    AmountOfMessage::ONLY_SKELETON,
                    DifferentUserJoinedChatRoomAmount::SKELETON,
                    false,
                    nullptr);

            EXPECT_TRUE(return_val);

            const auto message_type = MessageSpecifics::MessageBodyCase(doc[chat_room_message_keys::MESSAGE_TYPE].get_int32().value);
            if(index == 0) { //cap message
                EXPECT_EQ(message_type, MessageSpecifics::MessageBodyCase::kChatRoomCapMessage);
                compareEquivalentMessages(chat_message, create_chat_room_response.chat_room_cap_message());
            } else if(index == 1) { //different user joined message
                EXPECT_EQ(message_type, MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage);
                compareEquivalentMessages(chat_message, create_chat_room_response.current_user_joined_chat_message());
            } else {
                std::cout << convertMessageBodyTypeToString(message_type) << '\n';
            }

            index++;
        }

        EXPECT_EQ(index, 2);
    }
};

TEST_F(CreateChatRoomTesting, createChatRoom_invalidName) {

    const std::string chat_room_name = gen_random_alpha_numeric_string(
            rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1
    );

    create_chat_room_request.set_chat_room_name(chat_room_name);

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    chat_room_id = create_chat_room_response.chat_room_id();

    EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    auto collection_names = chat_room_db.list_collection_names();

    //Make sure chat room was not created.
    EXPECT_EQ(collection_names.size(), 1);
}

TEST_F(CreateChatRoomTesting, createChatRoom) {

    const std::string chat_room_name = "chat_room_name";

    create_chat_room_request.set_chat_room_name(chat_room_name);

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    chat_room_id = create_chat_room_response.chat_room_id();

    checkChatRoom(chat_room_name);
}

TEST_F(CreateChatRoomTesting, createChatRoom_emptyName) {

    const std::string chat_room_name = generateEmptyChatRoomName(initial_user_account.first_name);

    create_chat_room_request.set_chat_room_name("");

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    chat_room_id = create_chat_room_response.chat_room_id();

    checkChatRoom(chat_room_name);
}

TEST_F(CreateChatRoomTesting, createChatRoom_loginInfo) {

    checkLoginInfoClientOnly(
            generated_account_oid,
            initial_user_account.logged_in_token,
            initial_user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {
                create_chat_room_request.mutable_login_info()->CopyFrom(login_info);

                createChatRoom(&create_chat_room_request, &create_chat_room_response);

                return create_chat_room_response.return_status();
            }
    );
}
