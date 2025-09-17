//
// Created by jeremiah on 3/20/21.
//

#include "utility_testing_functions.h"

std::string convertAccessStatusToString(int a) {

    if (a == 0)
        return "CONNECTION_ERR";
    else if (a == 1)
        return "NEEDS_MORE_INFO";
    else if (a == 2)
        return "ACCESS_GRANTED";
    else if (a == 3)
        return "STATUS_SUSPENDED";
    else if (a == 4)
        return "STATUS_BANNED";
    else if (a == 5)
        return "LG_ERR";
    else
        return "'Not in enum bounds'";
}