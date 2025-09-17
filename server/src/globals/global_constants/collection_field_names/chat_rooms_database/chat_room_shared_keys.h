//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (CHAT_ROOMS_DATABASE_NAME) (CHAT_ROOM_ID_)
//fields shared in all documents (this means it is used in header AND message type documents) (TIMESTAMP_CREATED is indexed for the collection)
namespace chat_room_shared_keys {
    inline const std::string TIMESTAMP_CREATED = "cTi"; //mongoDB Date; the timestamps extracted and stored for this are expected to be in milliseconds; NOTE: indexed by this field
}