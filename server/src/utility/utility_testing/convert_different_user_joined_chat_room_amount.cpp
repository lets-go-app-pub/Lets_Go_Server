//
// Created by jeremiah on 11/14/21.
//


#include "utility_testing_functions.h"

std::string convertDifferentUserJoinedChatRoomAmountToString(const DifferentUserJoinedChatRoomAmount& differentUserJoinedChatRoomAmount) {

    std::string returnString = "Error: invalid value";

    switch (differentUserJoinedChatRoomAmount)
    {
        case DifferentUserJoinedChatRoomAmount::SKELETON:
            returnString = "SKELETON";
            break;
        case DifferentUserJoinedChatRoomAmount::INFO_WITHOUT_IMAGES:
            returnString = "INFO_WITHOUT_IMAGES";
            break;
        case DifferentUserJoinedChatRoomAmount::INFO_WITH_THUMBNAIL_NO_PICTURES:
            returnString = "INFO_WITH_THUMBNAIL_NO_PICTURES";
            break;
        case DifferentUserJoinedChatRoomAmount::ALL_INFO_AND_IMAGES:
            returnString = "ALL_INFO_AND_IMAGES";
            break;
        default:
            break;
    }

    return returnString;
}