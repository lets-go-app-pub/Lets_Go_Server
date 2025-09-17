//
// Created by jeremiah on 10/25/21.
//

#pragma once

#include <chrono>
#include <string>

#include "MemberSharedInfoMessage.grpc.pb.h"

//Used as the default 'deleted' values sent back to the client when a thumbnail is not found inside the chat room.
//Also, when an admin deletes a user picture, if there are no pictures to replace the thumbnail, these values will
// be used instead.
struct DeletedThumbnailInfo {

    static void saveDeletedThumbnailInfo(MemberSharedInfoMessage* mutable_user_info) {
        mutable_user_info->set_account_thumbnail_size((int)thumbnail.size());
        mutable_user_info->set_account_thumbnail_timestamp(DeletedThumbnailInfo::deleted_thumbnail_timestamp.count());
        mutable_user_info->set_account_thumbnail(thumbnail);
        mutable_user_info->set_account_thumbnail_index(DeletedThumbnailInfo::index);
    }

    //NOTE: the "~" char as the thumbnail byte string means
    // that it was deleted
    inline static std::string thumbnail = "~"; //This is moved, it cannot be const or static.
    const static inline std::string thumbnail_reference_oid{};
    const static inline int index = -1;
    static const std::chrono::milliseconds inline deleted_thumbnail_timestamp{-1L};
};
