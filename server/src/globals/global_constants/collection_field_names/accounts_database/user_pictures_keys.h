//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (USER_PICTURES_COLLECTION_NAME)
namespace user_pictures_keys {
    inline const std::string USER_ACCOUNT_REFERENCE = "aR"; //ObjectId (type oid in C++)
    inline const std::string THUMBNAIL_REFERENCES = "tR"; //array of strings; list of chat room ids that hold a reference to this pictures thumbnail
    inline const std::string TIMESTAMP_STORED = "pT"; //mongoDB Date; timestamp this picture was saved (in seconds) as shown in user account
    inline const std::string PICTURE_INDEX = "pN"; //int32; index this picture holds in the user account
    inline const std::string THUMBNAIL_IN_BYTES = "tN"; //utf8; picture thumbnail in bytes
    inline const std::string THUMBNAIL_SIZE_IN_BYTES = "tS"; //int32; total size in bytes of this thumbnail
    inline const std::string PICTURE_IN_BYTES = "pI"; //utf8; picture itself in bytes
    inline const std::string PICTURE_SIZE_IN_BYTES = "pS"; //int32; total size in bytes of this picture
}
