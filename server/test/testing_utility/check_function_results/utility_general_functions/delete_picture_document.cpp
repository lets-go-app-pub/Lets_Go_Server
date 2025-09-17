//
// Created by jeremiah on 6/1/22.
//

#include <gtest/gtest.h>
#include "collection_objects/deleted_objects/deleted_objects.h"
#include "utility_general_functions_test.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void checkDeletePictureDocumentResult(
        const std::chrono::milliseconds& currentTimestamp,
        mongocxx::collection& user_pictures_collection,
        bool return_val,
        const UserPictureDoc& user_picture,
        ReasonPictureDeleted reason_picture_deleted,
        const std::unique_ptr<std::string>& admin_name,
        int* thumbnail_size
        ) {

    EXPECT_EQ(return_val, true);

    //make sure picture was properly deleted
    auto picture_doc = user_pictures_collection.find_one(document{} << "_id" << user_picture.current_object_oid << finalize);

    EXPECT_FALSE(picture_doc.operator bool());

    DeletedUserPictureDoc generated_deleted_user_pictures_doc(
            user_picture,
            nullptr,
            bsoncxx::types::b_date{currentTimestamp},
            reason_picture_deleted,
            admin_name
            );

    DeletedUserPictureDoc extracted_deleted_user_pictures_doc(user_picture.current_object_oid);
    generated_deleted_user_pictures_doc.timestamp_removed = extracted_deleted_user_pictures_doc.timestamp_removed;

    EXPECT_EQ(generated_deleted_user_pictures_doc, extracted_deleted_user_pictures_doc);

    if(thumbnail_size != nullptr) {
        EXPECT_EQ(*thumbnail_size, user_picture.thumbnail_size_in_bytes);
    }
}