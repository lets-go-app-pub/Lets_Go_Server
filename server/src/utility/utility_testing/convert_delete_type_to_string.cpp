//
// Created by jeremiah on 3/20/21.
//

#include "utility_testing_functions.h"

std::string convertDeleteTypeToString(DeleteType deleteType) {

    std::string returnString = "Error: invalid value";

    switch (deleteType)
    {
        case DELETE_TYPE_NOT_SET:
            returnString = "DELETE_TYPE_NOT_SET";
            break;
        case DELETED_ON_CLIENT:
            returnString = "DELETED_ON_CLIENT";
            break;
        case DELETE_FOR_SINGLE_USER:
            returnString = "DELETE_FOR_SINGLE_USER";
            break;
        case DELETE_FOR_ALL_USERS:
            returnString = "DELETE_FOR_ALL_USERS";
            break;
        case DeleteType_INT_MIN_SENTINEL_DO_NOT_USE_:
            returnString = "DeleteType_INT_MIN_SENTINEL_DO_NOT_USE_";
            break;
        case DeleteType_INT_MAX_SENTINEL_DO_NOT_USE_:
            returnString = "DeleteType_INT_MAX_SENTINEL_DO_NOT_USE_";
            break;
        default:
            break;
    }

    return returnString;

}
