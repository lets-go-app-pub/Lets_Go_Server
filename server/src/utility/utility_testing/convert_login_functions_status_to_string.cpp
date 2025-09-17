//
// Created by jeremiah on 3/20/21.
//

#include "utility_testing_functions.h"

std::string convertLoginFunctionStatusToString(int a) {

    std::string returnString = "Error: invalid value";

    if (a == 0) {
        returnString = "VALUE_NOT_SET";
    }
    else if (a == 1) {
        returnString = "LOGGED_IN";
    }
    else if (a == 2) {
        returnString = "ACCOUNT_CLOSED";
    }
    else if (a == 3) {
        returnString = "REQUIRES_AUTHENTICATION";
    }
    else if (a == 4) {
        returnString = "OUTDATED_VERSION";
    }
    else if (a == 5) {
        returnString = "SMS_ON_COOL_DOWN";
    }
    else if (a == 6) {
        returnString = "REQUIRES_PHONE_NUMBER_TO_CREATE_ACCOUNT";
    }
    else if (a == 7) {
        returnString = "INVALID_PHONE_NUMBER_OR_ACCOUNT_ID";
    }
    else if (a == 8) {
        returnString = "INVALID_INSTALLATION_ID";
    }
    else if (a == 9) {
        returnString = "INVALID_ACCOUNT_TYPE";
    }
    else if (a == 10) {
        returnString = "UNKNOWN";
    }
    else if (a == 11) {
        returnString = "DATABASE_DOWN";
    }
    else if (a == 12) {
        returnString = "LG_ERROR";
    }
    else if (a == 13) {
        returnString = "ANDROID_SIDE_ERROR";
    }

    return returnString;

}