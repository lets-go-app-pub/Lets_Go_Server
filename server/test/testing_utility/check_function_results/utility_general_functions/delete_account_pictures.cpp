//
// Created by jeremiah on 6/1/22.
//

#include <gtest/gtest.h>
#include "utility_general_functions_test.h"

void checkDeleteAccountPicturesResults(
        bool return_val,
        const std::vector<UserPictureDoc>& pictures_before_delete,
        const std::chrono::milliseconds& currentTimestamp,
        mongocxx::collection& user_pictures_collection,
        int& deleted_account_thumbnail_size,
        const std::string& thumbnail_reference_oid,
        const std::string& deleted_account_thumbnail_reference_oid
        ) {

    EXPECT_EQ(return_val, true);

    for (size_t i = 0; i < pictures_before_delete.size(); i++) {
        int* thumbnail_ptr = nullptr;
        if(i == 0 && deleted_account_thumbnail_size != 0) {
            thumbnail_ptr = &deleted_account_thumbnail_size;
        }
        checkDeletePictureDocumentResult(
                currentTimestamp,
                user_pictures_collection,
                return_val,
                pictures_before_delete[i],
                ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
                nullptr,
                thumbnail_ptr
        );
    }

    if(deleted_account_thumbnail_size != 0) {
        EXPECT_EQ(thumbnail_reference_oid, deleted_account_thumbnail_reference_oid);
    }
}