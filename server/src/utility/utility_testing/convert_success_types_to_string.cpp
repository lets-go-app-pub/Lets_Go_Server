//
// Created by jeremiah on 3/19/21.
//

#include "utility_testing_functions.h"

std::string convertSuccessTypesToString(findmatches::FindMatchesCapMessage::SuccessTypes successTypes) {

    switch (successTypes) {
        case findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN:
            return "FindMatchesCapMessage_SuccessTypes_UNKNOWN";
        case findmatches::FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED:
            return "FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED";
        case findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND:
            return "FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND";
        case findmatches::FindMatchesCapMessage_SuccessTypes_MATCH_ALGORITHM_ON_COOL_DOWN:
            return "FindMatchesCapMessage_SuccessTypes_MATCH_ALGORITHM_ON_COOL_DOWN";
        case findmatches::FindMatchesCapMessage_SuccessTypes_NO_SWIPES_REMAINING:
            return "FindMatchesCapMessage_SuccessTypes_NO_SWIPES_REMAINING";
        case findmatches::FindMatchesCapMessage_SuccessTypes_FindMatchesCapMessage_SuccessTypes_INT_MIN_SENTINEL_DO_NOT_USE_:
            return "FindMatchesCapMessage_SuccessTypes_FindMatchesCapMessage_SuccessTypes_INT_MIN_SENTINEL_DO_NOT_USE_";
        case findmatches::FindMatchesCapMessage_SuccessTypes_FindMatchesCapMessage_SuccessTypes_INT_MAX_SENTINEL_DO_NOT_USE_:
            return "FindMatchesCapMessage_SuccessTypes_FindMatchesCapMessage_SuccessTypes_INT_MAX_SENTINEL_DO_NOT_USE_";
    }

    return "Error: invalid FindMatchesCapMessage::SuccessTypes";
}