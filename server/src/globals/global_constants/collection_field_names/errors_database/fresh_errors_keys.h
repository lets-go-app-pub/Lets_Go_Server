//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//mongoDB (ERRORS_DATABASE_NAME) (FRESH_ERRORS_COLLECTION_NAME)
namespace fresh_errors_keys {
    inline const std::string ERROR_ORIGIN = "eO"; //int32; follows ErrorOriginType enum inside ErrorOriginEnum.proto
    inline const std::string ERROR_URGENCY = "eU"; //int32; follows ErrorUrgencyLevel enum inside ErrorOriginEnum.proto
    inline const std::string VERSION_NUMBER = "vN"; //int32; version number (should be greater than 0)
    inline const std::string FILE_NAME = "fN"; //string; file name where error occurred
    inline const std::string LINE_NUMBER = "lN"; //int32; line number where error occurred (should be greater than or equal to 0)
    inline const std::string STACK_TRACE = "sT"; //string; stack trace (if available) of error ALWAYS EXISTS
    inline const std::string TIMESTAMP_STORED = "tS"; //mongoDB Date; timestamp error was stored
    inline const std::string API_NUMBER = "aP"; //int32 or does not exist; Android API number (only used when ERROR_ORIGIN == ErrorOriginType.ERROR_ORIGIN_ANDROID)
    inline const std::string DEVICE_NAME = "dN"; //string or does not exist; Device name (only used when ERROR_ORIGIN == ErrorOriginType.ERROR_ORIGIN_ANDROID)
    inline const std::string ERROR_MESSAGE = "eM"; //string; error message
}