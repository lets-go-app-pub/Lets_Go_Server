//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (CHAT_ROOMS_DATABASE_NAME) (CHAT_MESSAGE_PICTURES_COLLECTION_NAME)
namespace chat_message_pictures_keys {
    inline const std::string CHAT_ROOM_ID = "cR"; //utf8; ID of chat room this message was sent from
    inline const std::string PICTURE_IN_BYTES = "pI"; //utf8; picture itself in bytes
    inline const std::string PICTURE_SIZE_IN_BYTES = "pS"; //int32; total size in bytes of this picture
    inline const std::string HEIGHT = "pH"; //int32; height of the picture in pixels
    inline const std::string WIDTH = "pW"; //int32; width of the picture in pixels
    inline const std::string TIMESTAMP_STORED = "tS"; //mongocxx date type; timestamp picture was stored
}