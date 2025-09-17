//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>
#include <cmath>
#include <chrono>

namespace chat_room_values {
    //NOTE: chat room IDs will be either size CHAT_ROOM_ID_NUMBER_OF_DIGITS or CHAT_ROOM_ID_NUMBER_OF_DIGITS-1
    inline const std::string CHAT_ROOM_PASSWORD_CHAR_LIST = "0123456789abcdefghijklmnopqrstuvwxyz"; //chars possible to exist in a chat room password (NOTE: the user can set any password, these are just for the auto generated password)
    inline const unsigned int CHAT_ROOM_PASSWORD_NUMBER_OF_DIGITS = 3; //number of digits in a chat room password, this can be changed at any time
    inline const std::string CHAT_ROOM_ID_CHAR_LIST = "0123456789abcdefghijklmnopqrstuvwxyz"; //chars possible to exist in a chat room Id; NOTE: need to change isInvalidChatRoomId() if this is changed.
    inline const unsigned int CHAT_ROOM_ID_NUMBER_OF_DIGITS = 8; //number of digits in a chat room ID, however it cannot just be changed, the prime must be changed as well
    inline const unsigned long long CHAT_ROOM_ID_GENERATOR_CONSTANT = std::pow(CHAT_ROOM_ID_CHAR_LIST.size(), CHAT_ROOM_ID_NUMBER_OF_DIGITS); //used for generating unique chat room Ids
    inline const unsigned long long CHAT_ROOM_ID_GENERATOR_PRIME = 6000500030023; //a prime greater than (CHAT_ROOM_ID_CHAR_LIST.size())^(CHAT_ROOM_ID_NUMBER_OF_DIGITS) used for generating unique chat room Ids

    inline const unsigned int CHAT_ROOM_MAX_NUMBER_MESSAGES_TO_REQUEST_ON_INITIAL_JOIN = 20; //this is the maximum number of messages that will be requested when a user joins a chat room
    inline const unsigned int MAXIMUM_NUMBER_USERS_IN_CHAT_ROOM_TO_REQUEST_ALL_INFO = 5; //if the server gets past this number of members in a chat room (this includes the user checking), it will only send back partial info

    inline const std::chrono::seconds TIME_UNTIL_TEMPORARY_EXPIRES{2L * 60L}; //used for when the server will automatically delete a temporary timestamp

    //The below are default values expected to be sent to the client from a chat room header if the respective fields
    // are not set (event value and not an event chat room OR expiration time passed OR field does not exist).
    inline const std::string EVENT_ID_DEFAULT;
    inline const double PINNED_LOCATION_DEFAULT_LONGITUDE = 181;
    inline const double PINNED_LOCATION_DEFAULT_LATITUDE = 91;
    inline const std::string QR_CODE_DEFAULT = "~";
    inline const std::string QR_CODE_MESSAGE_DEFAULT = "~";
    inline const long QR_CODE_TIME_UPDATED_DEFAULT = -5;

    inline const std::chrono::milliseconds EVENT_USER_LAST_ACTIVITY_TIME_DEFAULT{-1};
}