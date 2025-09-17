//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (INFO_FOR_STATISTICS_DATABASE_NAME) (USER_ACCOUNT_STATISTICS_DOCUMENTS_COMPLETED_COLLECTION_NAME)
namespace user_account_statistics_documents_completed_keys {
    inline const std::string PREVIOUS_ID = "p_id"; //oid; the oid this document was moved from (the user it belongs to)
    inline const std::string TIMESTAMP_MOVED = "tSm"; //mongodb Date; the time the document was moved to this collection
}