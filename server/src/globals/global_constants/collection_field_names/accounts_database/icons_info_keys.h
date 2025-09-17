//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (ICONS_INFO_COLLECTION_NAME)
namespace icons_info_keys {
    //NOTE: the _id for these will be the index (as an int64)
    inline const std::string INDEX = "_id"; //int64, index of the icon
    inline const std::string TIMESTAMP_LAST_UPDATED = "ts"; //mongoDB Date, time this icon last received an update, will be set to -2 if file corrupt or inactive
    inline const std::string ICON_IN_BYTES = "img"; //string; byte array for the basic image
    inline const std::string ICON_SIZE_IN_BYTES = "size"; //int64; basic image size in bytes
    inline const std::string ICON_ACTIVE = "active"; //bool; true if icon is active, false if icon is disabled (note this is opposite of how android handles it where it uses isDeleted as the bool)
}