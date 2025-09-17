//
// Created by jeremiah on 6/26/21.
//

#include "utility_testing_functions.h"

std::string convertAccountLoginTypeToString(const AccountLoginType accountLoginType) {

    switch (accountLoginType) {
        case LOGIN_TYPE_VALUE_NOT_SET:
            return "LOGIN_TYPE_VALUE_NOT_SET";
        case GOOGLE_ACCOUNT:
            return "GOOGLE_ACCOUNT";
        case FACEBOOK_ACCOUNT:
            return "FACEBOOK_ACCOUNT";
        case PHONE_ACCOUNT:
            return "PHONE_ACCOUNT";
        case AccountLoginType_INT_MIN_SENTINEL_DO_NOT_USE_:
            return "AccountLoginType_INT_MIN_SENTINEL_DO_NOT_USE_";
        case AccountLoginType_INT_MAX_SENTINEL_DO_NOT_USE_:
            return "AccountLoginType_INT_MAX_SENTINEL_DO_NOT_USE_";
    }

    return "Error: invalid value";
}