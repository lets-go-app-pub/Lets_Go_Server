//
// Created by jeremiah on 6/1/22.
//

#pragma once

#include "reason_picture_deleted.h"
#include <mongocxx/collection.hpp>
#include "account_objects.h"

void checkDeletePictureDocumentResult(
        const std::chrono::milliseconds& currentTimestamp,
        mongocxx::collection& user_pictures_collection,
        bool return_val,
        const UserPictureDoc& user_picture,
        ReasonPictureDeleted reason_picture_deleted,
        const std::unique_ptr<std::string>& admin_name,
        int* thumbnail_size
);

void checkDeleteAccountPicturesResults(
        bool return_val,
        const std::vector<UserPictureDoc>& pictures_before_delete,
        const std::chrono::milliseconds& currentTimestamp,
        mongocxx::collection& user_pictures_collection,
        int& deleted_account_thumbnail_size,
        const std::string& thumbnail_reference_oid,
        const std::string& deleted_account_thumbnail_reference_oid
);

//run_delete is expected to run serverInternalDeleteAccount()
void buildAndCheckDeleteAccount(
        const bsoncxx::oid& generated_account_oid,
        const std::function<bool(std::chrono::milliseconds& /*current_timestamp*/)>& run_delete
);
