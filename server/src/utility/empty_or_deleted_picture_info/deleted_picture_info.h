//
// Created by jeremiah on 8/27/22.
//

#pragma once

#include <chrono>
#include <string>

#include "MemberSharedInfoMessage.grpc.pb.h"

//Used as the default 'deleted' values sent back to the client when a picture has been deleted (not replaced) for a
// user. This happens when an admin deletes a user picture.
//This should only be called when a client attempts to update a picture and the picture they send in does not exist on
// the server.
struct DeletedPictureInfo {

    static void saveDeletedPictureInfo(PictureMessage* pictureMessage, int index) {
        pictureMessage->set_timestamp_picture_last_updated(-1);
        pictureMessage->set_file_in_bytes("~");
        pictureMessage->set_file_size(1);
        pictureMessage->set_index_number(index);
    }

    //NOTE: the "~" char as the thumbnail byte string means
    // that it was deleted
    inline static std::string thumbnail = "~"; //This is moved, it cannot be const or static.
    const static inline std::string thumbnail_reference_oid{};
    static const std::chrono::milliseconds inline deleted_thumbnail_timestamp{-1L};
};