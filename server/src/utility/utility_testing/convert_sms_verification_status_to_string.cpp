//
// Created by jeremiah on 3/20/21.
//

#include "utility_testing_functions.h"

std::string convertSmsVerificationStatusToString(int a) {

    std::string returnString = "Error: invalid value";

    if (a == 0) {
        returnString = "VALUE_NOT_SET";
    }
    else if (a == 1) {
        returnString = "SUCCESS";
    }
    else if (a == 2) {
        returnString = "OUTDATED_VERSION";
    }
    else if (a == 3) {
        returnString = "VERIFICATION_CODE_EXPIRED";
    }
    else if (a == 4) {
        returnString = "INVALID_PHONE_NUMBER_OR_ACCOUNT_ID";
    }
    else if (a == 5) {
        returnString = "INVALID_VERIFICATION_CODE";
    }
    else if (a == 6) {
        returnString = "INVALID_INSTALLATION_ID";
    }
    else if (a == 7) {
        returnString = "PENDING_ACCOUNT_NOT_FOUND";
    }
    else if (a == 8) {
        returnString = "INCORRECT_BIRTHDAY";
    }
    else if (a == 9) {
        returnString = "INVALID_UPDATE_ACCOUNT_PASSED";
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