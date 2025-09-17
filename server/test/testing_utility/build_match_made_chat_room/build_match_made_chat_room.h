//
// Created by jeremiah on 7/30/22.
//

#pragma once

#include <chrono>
#include <string>

#include <bsoncxx/oid.hpp>
#include "account_objects.h"
#include "chat_rooms_objects.h"

//builds a random match made, returns the match made chat room id
std::string buildMatchMadeChatRoom(
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::oid& first_user_account_oid,
        const bsoncxx::oid& second_user_account_oid,
        UserAccountDoc& first_user_account,
        UserAccountDoc& second_user_account
);

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
);

struct SetupTestingForUnMatch {

    std::chrono::milliseconds current_timestamp{-1};

    bsoncxx::oid generated_account_oid;
    bsoncxx::oid second_generated_account_oid;

    UserAccountDoc user_account;
    UserAccountDoc second_user_account;

    ChatRoomHeaderDoc original_chat_room_header;

    std::string matching_chat_room_id;

    UserPictureDoc thumbnail_picture;
    UserPictureDoc second_thumbnail_picture;

    void initialize();

};