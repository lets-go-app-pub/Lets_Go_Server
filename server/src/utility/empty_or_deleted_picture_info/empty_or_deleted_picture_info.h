//
// Created by jeremiah on 9/20/22.
//

#pragma once

#include <chrono>
#include <string>

#include "RequestUserAccountInfo.pb.h"

struct EmptyOrDeletedPictureInfo {

    //Used as the default 'deleted' values sent back to the desktop interface when a picture has been deleted (not replaced) for a
    // user. This happens when an admin deletes a user picture.
    static void savePictureInfoDesktopInterface(
            PictureMessageResult* picture_message,
            const std::chrono::milliseconds& timestamp,
            int index
    ) {
        picture_message->set_timestamp(timestamp.count());
        picture_message->mutable_picture()->set_file_size(0);
        picture_message->mutable_picture()->set_file_in_bytes("");
        picture_message->mutable_picture()->set_index_number(index);
        picture_message->mutable_picture()->set_picture_oid("");
    }

    static void savePictureInfoClient(
            request_fields::PictureResponse* picture_message,
            const std::chrono::milliseconds& timestamp,
            int index
    ) {
        picture_message->set_timestamp(timestamp.count());
        picture_message->mutable_picture_info()->set_file_size(0);
        picture_message->mutable_picture_info()->set_file_in_bytes("");
        picture_message->mutable_picture_info()->set_index_number(index);
        picture_message->set_return_status(SUCCESS);
    }
};