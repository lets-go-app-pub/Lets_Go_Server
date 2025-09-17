//
// Created by jeremiah on 6/16/22.
//

#include <gtest/gtest.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <admin_account_keys.h>
#include <clear_database_for_testing.h>
#include <generate_multiple_random_accounts.h>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <chat_rooms_objects.h>
#include <utility_chat_functions.h>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/client_session.hpp>

#include "create_chat_room_helper.h"
#include "utility_general_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class CreateChatRoomHelper : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(CreateChatRoomHelper, generateChatRoomId) {
    insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    std::string generated_chat_room_id;

    ChatRoomInfoDoc before_chat_room_info(true);

    bool return_val = generateChatRoomId(chat_room_db, generated_chat_room_id);

    EXPECT_TRUE(return_val);

    EXPECT_FALSE(isInvalidChatRoomId(generated_chat_room_id));

    before_chat_room_info.previously_used_chat_room_number++;
    ChatRoomInfoDoc after_chat_room_info(true);

    EXPECT_EQ(before_chat_room_info, after_chat_room_info);
}

TEST_F(CreateChatRoomHelper, createChatRoomFromId) {
    insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    std::string generated_chat_room_id;

    generateChatRoomId(chat_room_db, generated_chat_room_id);

    ASSERT_FALSE(isInvalidChatRoomId(generated_chat_room_id));

    std::string chat_room_name = "chat_room_name";
    std::string chat_room_cap_message_uuid = generateUUID();
    bsoncxx::oid chat_room_created_by_oid{};
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const GenerateNewChatRoomTimes chat_room_times(current_timestamp);

    std::string returned_chat_room_id;
    std::string returned_chat_room_password;

    auto set_return_status = [&](const std::string& chatRoomId,
            const std::string& _chat_room_password) {
        returned_chat_room_id = chatRoomId;
        returned_chat_room_password = _chat_room_password;
    };

    bsoncxx::builder::basic::array header_accounts_in_chat_room_array;

    header_accounts_in_chat_room_array.append(
            createChatRoomHeaderUserDoc(
                    chat_room_created_by_oid,
                    AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
                    "user_name",
                    18,
                    bsoncxx::oid{}.to_string(),
                    bsoncxx::types::b_date{std::chrono::milliseconds(chat_room_times.chat_room_last_active_time)},
                    chat_room_times.chat_room_last_active_time
                    )
            );
    //NOTE: this transaction is here to avoid inconsistencies between the chat room header and the user account info
    mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* session) {

        bsoncxx::builder::basic::array matching_array;

        bool return_val = createChatRoomFromId(
                header_accounts_in_chat_room_array,
                chat_room_name,
                chat_room_cap_message_uuid,
                chat_room_created_by_oid,
                chat_room_times,
                session,
                set_return_status,
                matching_array,
                chat_room_db,
                generated_chat_room_id
                );

        EXPECT_TRUE(return_val);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
        ASSERT_TRUE(false);
    }

    ChatRoomHeaderDoc generated_chat_room_header;

    bsoncxx::builder::stream::document gen_chat_room_header;

    gen_chat_room_header
        << "_id" << chat_room_header_keys::ID
        << chat_room_header_keys::CHAT_ROOM_NAME << chat_room_name
        << chat_room_header_keys::CHAT_ROOM_PASSWORD << returned_chat_room_password
        << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{chat_room_times.chat_room_created_time}
        << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{}
        << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time}
        << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << header_accounts_in_chat_room_array;

    generated_chat_room_header.convertDocumentToClass(gen_chat_room_header, returned_chat_room_id);

    ChatRoomHeaderDoc extracted_chat_room_header(returned_chat_room_id);

    EXPECT_EQ(extracted_chat_room_header, generated_chat_room_header);

    ChatRoomMessageDoc extracted_message_cap(chat_room_cap_message_uuid, returned_chat_room_id);
    EXPECT_EQ(extracted_message_cap.message_type, MessageSpecifics::MessageBodyCase::kChatRoomCapMessage);
}

TEST_F(CreateChatRoomHelper, createChatRoomFromId_sessionNullptr) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    bsoncxx::builder::basic::array header_accounts_in_chat_room_array;

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const GenerateNewChatRoomTimes chat_room_times(current_timestamp);

    auto set_return_status = [&](const std::string&,
            const std::string&) {
    };

    bsoncxx::builder::basic::array matching_array;

    std::string generated_chat_room_id = "12345678";
    std::string cap_message_uuid;

    bool return_val = createChatRoomFromId(
            header_accounts_in_chat_room_array,
            "chat_room_name",
            cap_message_uuid,
            bsoncxx::oid{},
            chat_room_times,
            nullptr,
            set_return_status,
            matching_array,
            chat_room_db,
            generated_chat_room_id
            );

    EXPECT_FALSE(return_val);

    ChatRoomHeaderDoc extracted_chat_room_header;

    EXPECT_FALSE(extracted_chat_room_header.getFromCollection(generated_chat_room_id));
}
