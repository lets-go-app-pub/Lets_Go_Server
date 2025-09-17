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
#include "generate_random_messages.h"
#include "connection_pool_global_variable.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>
#include "utility_chat_functions.h"

#include "chat_room_commands_helper_functions.h"
#include "chat_room_message_keys.h"
#include "accepted_mime_types.h"
#include "build_match_made_chat_room.h"
#include "set_fields_functions.h"
#include "generate_randoms.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class HandleMessageToBeSentTests : public ::testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    std::chrono::milliseconds current_timestamp{-1};

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        current_timestamp = getCurrentTimestamp();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

class SendDifferentUserJoinedChatRoomTests : public ::testing::Test {
protected:

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

class SendMessageToChatRoomTests : public ::testing::Test {
protected:

    const std::string message_uuid = generateUUID();
    const std::string chat_room_id = "12345678";
    const bsoncxx::oid user_account_oid{};

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    std::chrono::milliseconds current_timestamp{-1};

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        current_timestamp = getCurrentTimestamp();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void compareFunctionResult(
            grpc_chat_commands::ClientMessageToServerRequest& request,
            AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO
            ) {
        ChatRoomMessageDoc message(message_uuid, chat_room_id);
        document message_doc_builder{};

        message.convertToDocument(message_doc_builder);

        auto extract_user_info = ExtractUserInfoObjects(mongo_cpp_client, accounts_db, user_accounts_collection);

        ChatMessageToClient extracted_message;

        convertChatMessageDocumentToChatMessageToClient(
                message_doc_builder.view(),
                chat_room_id,
                general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT,
                false,
                &extracted_message,
                amount_of_message,
                DifferentUserJoinedChatRoomAmount::SKELETON,
                false,
                &extract_user_info
        );

        EXPECT_EQ(request.message_uuid(), extracted_message.message_uuid());

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                request.message(),
                extracted_message.message()
        );

        if(!equivalent) {
            std::cout << "request.message()\n" << request.message().DebugString() << '\n';
            std::cout << "extracted_message\n" << extracted_message.message().DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }
};

class UnMatchHelperTests : public ::testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    SetupTestingForUnMatch values;

    //Set up a match.
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

class UpdateSingleChatRoomMemberNotInChatRoomTests : public ::testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

class FilterAndStoreUsersToBeUpdatedTests : public ::testing::Test {
protected:

    const std::string chat_room_id = generateRandomChatRoomId();
    google::protobuf::RepeatedPtrField<OtherUserInfoForUpdates> chat_room_member_info;
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    void SetUp() override {
        addRandomOtherUserInfo();
    }

    void TearDown() override {
    }

    void addRandomOtherUserInfo() {
        auto other_user = chat_room_member_info.Add();

        other_user->set_account_oid(bsoncxx::oid{}.to_string());
        other_user->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);
        other_user->set_thumbnail_size_in_bytes(123);
        other_user->set_thumbnail_index_number(1);
        other_user->set_thumbnail_timestamp(current_timestamp.count());
        other_user->set_first_name(gen_random_alpha_numeric_string(rand() % 10 + 10));
        other_user->set_age(22);
        other_user->set_member_info_last_updated_timestamp(current_timestamp.count());
    }

    static void compareMessageEquivalent(
            const OtherUserInfoForUpdates& first,
            const OtherUserInfoForUpdates& second
            ) {
        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                first,
                second
        );

        if(!equivalent) {
            std::cout << "users_to_be_updated\n" << first.DebugString() << '\n';
            std::cout << "first_user\n" << second.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }
};

TEST_F(HandleMessageToBeSentTests, messageProperlyInserted) {

    std::string chat_room_id = "12345678";

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    const std::string message_uuid = generateUUID();
    const std::string message_text = "hello\n.";

    bsoncxx::oid user_account_oid{};
    grpc_chat_commands::ClientMessageToServerRequest request;

    //set this up to be compared with extracted_message below
    request.set_message_uuid(message_uuid);
    request.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);
    request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    request.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(message_text);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(false);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(-1);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    bsoncxx::builder::stream::document insert_doc_builder;

    insert_doc_builder
        << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
                << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << DeleteType::DELETE_TYPE_NOT_SET
                << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
                << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << message_text
            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << false
            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
        << close_document;

    SendMessageToChatRoomReturn return_val = handleMessageToBeSent(
            nullptr,
            chat_room_collection,
            &request,
            insert_doc_builder,
            user_account_oid
    );

    ChatRoomMessageDoc message(message_uuid, chat_room_id);
    document message_doc_builder{};

    message.convertToDocument(message_doc_builder);

    auto extract_user_info = ExtractUserInfoObjects(mongo_cpp_client, accounts_db, user_accounts_collection);

    ChatMessageToClient extracted_message;

    convertChatMessageDocumentToChatMessageToClient(
            message_doc_builder.view(),
            chat_room_id,
            general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT,
            false,
            &extracted_message,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            &extract_user_info
    );

    EXPECT_EQ(return_val.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_GE(return_val.time_stored_on_server.count(), current_timestamp.count());
    EXPECT_TRUE(return_val.picture_oid.empty());

    EXPECT_EQ(request.message_uuid(), extracted_message.message_uuid());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            request.message(),
            extracted_message.message()
    );

    if(!equivalent) {
        std::cout << "request.message()\n" << request.message().DebugString() << '\n';
        std::cout << "extracted_message\n" << extracted_message.message().DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);

}

TEST_F(HandleMessageToBeSentTests, duplicateMessage) {

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

    const std::string chat_room_id = create_chat_room_response.chat_room_id();
    const std::string message_uuid = generateUUID();
    const std::string message_text = "hello\n.";

    auto[text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            message_uuid,
            message_text
    );

    ASSERT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //make sure the duplicate is attempted at a different timestamp (this is to test the results)
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    grpc_chat_commands::ClientMessageToServerRequest request;

    //set this up to be compared with extracted_message below
    request.set_message_uuid(message_uuid);
    request.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);
    request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    request.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(message_text);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(false);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(-1);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    bsoncxx::builder::stream::document insert_doc_builder;

    insert_doc_builder
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
                    << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << DeleteType::DELETE_TYPE_NOT_SET
                    << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
                    << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
                << close_document
                << chat_room_message_keys::message_specifics::TEXT_MESSAGE << message_text
                << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << false
                << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
            << close_document;

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    SendMessageToChatRoomReturn return_val = handleMessageToBeSent(
            nullptr,
            chat_room_collection,
            &request,
            insert_doc_builder,
            generated_account_oid
    );

    ChatRoomMessageDoc message(message_uuid, chat_room_id);
    document message_doc_builder{};

    message.convertToDocument(message_doc_builder);

    auto extract_user_info = ExtractUserInfoObjects(mongo_cpp_client, accounts_db, user_accounts_collection);

    ChatMessageToClient extracted_message;

    convertChatMessageDocumentToChatMessageToClient(
            message_doc_builder.view(),
            chat_room_id,
            general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT,
            false,
            &extracted_message,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            &extract_user_info
    );

    EXPECT_EQ(return_val.successful, SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED);
    EXPECT_TRUE(return_val.picture_oid.empty());

    //make sure timestamp has not changed (message was NOT updated)
    EXPECT_GE(return_val.time_stored_on_server.count(), text_message_response.timestamp_stored());

    EXPECT_EQ(request.message_uuid(), extracted_message.message_uuid());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            request.message(),
            extracted_message.message()
    );

    if(!equivalent) {
        std::cout << "request.message()\n" << request.message().DebugString() << '\n';
        std::cout << "extracted_message\n" << extracted_message.message().DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);

}

TEST_F(HandleMessageToBeSentTests, corruptMessage) {

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

    const std::string chat_room_id = create_chat_room_response.chat_room_id();
    const std::string message_uuid = generateUUID();
    const std::string message_text = "hello\n.";

    auto[text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front(),
            chat_room_id,
            message_uuid,
            message_text
    );

    ASSERT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //make sure the duplicate is attempted at a different timestamp (this is to test the results)
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    grpc_chat_commands::ClientMessageToServerRequest request;

    //set this up to be compared with extracted_message below
    request.set_message_uuid(message_uuid);
    request.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);
    request.mutable_message()->mutable_standard_message_info()->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    request.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(message_text);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(false);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(-1);
    request.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    bsoncxx::builder::stream::document insert_doc_builder;

    insert_doc_builder
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
                    << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << DeleteType::DELETE_TYPE_NOT_SET
                    << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
                    << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
                << close_document
                << chat_room_message_keys::message_specifics::TEXT_MESSAGE << message_text
                << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << false
                << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
            << close_document;

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    //make sure message is corrupt
    chat_room_collection.update_one(
            document{}
                << "_id" << message_uuid
            << finalize,
            document{}
                << "$set" << open_document
                    << chat_room_message_keys::RANDOM_INT << "123"
                << close_document
            << finalize
    );

    SendMessageToChatRoomReturn return_val = handleMessageToBeSent(
            nullptr,
            chat_room_collection,
            &request,
            insert_doc_builder,
            generated_account_oid
    );

    ChatRoomMessageDoc message(message_uuid, chat_room_id);

    //message was removed
    EXPECT_TRUE(message.id.empty());
    EXPECT_TRUE(message.chat_room_id.empty());

    EXPECT_EQ(return_val.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED);
    EXPECT_TRUE(return_val.picture_oid.empty());

    //make sure timestamp has not changed (message was NOT updated)
    EXPECT_EQ(return_val.time_stored_on_server, std::chrono::milliseconds{-1});

}

TEST_F(SendDifferentUserJoinedChatRoomTests, properlyInsertedMessage) {

    const std::string chat_room_id = "12345678";
    const bsoncxx::oid user_account_oid{};
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    size_t doc_count = chat_room_collection.count_documents(
            document{}
                    << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
            << finalize
    );

    EXPECT_EQ(doc_count, 0);

    bsoncxx::builder::stream::document different_user_joined_chat_room_message_doc;

    const std::string message_uuid = generateDifferentUserJoinedChatRoomMessage(
            different_user_joined_chat_room_message_doc,
            user_account_oid,
            AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
            "",
            current_timestamp
    );

    sendDifferentUserJoinedChatRoom(
            chat_room_id,
            user_account_oid,
            different_user_joined_chat_room_message_doc,
            message_uuid
    );

    doc_count = chat_room_collection.count_documents(
            document{}
                    << "_id" << message_uuid
                    << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
            << finalize
    );

    EXPECT_EQ(doc_count, 1);
}

TEST_F(SendMessageToChatRoomTests, kTextMessage_randomMessage) {

    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(false);
    message_specifics->mutable_text_message()->set_message_text(gen_random_alpha_numeric_string((rand() % 1000) + 5));
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kTextMessage_mongoDbCommand) {

    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(false);
    message_specifics->mutable_text_message()->set_message_text("$" + chat_room_message_keys::MESSAGE_TYPE);
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kPictureMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(false);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    std::string actual_picture_in_bytes = gen_random_alpha_numeric_string((rand() % 1000) + 5);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_picture_message()->mutable_active_message_info()->set_is_reply(false);
    message_specifics->mutable_picture_message()->set_image_height(rand() % 500 + 25);
    message_specifics->mutable_picture_message()->set_image_width(rand() % 500 + 25);

    std::string inserted_picture_oid = bsoncxx::oid{}.to_string();

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            inserted_picture_oid,
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_EQ(result.picture_oid, inserted_picture_oid);
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(
        request,
        AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE
    );
}

TEST_F(SendMessageToChatRoomTests, kLocationMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_location_message()->mutable_active_message_info()->set_is_reply(false);
    message_specifics->mutable_location_message()->set_longitude(42.5523);
    message_specifics->mutable_location_message()->set_latitude(-87.5423);

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kMimeTypeMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    std::string actual_mime_type;

    std::vector<std::string> accepted_mime_types_list;

    for(const std::string& extracted_mime_type : accepted_mime_types) {
        accepted_mime_types_list.emplace_back(extracted_mime_type);
    }

    //randomly choose an accepted mime type (make a copy)
    actual_mime_type = std::string(accepted_mime_types_list[rand() % accepted_mime_types_list.size()]);

    std::string actual_url_of_download;

    actual_url_of_download = gen_random_alpha_numeric_string((rand() % 1000) + 5);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_mime_type_message()->mutable_active_message_info()->set_is_reply(false);
    message_specifics->mutable_mime_type_message()->set_image_height(rand() % 500 + 25);
    message_specifics->mutable_mime_type_message()->set_image_width(rand() % 500 + 25);
    message_specifics->mutable_mime_type_message()->set_url_of_download(actual_url_of_download);
    message_specifics->mutable_mime_type_message()->set_mime_type(actual_mime_type);

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kInviteMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    std::string actual_invited_user_account_oid = bsoncxx::oid{}.to_string();
    std::string actual_invited_user_name = gen_random_alpha_numeric_string(rand() % 50 + 20);

    //remove anything that is not a char, lowercase everything that is a char
    for(auto it = actual_invited_user_name.begin(); it != actual_invited_user_name.end();) {
        if(std::isalpha(*it)) {
            *it = (char)tolower(*it);
            it++;
        } else {
            it = actual_invited_user_name.erase(it);
        }
    }

    //capitalize first letter (the check inside clientMessageToServer() allows an empty name)
    if(!actual_invited_user_name.empty()) {
        actual_invited_user_name[0] = (char)toupper(actual_invited_user_name[0]);
    }

    std::string actual_invited_chat_room_id;

    actual_invited_chat_room_id = gen_random_alpha_numeric_string(7 + rand() % 2);

    for(char& c : actual_invited_chat_room_id) {
        c = (char)tolower(c);
    }

    std::string actual_invited_chat_room_password = gen_random_alpha_numeric_string(rand() % 50);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_invite_message()->mutable_active_message_info()->set_is_reply(false);
    message_specifics->mutable_invite_message()->set_invited_user_account_oid(actual_invited_user_account_oid);
    message_specifics->mutable_invite_message()->set_invited_user_name(actual_invited_user_name);
    message_specifics->mutable_invite_message()->set_chat_room_id(actual_invited_chat_room_id);
    message_specifics->mutable_invite_message()->set_chat_room_name("invited_chat_room_name");
    message_specifics->mutable_invite_message()->set_chat_room_password(actual_invited_chat_room_password);

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kEditedMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    std::string actual_edited_message_text = gen_random_alpha_numeric_string((rand() % 20) + 5);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_edited_message()->set_new_message(actual_edited_message_text);
    message_specifics->mutable_edited_message()->set_message_uuid(generateUUID());

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "previous_message",
            std::chrono::milliseconds{1234},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kDeletedMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    DeleteType actual_delete_type = DeleteType::DELETE_FOR_ALL_USERS;

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_deleted_message()->set_message_uuid(generateUUID());
    message_specifics->mutable_deleted_message()->set_delete_type(actual_delete_type);

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{1234},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kUserKickedMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    request.mutable_message()->mutable_message_specifics()->mutable_user_kicked_message()->set_kicked_account_oid(
            bsoncxx::oid{}.to_string()
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kUserBannedMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    request.mutable_message()->mutable_message_specifics()->mutable_user_banned_message()->set_banned_account_oid(
            bsoncxx::oid{}.to_string()
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kDifferentUserLeftMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    request.mutable_message()->mutable_message_specifics()->mutable_different_user_left_message()->set_new_admin_account_oid(
            bsoncxx::oid{}.to_string()
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kChatRoomCapMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    request.mutable_message()->mutable_message_specifics()->mutable_chat_room_cap_message();

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());
    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kUserActivityDetectedMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    request.mutable_message()->mutable_message_specifics()->mutable_user_activity_detected_message();

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    request.clear_message_uuid();

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kChatRoomNameUpdatedMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    request.mutable_message()->mutable_message_specifics()->mutable_chat_room_name_updated_message()->set_new_chat_room_name(
            gen_random_alpha_numeric_string(rand() % 100 + 5)
            );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kChatRoomPasswordUpdatedMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    request.mutable_message()->mutable_message_specifics()->mutable_chat_room_password_updated_message()->set_new_chat_room_password(
            gen_random_alpha_numeric_string(rand() % 100 + 5)
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kNewAdminPromotedMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    request.mutable_message()->mutable_message_specifics()->mutable_new_admin_promoted_message()->set_promoted_account_oid(
            bsoncxx::oid{}.to_string()
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kMatchCanceledMessage) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    request.mutable_message()->mutable_message_specifics()->mutable_match_canceled_message()->set_matched_account_oid(
            bsoncxx::oid{}.to_string()
            );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, MESSAGE_BODY_NOT_SET) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            nullptr
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_EQ(result.time_stored_on_server, std::chrono::milliseconds{-1});
}

TEST_F(SendMessageToChatRoomTests, kTextReply_doesNotNeedTrimmed) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->set_message_text(gen_random_alpha_numeric_string((rand() % 100) + 5));
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(true);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            bsoncxx::oid{}.to_string()
            );
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            generateUUID()
    );
    //guarantee that message does not need trimmed
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_text_reply()->set_message_text(
            gen_random_alpha_numeric_string((rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TEXT_MESSAGE_REPLY-1)) + 1)
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kTextReply_needsTrimmed) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->set_message_text(gen_random_alpha_numeric_string((rand() % 100) + 5));
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(true);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            bsoncxx::oid{}.to_string()
    );
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            generateUUID()
    );

    //guarantee that message needs to be trimmed
    std::string untrimmed_message = gen_random_alpha_numeric_string((rand() % 100) + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TEXT_MESSAGE_REPLY+1);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_text_reply()->set_message_text(
            untrimmed_message
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()
    );

    while(untrimmed_message.size() > server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TEXT_MESSAGE_REPLY) {
        untrimmed_message.pop_back();
    }

    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_text_reply()->set_message_text(
            untrimmed_message
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kPictureReply) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->set_message_text(gen_random_alpha_numeric_string((rand() % 100) + 5));
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(true);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            bsoncxx::oid{}.to_string()
    );
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            generateUUID()
    );

    const std::string thumbnail = gen_random_alpha_numeric_string((rand() % 1000) + 10);

    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_in_bytes(
            thumbnail
    );

    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_file_size(
            thumbnail.size()
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()
    );

    //message was moved out
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_in_bytes(
            thumbnail
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kMimeReply) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->set_message_text(gen_random_alpha_numeric_string((rand() % 100) + 5));
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(true);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            bsoncxx::oid{}.to_string()
    );
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            generateUUID()
    );

    const std::string thumbnail = gen_random_alpha_numeric_string((rand() % 1000) + 10);

    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_in_bytes(
            thumbnail
    );

    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_file_size(
            thumbnail.size()
    );

    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_mime_type(
            gen_random_alpha_numeric_string((rand() % 190) + 10)
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()
    );

    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_in_bytes(
            thumbnail
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kLocationReply) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->set_message_text(gen_random_alpha_numeric_string((rand() % 100) + 5));
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(true);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            bsoncxx::oid{}.to_string()
    );
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            generateUUID()
    );

    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_location_reply();

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, kInviteReply) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->set_message_text(gen_random_alpha_numeric_string((rand() % 100) + 5));
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(true);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            bsoncxx::oid{}.to_string()
    );
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            generateUUID()
    );

    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_invite_reply();

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_GE(result.time_stored_on_server.count(), current_timestamp.count());

    compareFunctionResult(request);
}

TEST_F(SendMessageToChatRoomTests, REPLY_BODY_NOT_SET) {
    grpc_chat_commands::ClientMessageToServerRequest request;

    request.set_message_uuid(message_uuid);
    request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_text_message()->set_message_text(gen_random_alpha_numeric_string((rand() % 100) + 5));
    message_specifics->mutable_text_message()->set_is_edited(false);
    message_specifics->mutable_text_message()->set_edited_time(-1L);

    message_specifics->mutable_text_message()->mutable_active_message_info()->set_is_reply(true);
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            bsoncxx::oid{}.to_string()
    );
    message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            generateUUID()
    );

    SendMessageToChatRoomReturn result = sendMessageToChatRoom(
            &request,
            chat_room_collection,
            user_account_oid,
            nullptr,
            "",
            "",
            std::chrono::milliseconds{-1},
            message_specifics->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()
    );

    EXPECT_EQ(result.successful, SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED);
    EXPECT_TRUE(result.picture_oid.empty());
    EXPECT_EQ(result.time_stored_on_server, std::chrono::milliseconds{-1});
}

TEST_F(UnMatchHelperTests, nullSession) {

    ReturnStatus return_status;
    std::chrono::milliseconds timestamp_stored{-2};

    auto set_return_status = [&](const ReturnStatus& _return_status,
                                  const std::chrono::milliseconds& _timestamp_stored) {
        return_status = _return_status;
        timestamp_stored = _timestamp_stored;
    };

    bsoncxx::builder::stream::document user_account_doc_builder;
    values.user_account.convertToDocument(user_account_doc_builder);

    bool return_val = unMatchHelper(
            accounts_db,
            chat_room_db,
            nullptr,
            user_accounts_collection,
            user_account_doc_builder.view(),
            values.generated_account_oid,
            values.second_generated_account_oid,
            values.matching_chat_room_id,
            values.current_timestamp,
            set_return_status
   );

    EXPECT_FALSE(return_val);
    EXPECT_EQ(return_status, ReturnStatus::LG_ERROR);
    EXPECT_EQ(timestamp_stored, std::chrono::milliseconds{-1L});
}

TEST_F(UnMatchHelperTests, standardCase) {

    ReturnStatus return_status;
    std::chrono::milliseconds timestamp_stored{-2};

    auto set_return_status = [&](
        const ReturnStatus& _return_status,
        const std::chrono::milliseconds& _timestamp_stored
    ) {
        return_status = _return_status;
        timestamp_stored = _timestamp_stored;
    };

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session
        ) {

        bsoncxx::builder::stream::document user_account_doc_builder;
        values.user_account.convertToDocument(user_account_doc_builder);

        bool return_val = unMatchHelper(
                accounts_db,
                chat_room_db,
                session,
                user_accounts_collection,
                user_account_doc_builder.view(),
                values.generated_account_oid,
                values.second_generated_account_oid,
                values.matching_chat_room_id,
                values.current_timestamp,
                set_return_status
        );

        EXPECT_TRUE(return_val);
    };

    mongocxx::client_session transaction_session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        transaction_session.with_transaction(transactionCallback);
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "");
    }

    EXPECT_EQ(return_status, ReturnStatus::SUCCESS);
    EXPECT_EQ(timestamp_stored, values.current_timestamp);

    checkIfUnmatchSuccessful(
            values.original_chat_room_header,
            values.current_timestamp,
            values.user_account,
            values.second_user_account,
            values.thumbnail_picture,
            values.second_thumbnail_picture,
            values.matching_chat_room_id,
            values.generated_account_oid,
            values.second_generated_account_oid
    );

}

TEST_F(UnMatchHelperTests, thumbnailsHaveBeenUpdated) {

    setfields::SetPictureRequest request;
    setfields::SetFieldResponse response;

    setupUserLoginInfo(
            request.mutable_login_info(),
            values.generated_account_oid,
            values.user_account.logged_in_token,
            values.user_account.installation_ids.front()
    );

    std::string new_picture = "new_picture string";
    std::string new_thumbnail = "thumb string";

    request.set_file_in_bytes(new_picture);
    request.set_file_size((int) new_picture.size());
    request.set_thumbnail_in_bytes(new_thumbnail);
    request.set_thumbnail_size((int) new_thumbnail.size());
    request.set_picture_array_index(0);

    //Set a new thumbnail for the first user.
    setPicture(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    //Must update user account after picture changed.
    values.user_account.getFromCollection(values.generated_account_oid);

    request.Clear();
    response.Clear();

    setupUserLoginInfo(
            request.mutable_login_info(),
            values.second_generated_account_oid,
            values.second_user_account.logged_in_token,
            values.second_user_account.installation_ids.front()
    );

    request.set_file_in_bytes(new_picture);
    request.set_file_size((int) new_picture.size());
    request.set_thumbnail_in_bytes(new_thumbnail);
    request.set_thumbnail_size((int) new_thumbnail.size());
    request.set_picture_array_index(0);

    //Set a new thumbnail for the second user.
    setPicture(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    //Must update user account after picture changed.
    values.second_user_account.getFromCollection(values.second_generated_account_oid);

    ReturnStatus return_status;
    std::chrono::milliseconds timestamp_stored{-2};

    const auto set_return_status = [&](
            const ReturnStatus& _return_status,
            const std::chrono::milliseconds& _timestamp_stored
    ) {
        return_status = _return_status;
        timestamp_stored = _timestamp_stored;
    };

    bsoncxx::oid stored_picture_reference;
    bsoncxx::types::b_date timestamp_pic_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    values.user_account.pictures[0].getPictureReference(
            stored_picture_reference,
            timestamp_pic_stored
    );

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session
    ) {

        bsoncxx::builder::stream::document user_account_doc_builder;
        values.user_account.convertToDocument(user_account_doc_builder);

        bool return_val = unMatchHelper(
                accounts_db,
                chat_room_db,
                session,
                user_accounts_collection,
                user_account_doc_builder.view(),
                values.generated_account_oid,
                values.second_generated_account_oid,
                values.matching_chat_room_id,
                values.current_timestamp,
                set_return_status
        );

        EXPECT_TRUE(return_val);
    };

    mongocxx::client_session transaction_session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        transaction_session.with_transaction(transactionCallback);
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "");
    }

    EXPECT_EQ(return_status, ReturnStatus::SUCCESS);
    EXPECT_EQ(timestamp_stored, values.current_timestamp);

    values.user_account.other_accounts_matched_with.pop_back();
    values.user_account.chat_rooms.pop_back();
    values.second_user_account.other_accounts_matched_with.pop_back();
    values.second_user_account.chat_rooms.pop_back();

    UserAccountDoc extracted_first_user_account(values.generated_account_oid);
    UserAccountDoc extracted_second_user_account(values.second_generated_account_oid);

    values.user_account.pictures[0] = extracted_first_user_account.pictures[0];
    values.second_user_account.pictures[0] = extracted_second_user_account.pictures[0];

    EXPECT_EQ(values.user_account, extracted_first_user_account);
    EXPECT_EQ(values.second_user_account, extracted_second_user_account);

    UserPictureDoc extracted_original_first_thumbnail_picture(values.thumbnail_picture.current_object_oid);
    UserPictureDoc extracted_original_second_thumbnail_picture(values.second_thumbnail_picture.current_object_oid);

    //make sure old thumbnail_reference was removed
    EXPECT_TRUE(extracted_original_first_thumbnail_picture.thumbnail_references.empty());
    EXPECT_TRUE(extracted_original_second_thumbnail_picture.thumbnail_references.empty());

    values.user_account.pictures[0].getPictureReference(
            stored_picture_reference,
            timestamp_pic_stored
    );

    UserPictureDoc extracted_first_thumbnail_picture(stored_picture_reference);

    values.second_user_account.pictures[0].getPictureReference(
            stored_picture_reference,
            timestamp_pic_stored
    );

    UserPictureDoc extracted_second_thumbnail_picture(stored_picture_reference);

    //make sure that new thumbnail references were added
    EXPECT_EQ(extracted_first_thumbnail_picture.thumbnail_references.size(), 1);
    if(extracted_first_thumbnail_picture.thumbnail_references.size() == 1) {
        EXPECT_EQ(extracted_first_thumbnail_picture.thumbnail_references[0], values.matching_chat_room_id);
    }
    EXPECT_EQ(extracted_second_thumbnail_picture.thumbnail_references.size(), 1);
    if(extracted_second_thumbnail_picture.thumbnail_references.size() == 1) {
        EXPECT_EQ(extracted_second_thumbnail_picture.thumbnail_references[0], values.matching_chat_room_id);
    }

    values.original_chat_room_header.matching_oid_strings = nullptr;
    values.original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{values.current_timestamp};

    EXPECT_EQ(values.original_chat_room_header.accounts_in_chat_room.size(), 2);
    if(values.original_chat_room_header.accounts_in_chat_room.size() >= 2) {

        values.original_chat_room_header.accounts_in_chat_room[0].thumbnail_reference = extracted_first_thumbnail_picture.current_object_oid.to_string();
        values.original_chat_room_header.accounts_in_chat_room[0].thumbnail_size = extracted_first_thumbnail_picture.thumbnail_size_in_bytes;
        values.original_chat_room_header.accounts_in_chat_room[0].last_activity_time = bsoncxx::types::b_date{
                values.current_timestamp};

        values.original_chat_room_header.accounts_in_chat_room[1].thumbnail_reference = extracted_second_thumbnail_picture.current_object_oid.to_string();
        values.original_chat_room_header.accounts_in_chat_room[1].thumbnail_size = extracted_second_thumbnail_picture.thumbnail_size_in_bytes;

        for (int i = 0; i < 2; ++i) {
            values.original_chat_room_header.accounts_in_chat_room[i].state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
            values.original_chat_room_header.accounts_in_chat_room[i].thumbnail_timestamp = bsoncxx::types::b_date{
                    values.current_timestamp};
            values.original_chat_room_header.accounts_in_chat_room[i].times_joined_left.emplace_back(
                    bsoncxx::types::b_date{values.current_timestamp}
            );
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(values.matching_chat_room_id);

    EXPECT_EQ(values.original_chat_room_header, extracted_chat_room_header);
}

TEST_F(UnMatchHelperTests, matchNoLongerExists) {

    values.second_user_account.other_accounts_matched_with.pop_back();
    values.second_user_account.chat_rooms.pop_back();
    values.second_user_account.setIntoCollection();

    EXPECT_EQ(values.original_chat_room_header.accounts_in_chat_room.size(), 2);
    if(values.original_chat_room_header.accounts_in_chat_room.size() == 2) {
        values.original_chat_room_header.accounts_in_chat_room[1].times_joined_left.emplace_back(
                bsoncxx::types::b_date{values.current_timestamp}
                );
        values.original_chat_room_header.accounts_in_chat_room[1].state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
        values.original_chat_room_header.setIntoCollection();
    }

    ReturnStatus return_status;
    std::chrono::milliseconds timestamp_stored{-2};

    auto set_return_status = [&](
            const ReturnStatus& _return_status,
            const std::chrono::milliseconds& _timestamp_stored
    ) {
        return_status = _return_status;
        timestamp_stored = _timestamp_stored;
    };

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session
    ) {

        bsoncxx::builder::stream::document user_account_doc_builder;
        values.user_account.convertToDocument(user_account_doc_builder);

        bool return_val = unMatchHelper(
                accounts_db,
                chat_room_db,
                session,
                user_accounts_collection,
                user_account_doc_builder.view(),
                values.generated_account_oid,
                values.second_generated_account_oid,
                values.matching_chat_room_id,
                values.current_timestamp,
                set_return_status
        );

        EXPECT_TRUE(return_val);
    };

    mongocxx::client_session transaction_session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        transaction_session.with_transaction(transactionCallback);
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "");
    }

    EXPECT_EQ(return_status, ReturnStatus::SUCCESS);
    EXPECT_EQ(timestamp_stored, values.current_timestamp);

    values.original_chat_room_header.matching_oid_strings = nullptr;
    values.original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{values.current_timestamp};

    EXPECT_EQ(values.original_chat_room_header.accounts_in_chat_room.size(), 2);
    if(values.original_chat_room_header.accounts_in_chat_room.size() >= 2) {
        values.original_chat_room_header.accounts_in_chat_room[0].last_activity_time = bsoncxx::types::b_date{
                values.current_timestamp};
        values.original_chat_room_header.accounts_in_chat_room[0].state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
        values.original_chat_room_header.accounts_in_chat_room[0].thumbnail_timestamp = bsoncxx::types::b_date{
                values.current_timestamp};
        values.original_chat_room_header.accounts_in_chat_room[0].times_joined_left.emplace_back(
                bsoncxx::types::b_date{values.current_timestamp}
        );
    }

    ChatRoomHeaderDoc extracted_chat_room_header(values.matching_chat_room_id);

    EXPECT_EQ(values.original_chat_room_header, extracted_chat_room_header);

    values.user_account.other_accounts_matched_with.pop_back();
    values.user_account.chat_rooms.pop_back();

    UserAccountDoc extracted_first_user_account(values.generated_account_oid);
    UserAccountDoc extracted_second_user_account(values.second_generated_account_oid);

    EXPECT_EQ(values.user_account, extracted_first_user_account);
    EXPECT_EQ(values.second_user_account, extracted_second_user_account);

    UserPictureDoc extracted_first_thumbnail_picture(values.thumbnail_picture.current_object_oid);
    UserPictureDoc extracted_second_thumbnail_picture(values.second_thumbnail_picture.current_object_oid);

    EXPECT_EQ(values.thumbnail_picture, extracted_first_thumbnail_picture);
    EXPECT_EQ(values.second_thumbnail_picture, extracted_second_thumbnail_picture);

}

TEST_F(UpdateSingleChatRoomMemberNotInChatRoomTests, requiresAllUpdates) {

    UserPictureDoc user_picture;

    user_picture.user_account_reference = bsoncxx::oid{};
    user_picture.picture_index = 1;
    user_picture.thumbnail_in_bytes = gen_random_alpha_numeric_string(rand() % 100 + 830);
    user_picture.thumbnail_size_in_bytes = (int)user_picture.thumbnail_in_bytes.size();

    user_picture.setIntoCollection();

    EXPECT_NE(user_picture.current_object_oid.to_string(), "000000000000000000000000");

    const std::string member_to_be_updated_account_oid = bsoncxx::oid{}.to_string();
    const std::string new_first_name = "new";
    const std::chrono::milliseconds thumbnail_timestamp{1000};
    const std::chrono::milliseconds user_last_activity_time{150};
    const std::string chat_room_id = generateRandomChatRoomId();

    const auto new_account_state = AccountStateInChatRoom::ACCOUNT_STATE_IS_SUPER_ADMIN;

    bsoncxx::builder::stream::document user_doc_builder;

    user_doc_builder
        << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << bsoncxx::types::b_date{thumbnail_timestamp}
        << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << (int)user_picture.thumbnail_size_in_bytes
        << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << user_picture.current_object_oid.to_string()
        << chat_room_header_keys::accounts_in_chat_room::FIRST_NAME << new_first_name;

    OtherUserInfoForUpdates passed_member_info;

    passed_member_info.set_thumbnail_timestamp(thumbnail_timestamp.count()-1);
    passed_member_info.set_thumbnail_size_in_bytes((int)user_picture.thumbnail_size_in_bytes+1);
    passed_member_info.set_first_name("old");
    passed_member_info.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM);

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UpdateOtherUserResponse passed_response;
    bool response_function_called = false;

    mongocxx::collection chat_room_collection = accounts_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    updateSingleChatRoomMemberNotInChatRoom(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            member_to_be_updated_account_oid,
            user_doc_builder.view(),
            passed_member_info,
            current_timestamp,
            false,
            new_account_state,
            user_last_activity_time,
            &passed_response,
            [&response_function_called]() {
                response_function_called = true;
            }
    );

    UpdateOtherUserResponse generated_response;

    generated_response.mutable_user_info()->set_account_thumbnail_size(user_picture.thumbnail_size_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_timestamp.count());
    generated_response.mutable_user_info()->set_account_thumbnail(user_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_index(0);

    generated_response.mutable_user_info()->set_account_name(new_first_name);

    generated_response.set_return_status(ReturnStatus::SUCCESS);
    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_state(new_account_state);
    generated_response.set_account_last_activity_time(user_last_activity_time.count());
    generated_response.mutable_user_info()->set_account_oid(member_to_be_updated_account_oid);
    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.mutable_user_info()->set_account_type(UserAccountType::USER_ACCOUNT_TYPE);

    EXPECT_TRUE(response_function_called);

    bool messages_equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            generated_response,
            passed_response
    );

    if(!messages_equivalent) {
        std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
        std::cout << "passed_response\n" << passed_response.DebugString() << '\n';
    }

    EXPECT_TRUE(messages_equivalent);
}

TEST_F(UpdateSingleChatRoomMemberNotInChatRoomTests, requiresNoUpdates) {
    const std::string member_to_be_updated_account_oid = bsoncxx::oid{}.to_string();
    const std::string new_thumbnail = "12345678";
    const std::string new_first_name = "new";
    const std::chrono::milliseconds thumbnail_timestamp{1000};
    const std::chrono::milliseconds user_last_activity_time{150};
    const std::string chat_room_id = generateRandomChatRoomId();

    const auto new_account_state = AccountStateInChatRoom::ACCOUNT_STATE_IS_SUPER_ADMIN;

    bsoncxx::builder::stream::document user_doc_builder;

    user_doc_builder
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << bsoncxx::types::b_date{thumbnail_timestamp}
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << (int)new_thumbnail.size()
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << bsoncxx::oid{}.to_string()
            << chat_room_header_keys::accounts_in_chat_room::FIRST_NAME << new_first_name;

    OtherUserInfoForUpdates passed_member_info;

    passed_member_info.set_thumbnail_timestamp(thumbnail_timestamp.count());
    passed_member_info.set_thumbnail_size_in_bytes((int)new_thumbnail.size());
    passed_member_info.set_first_name(new_first_name);
    passed_member_info.set_account_state(new_account_state);

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UpdateOtherUserResponse passed_response;
    bool response_function_called = false;

    mongocxx::collection chat_room_collection = accounts_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    updateSingleChatRoomMemberNotInChatRoom(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            member_to_be_updated_account_oid,
            user_doc_builder.view(),
            passed_member_info,
            current_timestamp,
            false,
            new_account_state,
            user_last_activity_time,
            &passed_response,
            [&response_function_called]() {
                response_function_called = true;
            }
    );

    UpdateOtherUserResponse generated_response;

    EXPECT_FALSE(response_function_called);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_response,
                    passed_response
            )
    );
}

TEST_F(UpdateSingleChatRoomMemberNotInChatRoomTests, onlyFirstNameUpdate) {
    const std::string member_to_be_updated_account_oid = bsoncxx::oid{}.to_string();
    const std::string new_thumbnail = "12345678";
    const std::string new_first_name = "new";
    const std::chrono::milliseconds thumbnail_timestamp{1000};
    const std::chrono::milliseconds user_last_activity_time{150};
    const std::string chat_room_id = generateRandomChatRoomId();

    const auto new_account_state = AccountStateInChatRoom::ACCOUNT_STATE_IS_SUPER_ADMIN;

    bsoncxx::builder::stream::document user_doc_builder;

    user_doc_builder
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << bsoncxx::types::b_date{thumbnail_timestamp}
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << (int)new_thumbnail.size()
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << bsoncxx::oid{}.to_string()
            << chat_room_header_keys::accounts_in_chat_room::FIRST_NAME << new_first_name;

    OtherUserInfoForUpdates passed_member_info;

    passed_member_info.set_thumbnail_timestamp(thumbnail_timestamp.count());
    passed_member_info.set_thumbnail_size_in_bytes((int)new_thumbnail.size());
    passed_member_info.set_first_name("old");
    passed_member_info.set_account_state(new_account_state);

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UpdateOtherUserResponse passed_response;
    bool response_function_called = false;

    mongocxx::collection chat_room_collection = accounts_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    updateSingleChatRoomMemberNotInChatRoom(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            member_to_be_updated_account_oid,
            user_doc_builder.view(),
            passed_member_info,
            current_timestamp,
            false,
            new_account_state,
            user_last_activity_time,
            &passed_response,
            [&response_function_called]() {
                response_function_called = true;
            }
    );

    UpdateOtherUserResponse generated_response;

    generated_response.mutable_user_info()->set_account_name(new_first_name);

    generated_response.set_return_status(ReturnStatus::SUCCESS);
    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_state(new_account_state);
    generated_response.set_account_last_activity_time(user_last_activity_time.count());
    generated_response.mutable_user_info()->set_account_oid(member_to_be_updated_account_oid);
    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.mutable_user_info()->set_account_type(UserAccountType::USER_ACCOUNT_TYPE);

    EXPECT_TRUE(response_function_called);

    bool messages_equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            generated_response,
            passed_response
    );

    if(!messages_equivalent) {
        std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
        std::cout << "passed_response\n" << passed_response.DebugString() << '\n';
    }

    EXPECT_TRUE(messages_equivalent);
}

TEST_F(UpdateSingleChatRoomMemberNotInChatRoomTests, onlyThumbnailUpdate) {

    UserPictureDoc user_picture;

    user_picture.user_account_reference = bsoncxx::oid{};
    user_picture.picture_index = 1;
    user_picture.thumbnail_in_bytes = gen_random_alpha_numeric_string(rand() % 100 + 830);
    user_picture.thumbnail_size_in_bytes = user_picture.thumbnail_in_bytes.size();

    user_picture.setIntoCollection();

    EXPECT_NE(user_picture.current_object_oid.to_string(), "000000000000000000000000");

    const std::string member_to_be_updated_account_oid = bsoncxx::oid{}.to_string();
    const std::string new_first_name = "new";
    const std::chrono::milliseconds thumbnail_timestamp{1000};
    const std::chrono::milliseconds user_last_activity_time{150};
    const std::string chat_room_id = generateRandomChatRoomId();

    const auto new_account_state = AccountStateInChatRoom::ACCOUNT_STATE_IS_SUPER_ADMIN;

    bsoncxx::builder::stream::document user_doc_builder;

    user_doc_builder
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << bsoncxx::types::b_date{thumbnail_timestamp}
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << user_picture.thumbnail_size_in_bytes
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << user_picture.current_object_oid.to_string()
            << chat_room_header_keys::accounts_in_chat_room::FIRST_NAME << new_first_name;

    OtherUserInfoForUpdates passed_member_info;

    passed_member_info.set_thumbnail_timestamp(thumbnail_timestamp.count()-1);
    passed_member_info.set_thumbnail_size_in_bytes(user_picture.thumbnail_size_in_bytes +1);
    passed_member_info.set_first_name(new_first_name);
    passed_member_info.set_account_state(new_account_state);

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UpdateOtherUserResponse passed_response;
    bool response_function_called = false;

    mongocxx::collection chat_room_collection = accounts_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    updateSingleChatRoomMemberNotInChatRoom(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            member_to_be_updated_account_oid,
            user_doc_builder.view(),
            passed_member_info,
            current_timestamp,
            false,
            new_account_state,
            user_last_activity_time,
            &passed_response,
            [&response_function_called]() {
                response_function_called = true;
            }
    );

    UpdateOtherUserResponse generated_response;

    generated_response.mutable_user_info()->set_account_thumbnail_size(user_picture.thumbnail_size_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_timestamp.count());
    generated_response.mutable_user_info()->set_account_thumbnail(user_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_index(0);

    generated_response.set_return_status(ReturnStatus::SUCCESS);
    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_state(new_account_state);
    generated_response.set_account_last_activity_time(user_last_activity_time.count());
    generated_response.mutable_user_info()->set_account_oid(member_to_be_updated_account_oid);
    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.mutable_user_info()->set_account_type(UserAccountType::USER_ACCOUNT_TYPE);

    EXPECT_TRUE(response_function_called);

    bool messages_equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            generated_response,
            passed_response
    );

    if(!messages_equivalent) {
        std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
        std::cout << "passed_response\n" << passed_response.DebugString() << '\n';
    }

    EXPECT_TRUE(messages_equivalent);
}

TEST_F(UpdateSingleChatRoomMemberNotInChatRoomTests, onlyAccountStateUpdate) {
    const std::string member_to_be_updated_account_oid = bsoncxx::oid{}.to_string();
    const std::string new_thumbnail = "12345678";
    const std::string new_first_name = "new";
    const std::chrono::milliseconds thumbnail_timestamp{1000};
    const std::chrono::milliseconds user_last_activity_time{150};
    const std::string chat_room_id = generateRandomChatRoomId();

    const auto new_account_state = AccountStateInChatRoom::ACCOUNT_STATE_IS_SUPER_ADMIN;

    bsoncxx::builder::stream::document user_doc_builder;

    user_doc_builder
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << bsoncxx::types::b_date{thumbnail_timestamp}
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << (int)new_thumbnail.size()
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << bsoncxx::oid{}.to_string()
            << chat_room_header_keys::accounts_in_chat_room::FIRST_NAME << new_first_name;

    OtherUserInfoForUpdates passed_member_info;

    passed_member_info.set_thumbnail_timestamp(thumbnail_timestamp.count());
    passed_member_info.set_thumbnail_size_in_bytes((int)new_thumbnail.size());
    passed_member_info.set_first_name(new_first_name);
    passed_member_info.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM);

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UpdateOtherUserResponse passed_response;
    bool response_function_called = false;

    mongocxx::collection chat_room_collection = accounts_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    updateSingleChatRoomMemberNotInChatRoom(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            member_to_be_updated_account_oid,
            user_doc_builder.view(),
            passed_member_info,
            current_timestamp,
            false,
            new_account_state,
            user_last_activity_time,
            &passed_response,
            [&response_function_called]() {
                response_function_called = true;
            }
    );

    UpdateOtherUserResponse generated_response;

    generated_response.set_return_status(ReturnStatus::SUCCESS);
    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_state(new_account_state);
    generated_response.set_account_last_activity_time(user_last_activity_time.count());
    generated_response.mutable_user_info()->set_account_oid(member_to_be_updated_account_oid);
    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.mutable_user_info()->set_account_type(UserAccountType::USER_ACCOUNT_TYPE);

    EXPECT_TRUE(response_function_called);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_response,
                    passed_response
            )
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, validInfo) {
    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
            );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidAccountOid) {

    chat_room_member_info[0].set_account_oid("other_user_oid");

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    EXPECT_TRUE(users_to_be_updated.empty());
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidAccountState) {
    chat_room_member_info[0].set_account_state(AccountStateInChatRoom(-1));

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    EXPECT_TRUE(users_to_be_updated.empty());
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidThumbnailSizeInBytes) {

    chat_room_member_info[0].set_thumbnail_size_in_bytes(-123);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info[0].set_thumbnail_size_in_bytes(0);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidThumbnailIndexNumber_tooSmall) {
    chat_room_member_info[0].set_thumbnail_index_number(-5);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info[0].set_thumbnail_index_number(0);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidThumbnailIndexNumber_tooLarge) {
    chat_room_member_info[0].set_thumbnail_index_number(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info[0].set_thumbnail_index_number(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT - 1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidThumbnailTimestamp) {

    chat_room_member_info[0].set_thumbnail_index_number(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info[0].set_thumbnail_index_number(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT - 1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );

}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidFirstName) {
    chat_room_member_info[0].set_first_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_FIRST_NAME + 1 + rand() % 100));

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info[0].set_first_name("");

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidAge_tooLow) {
    chat_room_member_info[0].set_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE - 1);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info[0].set_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidAge_tooHigh) {
    chat_room_member_info[0].set_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info[0].set_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidMemberInfoLastUpdatedTimestamp_tooHigh) {
    chat_room_member_info[0].set_member_info_last_updated_timestamp(getCurrentTimestamp().count() * 1000);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info[0].set_member_info_last_updated_timestamp(current_timestamp.count());

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidMemberInfoLastUpdatedTimestamp_tooLow) {
    chat_room_member_info[0].set_member_info_last_updated_timestamp(general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP.count() - 1);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info[0].set_member_info_last_updated_timestamp(-1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, validPictureIndexInfo) {
    auto picture_last_updated_timestamp = chat_room_member_info[0].add_pictures_last_updated_timestamps();

    picture_last_updated_timestamp->set_index_number(1);
    picture_last_updated_timestamp->set_last_updated_timestamp(current_timestamp.count());

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidPictureIndexInfo_index_tooLow) {
    auto picture_last_updated_timestamp = chat_room_member_info[0].add_pictures_last_updated_timestamps();

    picture_last_updated_timestamp->set_index_number(-1);
    picture_last_updated_timestamp->set_last_updated_timestamp(current_timestamp.count());

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    chat_room_member_info[0].mutable_pictures_last_updated_timestamps()->erase(
            chat_room_member_info[0].mutable_pictures_last_updated_timestamps()->begin()
            );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidPictureIndexInfo_index_tooHigh) {
    auto picture_last_updated_timestamp = chat_room_member_info[0].add_pictures_last_updated_timestamps();

    picture_last_updated_timestamp->set_index_number(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT + 1);
    picture_last_updated_timestamp->set_last_updated_timestamp(current_timestamp.count());

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    chat_room_member_info[0].mutable_pictures_last_updated_timestamps()->erase(
            chat_room_member_info[0].mutable_pictures_last_updated_timestamps()->begin()
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidPictureIndexInfo_lastUpdatedTimestamp_tooLow) {
    auto picture_last_updated_timestamp = chat_room_member_info[0].add_pictures_last_updated_timestamps();

    picture_last_updated_timestamp->set_index_number(1);
    picture_last_updated_timestamp->set_last_updated_timestamp(general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP.count() - 1);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    chat_room_member_info[0].mutable_pictures_last_updated_timestamps(0)->set_last_updated_timestamp(general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP.count());

    ASSERT_EQ(users_to_be_updated.size(), 1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, invalidPictureIndexInfo_lastUpdatedTimestamp_tooHigh) {
    auto picture_last_updated_timestamp = chat_room_member_info[0].add_pictures_last_updated_timestamps();

    picture_last_updated_timestamp->set_index_number(1);
    picture_last_updated_timestamp->set_last_updated_timestamp(getCurrentTimestamp().count() * 1000);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    chat_room_member_info[0].mutable_pictures_last_updated_timestamps(0)->set_last_updated_timestamp(current_timestamp.count());

    ASSERT_EQ(users_to_be_updated.size(), 1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, duplicateOtherUserInfoForUpdated) {
    addRandomOtherUserInfo();

    std::cout << chat_room_member_info.size() << '\n';

    chat_room_member_info[1].set_account_oid(
            chat_room_member_info[0].account_oid()
            );

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    chat_room_member_info.erase(
            chat_room_member_info.begin() + 1
            );

    EXPECT_EQ(chat_room_member_info.size(), users_to_be_updated.size());
    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}

TEST_F(FilterAndStoreUsersToBeUpdatedTests, duplicatePictureIndexInfo) {
    auto picture_last_updated_timestamp = chat_room_member_info[0].add_pictures_last_updated_timestamps();

    picture_last_updated_timestamp->set_index_number(1);
    picture_last_updated_timestamp->set_last_updated_timestamp(current_timestamp.count());

    picture_last_updated_timestamp = chat_room_member_info[0].add_pictures_last_updated_timestamps();

    picture_last_updated_timestamp->set_index_number(1);
    picture_last_updated_timestamp->set_last_updated_timestamp(current_timestamp.count() - 1);

    std::vector<OtherUserInfoForUpdates> users_to_be_updated = filterAndStoreListOfUsersToBeUpdated(
            chat_room_member_info,
            current_timestamp
    );

    chat_room_member_info[0].mutable_pictures_last_updated_timestamps()->erase(
            chat_room_member_info[0].mutable_pictures_last_updated_timestamps()->begin() + 1
            );

    ASSERT_EQ(users_to_be_updated.size(), 1);

    compareMessageEquivalent(
            users_to_be_updated[0],
            chat_room_member_info[0]
    );
}
