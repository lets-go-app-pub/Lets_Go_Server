//
// Created by jeremiah on 5/16/21.
//

#include "utility_testing_functions.h"

std::string convertPushedToQueueFromLocationToString(PushedToQueueFromLocation pushedToQueueFromLocation) {

    switch (pushedToQueueFromLocation) {
        case PUSHED_FROM_INJECTION:
            return "PUSHED_FROM_INJECTION";
        case PUSHED_FROM_READ:
            return "PUSHED_FROM_READ";
    }

    return "Unknown Case";
}