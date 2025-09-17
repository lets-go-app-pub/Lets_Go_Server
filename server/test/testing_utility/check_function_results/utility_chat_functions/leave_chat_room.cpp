//
// Created by jeremiah on 6/3/22.
//

#include <gtest/gtest.h>
#include "utility_chat_functions_test.h"

void checkLeaveChatRoomResult(
        const std::chrono::milliseconds& currentTimestamp,
        ChatRoomHeaderDoc& original_chat_room_header,
        const bsoncxx::oid& leaving_user_account_oid,
        const std::string& updated_thumbnail_reference,
        const std::string& updated_thumbnail,
        const std::string& chat_room_id,
        const ReturnStatus& return_status,
        const std::chrono::milliseconds& timestamp_stored,
        const std::chrono::milliseconds& expected_timestamp_stored,
        bool return_val
        ) {

    EXPECT_EQ(return_status, ReturnStatus::SUCCESS);
    EXPECT_EQ(timestamp_stored, expected_timestamp_stored);
    EXPECT_EQ(return_val, true);

    bsoncxx::types::b_date current_mongo_db_date = bsoncxx::types::b_date{currentTimestamp};

    if (currentTimestamp > original_chat_room_header.chat_room_last_active_time.value) {
        original_chat_room_header.chat_room_last_active_time = current_mongo_db_date;
    }

    original_chat_room_header.matching_oid_strings = nullptr;

    bool new_admin_required = false;

    for (ChatRoomHeaderDoc::AccountsInChatRoom& account_in_chat_room : original_chat_room_header.accounts_in_chat_room) {
        if (account_in_chat_room.account_oid == leaving_user_account_oid) {

            if (account_in_chat_room.state_in_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN) {
                new_admin_required = true;
            }

            account_in_chat_room.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;

            account_in_chat_room.thumbnail_timestamp = current_mongo_db_date;
            account_in_chat_room.thumbnail_reference = updated_thumbnail_reference;
            account_in_chat_room.thumbnail_size = updated_thumbnail.size();

            if (currentTimestamp > account_in_chat_room.last_activity_time.value) {
                account_in_chat_room.last_activity_time = current_mongo_db_date;
            }

            account_in_chat_room.times_joined_left.emplace_back(current_mongo_db_date);

            break;
        }
    }

    if (new_admin_required) {
        for (ChatRoomHeaderDoc::AccountsInChatRoom& account_in_chat_room : original_chat_room_header.accounts_in_chat_room) {
            if (account_in_chat_room.account_oid != leaving_user_account_oid
            || account_in_chat_room.state_in_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM) {
                account_in_chat_room.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN;
                break;
            }
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);

    //The leave_chat_room function will generate a new timestamp for some values when kDifferentUserLeftChatRoom
    // message is sent.
    original_chat_room_header.chat_room_last_active_time = extracted_chat_room_header.chat_room_last_active_time;

    if(!original_chat_room_header.accounts_in_chat_room.empty()) {
        original_chat_room_header.accounts_in_chat_room[0].thumbnail_timestamp = extracted_chat_room_header.accounts_in_chat_room[0].thumbnail_timestamp;
        original_chat_room_header.accounts_in_chat_room[0].last_activity_time = extracted_chat_room_header.accounts_in_chat_room[0].last_activity_time;

        if(!original_chat_room_header.accounts_in_chat_room[0].times_joined_left.empty()) {
            original_chat_room_header.accounts_in_chat_room[0].times_joined_left.back() =
                    extracted_chat_room_header.accounts_in_chat_room[0].times_joined_left.back();
        }
    }

    EXPECT_EQ(original_chat_room_header, extracted_chat_room_header);
}