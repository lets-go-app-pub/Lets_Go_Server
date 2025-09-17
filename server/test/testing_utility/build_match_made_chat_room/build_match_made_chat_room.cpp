//
// Created by jeremiah on 7/30/22.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <gtest/gtest.h>

#include "AccountState.grpc.pb.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "create_chat_room_helper.h"

#include "build_match_made_chat_room.h"
#include "user_account_keys.h"

void appendUserToHeader(
        bsoncxx::builder::basic::array& header_accounts_in_chat_room_array,
        const UserAccountDoc& user_account,
        AccountStateInChatRoom account_state,
        const std::chrono::milliseconds& chat_room_last_active_time,
        const std::string match_made_chat_room_id
) {

    bsoncxx::oid user_thumbnail_reference;
    int user_thumbnail_size = 0;

    for(const auto& pic : user_account.pictures) {
        if(pic.pictureStored()) {
            bsoncxx::types::b_date thumbnail_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    user_thumbnail_reference,
                    thumbnail_date
            );

            UserPictureDoc thumbnail_doc(user_thumbnail_reference);

            EXPECT_EQ(thumbnail_doc.current_object_oid.to_string(), user_thumbnail_reference.to_string());

            user_thumbnail_size = thumbnail_doc.thumbnail_size_in_bytes;

            thumbnail_doc.thumbnail_references.emplace_back(match_made_chat_room_id);
            thumbnail_doc.setIntoCollection();

            break;
        }
    }

    //append current user to chat room header
    header_accounts_in_chat_room_array.append(
            createChatRoomHeaderUserDoc(
                    user_account.current_object_oid,
                    account_state,
                    user_account.first_name,
                    user_thumbnail_size,
                    user_thumbnail_reference.to_string(),
                    bsoncxx::types::b_date{chat_room_last_active_time},
                    chat_room_last_active_time
            )
    );
}

//builds a random match made, returns the match made chat room id
std::string buildMatchMadeChatRoom(
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::oid& first_user_account_oid,
        const bsoncxx::oid& second_user_account_oid,
        UserAccountDoc& first_user_account,
        UserAccountDoc& second_user_account
) {

    const GenerateNewChatRoomTimes chat_room_times(current_timestamp);

    auto set_return_status = [](const std::string&, const std::string&) {};

    bsoncxx::builder::basic::array match_oid_strings;

    match_oid_strings.append(first_user_account_oid.to_string());
    match_oid_strings.append(second_user_account_oid.to_string());

    std::string cap_message_uuid;

    bsoncxx::builder::basic::array header_accounts_in_chat_room_array;

    std::string match_made_chat_room_id = gen_random_alpha_numeric_string(7 + rand() % 2);

    for(char& c : match_made_chat_room_id) {
        c = (char)tolower(c);
    }

    appendUserToHeader(
            header_accounts_in_chat_room_array,
            first_user_account,
            AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
            chat_room_times.chat_room_last_active_time,
            match_made_chat_room_id
    );

    appendUserToHeader(
            header_accounts_in_chat_room_array,
            second_user_account,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            chat_room_times.chat_room_last_active_time,
            match_made_chat_room_id
    );

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_account_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    //If this is not inside a session, create a new transaction.
    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* callback_session
    ) {
        bool chat_room_created = createChatRoomFromId(
                header_accounts_in_chat_room_array,
                "Matching Chat Room Name",
                cap_message_uuid,
                first_user_account_oid,
                chat_room_times,
                callback_session,
                set_return_status,
                match_oid_strings.view(),
                chat_room_db,
                match_made_chat_room_id
        );

        ASSERT_TRUE(chat_room_created);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "dummy_string");
    }

    first_user_account.other_accounts_matched_with.emplace_back(
            second_user_account_oid.to_string(),
            bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time}
            );

    first_user_account.chat_rooms.emplace_back(
            match_made_chat_room_id,
            bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time}
    );

    second_user_account.other_accounts_matched_with.emplace_back(
            first_user_account_oid.to_string(),
            bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time}
    );

    second_user_account.chat_rooms.emplace_back(
            match_made_chat_room_id,
            bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time}
    );

    first_user_account.setIntoCollection();
    second_user_account.setIntoCollection();

    return match_made_chat_room_id;
}