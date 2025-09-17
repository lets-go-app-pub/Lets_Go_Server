//
// Created by jeremiah on 7/2/21.
//

#include "utility_testing_functions.h"

std::string convertAccountStateInChatRoomToString(const AccountStateInChatRoom& accountStateInChatRoom) {

    switch (accountStateInChatRoom) {
        case ACCOUNT_STATE_IS_ADMIN:
            return "ACCOUNT_STATE_IS_ADMIN";
        case ACCOUNT_STATE_EVENT:
            return "ACCOUNT_STATE_EVENT";
        case ACCOUNT_STATE_BANNED:
            return "ACCOUNT_STATE_BANNED";
        case ACCOUNT_STATE_IN_CHAT_ROOM:
            return "STATE_IN_CHAT_ROOM";
        case ACCOUNT_STATE_NOT_IN_CHAT_ROOM:
            return "ACCOUNT_STATE_NOT_IN_CHAT_ROOM";
        case ACCOUNT_STATE_IS_SUPER_ADMIN:
            return "ACCOUNT_STATE_IS_SUPER_ADMIN";
        case AccountStateInChatRoom_INT_MIN_SENTINEL_DO_NOT_USE_:
            return "AccountStateInChatRoom_INT_MIN_SENTINEL_DO_NOT_USE_";
        case AccountStateInChatRoom_INT_MAX_SENTINEL_DO_NOT_USE_:
            return "AccountStateInChatRoom_INT_MAX_SENTINEL_DO_NOT_USE_";
    }

    return "Error: invalid value";
}