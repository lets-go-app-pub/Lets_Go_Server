//
// Created by jeremiah on 8/24/22.
//

#include <gtest/gtest.h>
#include "build_match_made_chat_room.h"

void checkIfUnmatchSuccessful(
        ChatRoomHeaderDoc& original_chat_room_header,
        const std::chrono::milliseconds& current_timestamp,
        UserAccountDoc& user_account,
        UserAccountDoc& second_user_account,
        UserPictureDoc& thumbnail_picture,
        UserPictureDoc& second_thumbnail_picture,
        const std::string& matching_chat_room_id,
        const bsoncxx::oid& generated_account_oid,
        const bsoncxx::oid& second_generated_account_oid
) {
    original_chat_room_header.matching_oid_strings = nullptr;
    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{current_timestamp};

    EXPECT_EQ(original_chat_room_header.accounts_in_chat_room.size(), 2);
    if(original_chat_room_header.accounts_in_chat_room.size() >= 2) {
        for (int i = 0; i < 2; ++i) {
            if(i == 0) {
                original_chat_room_header.accounts_in_chat_room[i].last_activity_time = bsoncxx::types::b_date{
                        current_timestamp};
            }
            original_chat_room_header.accounts_in_chat_room[i].state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
            original_chat_room_header.accounts_in_chat_room[i].thumbnail_timestamp = bsoncxx::types::b_date{
                    current_timestamp};
            original_chat_room_header.accounts_in_chat_room[i].times_joined_left.emplace_back(
                    bsoncxx::types::b_date{current_timestamp}
            );
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(matching_chat_room_id);

    EXPECT_EQ(original_chat_room_header, extracted_chat_room_header);

    user_account.other_accounts_matched_with.pop_back();
    user_account.chat_rooms.pop_back();
    second_user_account.other_accounts_matched_with.pop_back();
    second_user_account.chat_rooms.pop_back();

    UserAccountDoc extracted_first_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_first_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    UserPictureDoc extracted_first_thumbnail_picture(thumbnail_picture.current_object_oid);
    UserPictureDoc extracted_second_thumbnail_picture(second_thumbnail_picture.current_object_oid);

    EXPECT_EQ(thumbnail_picture, extracted_first_thumbnail_picture);
    EXPECT_EQ(second_thumbnail_picture, extracted_second_thumbnail_picture);
}