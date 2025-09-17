//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//mongoDB (ERRORS_DATABASE_NAME) (HANDLED_ERRORS_LIST_COLLECTION_NAME)
namespace handled_errors_list_keys {
    inline const std::string ERROR_ORIGIN = "eO"; //int32; follows ErrorOriginType enum inside ErrorOriginEnum.proto
    inline const std::string VERSION_NUMBER = "vN"; //int32; version number (should be greater than 0)
    inline const std::string FILE_NAME = "fN"; //string; file name where error occurred
    inline const std::string LINE_NUMBER = "lN"; //int32; line number where error occurred (should be greater than 0)
    inline const std::string ERROR_HANDLED_MOVE_REASON = "mR"; //int32; reason this message was moved from FRESH_ERRORS_COLLECTION_NAME to HANDLED_ERRORS_COLLECTION_NAME; follows ErrorHandledMoveReason inside ErrorHandledMoveReasonEnum.proto
    inline const std::string DESCRIPTION = "dE"; //string or does not exist; short description of bug; currently used in all cases, could NOT be used in a case in the future
}