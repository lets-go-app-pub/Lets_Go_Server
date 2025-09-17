//
// Created by jeremiah on 11/18/21.
//

#pragma once

//keys for (DELETED_DATABASE_NAME) (DELETED_CHAT_MESSAGE_PICTURES_COLLECTION_NAME)
namespace deleted_chat_message_pictures_keys {
    //NOTE: this will simply be the exact user chat room picture inserted so that it can be searched by the "_id" field
    inline const std::string TIMESTAMP_REMOVED = "tst_rem"; //mongoDB Date; time the picture was moved to the deleted collection
}