//
// Created by jeremiah on 3/19/21.
//
#include "utility_testing_functions.h"

std::string convertReturnStatusToString(ReturnStatus returnStatus) {

    switch (returnStatus) {
        case VALUE_NOT_SET:
            return "VALUE_NOT_SET";
        case SUCCESS:
            return "SUCCESS";
        case LOGGED_IN_ELSEWHERE:
            return "LOGGED_IN_ELSEWHERE";
        case NOT_ENOUGH_INFO:
            return "NOT_ENOUGH_INFO";
        case LOGIN_TOKEN_EXPIRED:
            return "LOGIN_TOKEN_EXPIRED";
        case NO_VERIFIED_ACCOUNT:
            return "NO_VERIFIED_ACCOUNT";
        case LOGIN_TOKEN_DID_NOT_MATCH:
            return "LOGIN_TOKEN_DID_NOT_MATCH";
        case ACCOUNT_SUSPENDED:
            return "ACCOUNT_SUSPENDED";
        case ACCOUNT_BANNED:
            return "ACCOUNT_BANNED";
        case FUNCTION_CALLED_TOO_QUICKLY:
            return "FUNCTION_CALLED_TOO_QUICKLY";
        case SUBSCRIPTION_REQUIRED:
            return "SUBSCRIPTION_REQUIRED";
        case INVALID_USER_OID:
            return "INVALID_USER_OID";
        case INVALID_LOGIN_TOKEN:
            return "INVALID_LOGIN_TOKEN";
        case OUTDATED_VERSION:
            return "OUTDATED_VERSION";
        case INVALID_INSTALLATION_ID:
            return "INVALID_INSTALLATION_ID";
        case INVALID_PARAMETER_PASSED:
            return "INVALID_PARAMETER_PASSED";
        case CORRUPTED_FILE:
            return "CORRUPTED_FILE";
        case DATABASE_DOWN:
            return "DATABASE_DOWN";
        case LG_ERROR:
            return "LG_ERROR";
        case UNKNOWN:
            return "UNKNOWN";
        case ReturnStatus_INT_MIN_SENTINEL_DO_NOT_USE_:
            return "ReturnStatus_INT_MIN_SENTINEL_DO_NOT_USE_";
        case ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_:
            return "ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_";
    }

    return "Error: invalid ReturnStatus";
}
