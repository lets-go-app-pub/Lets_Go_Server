//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

#include "UserAccountType.grpc.pb.h"

//mongoDB (INFO_FOR_STATISTICS_DATABASE_NAME) (MATCH_ALGORITHM_RESULTS_COLLECTION_NAME)
namespace match_algorithm_results_keys {
    //used to designate 'header' for day
    //The header will have the same GENERATED_FOR_DAY key as a normal document and be used to determine if that specific day
    // has already been calculated or not (if the day was calculated a document with TYPE_KEY_HEADER_VALUE of -1 will exist).
    inline const int ACCOUNT_TYPE_KEY_HEADER_VALUE = UserAccountType::UNKNOWN_ACCOUNT_TYPE;
    inline const int TYPE_KEY_HEADER_VALUE = -1; //used with CATEGORIES_TYPE
    inline const int VALUE_KEY_HEADER_VALUE = -1; //used with CATEGORIES_VALUE

    //NOTE: these include header documents for each day to 'mark' that a day has been calculated
    inline const std::string GENERATED_FOR_DAY = "gD"; //int64; the day number (from unix timestamp point of January 1970...) this result was generated for
    inline const std::string ACCOUNT_TYPE = "aC"; //int32; An enum representing the type of account was matched (user or event types). Follows UserAccountType enum from UserAccountType.proto. Can also be set to ACCOUNT_TYPE_KEY_HEADER_VALUE.
    inline const std::string CATEGORIES_TYPE = "tY"; //int32; enum representing what type of document this is, follows AccountCategoryType
    inline const std::string CATEGORIES_VALUE = "vA"; //int32; integer value of activity or category
    inline const std::string NUM_YES_YES = "yY"; //int64; the number of matches where the first user swiped yes and the response to that was also swiped yes on
    inline const std::string NUM_YES_NO = "yN"; //int64; the number of matches where the first user swiped yes and the response to that was swiped no on
    inline const std::string NUM_YES_BLOCK_AND_REPORT = "yB"; //int64; the number of matches where the first user swiped yes and the response to that swiped block & report on
    inline const std::string NUM_YES_INCOMPLETE = "yI"; //int64; the number of matches where the first user swiped yes and the response to that was not yet swiped on
    inline const std::string NUM_NO = "nO"; //int64; the number of matches where the user swiped no
    inline const std::string NUM_BLOCK_AND_REPORT = "bR"; //int64; the number of matches where the user swiped block and report
    inline const std::string NUM_INCOMPLETE = "iC"; //int64; the number of match was incomplete
}