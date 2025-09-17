//
// Created by jeremiah on 4/9/21.
//

#include "utility_chat_functions.h"

#include "chat_room_values.h"

bool isInvalidChatRoomId(const std::string& chatRoomID) {
    //This check is a bit complex to prevent one of these making its way to an aggregation pipeline
    // with an invalid char (like $ inside).
    return ((chatRoomID.size() != chat_room_values::CHAT_ROOM_ID_NUMBER_OF_DIGITS
    && chatRoomID.size() != chat_room_values::CHAT_ROOM_ID_NUMBER_OF_DIGITS - 1)
    || !std::all_of(chatRoomID.begin(), chatRoomID.end(), [](char c){return (std::islower(c)||std::isdigit(c));}));
}