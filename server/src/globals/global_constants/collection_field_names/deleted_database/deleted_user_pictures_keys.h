//
// Created by jeremiah on 11/18/21.
//

#pragma once

//keys for (DELETED_DATABASE_NAME) (DELETED_USER_PICTURES_COLLECTION_NAME)
namespace deleted_user_pictures_keys {
    //NOTE: this will simply be the exact user account picture inserted so that it can be searched by the "_id" field

    inline const std::string REFERENCES_REMOVED_AFTER_DELETE = "rem_aft"; //bool or missing; MAY exist inside a deleted picture (field may not exist as well), will be set to true if the references were deleted AFTER the picture was set to deleted.
    inline const std::string TIMESTAMP_REMOVED = "tst_rem"; //mongoDB Date; time the picture was moved to the deleted collection
    inline const std::string REASON_REMOVED = "rsn_rem"; //int32; reason picture deleted; follows ReasonPictureDeleted
    inline const std::string ADMIN_NAME = "adm_nam"; //utf8 or missing; admin name when ReasonPictureDeleted == REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE; will not exist otherwise
}