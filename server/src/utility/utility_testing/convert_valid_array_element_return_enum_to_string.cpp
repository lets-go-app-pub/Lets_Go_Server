//
// Created by jeremiah on 3/20/21.
//

#include "utility_testing_functions.h"

std::string convertValidateArrayElementReturnEnumToString(int a) {

    std::string returnString = "Error: invalid value";

    if (a == 0) {
        returnString = "empty_list";
    }
    else if (a == 1) {
        returnString = "error";
    }
    else if (a == 2) {
        returnString = "success";
    }
    else if (a == 3) {
        returnString = "no_longer_valid";
    }

    return returnString;

}