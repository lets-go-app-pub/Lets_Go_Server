//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//mongoDB (ERRORS_DATABASE_NAME) (HANDLED_ERRORS_COLLECTION_NAME)
namespace handled_errors_keys {
    inline const std::string ADMIN_NAME = "aN"; //string; admin name that set this 'chunk' of reports to handled
    inline const std::string TIMESTAMP_HANDLED = "tH"; //mongoDB Date; timestamp admin handled this 'chunk' of reports
    inline const std::string ERROR_HANDLED_MOVE_REASON = "mR"; //int32; reason this message was moved from FRESH_ERRORS_COLLECTION_NAME to HANDLED_ERRORS_COLLECTION_NAME; follows ErrorHandledMoveReason inside ErrorHandledMoveReasonEnum.proto
    inline const std::string ERRORS_DESCRIPTION = "dE"; //string or does not exist; short description of bug; currently used in all cases, could NOT be used in a case in the future

    //NOTE: Documents are moved from FRESH_ERRORS_COLLECTION_NAME to HANDLED_ERRORS_COLLECTION_NAME with a few
    // fields added. All fields inside of FRESH_ERRORS_COLLECTION_NAME have potential to be inside this document.
}