//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (CHAT_ROOMS_DATABASE_NAME) (CHAT_ROOM_INFO)
namespace chat_room_info_keys {
    //this is the collection that holds the number to convert into a chat room ID
    inline const std::string ID = "iD"; //the id of the chat room (it has no type, it is the string that _id refers to)
    inline const std::string PREVIOUSLY_USED_CHAT_ROOM_NUMBER = "pN"; //int64; the number used to convert into a chat room ID
}