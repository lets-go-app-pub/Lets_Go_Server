//
// Created by jeremiah on 3/19/21.
//

#include "utility_testing_functions.h"

//std::string convertMessageInstructionToString(MessageInstruction messageInstruction) {
//
//    std::string returnString = "Error: invalid value";
//
//    switch (messageInstruction)
//    {
//        case CHAT_TEXT_MESSAGE: //0
//            returnString = "CHAT_TEXT_MESSAGE";
//            break;
//        case PICTURE_MESSAGE: //1
//            returnString = "PICTURE_MESSAGE";
//            break;
//        case LOCATION_MESSAGE: //2
//            returnString = "LOCATION_MESSAGE";
//            break;
//        case MIME_TYPE_MESSAGE: //3
//            returnString = "MIME_TYPE_MESSAGE";
//            break;
//        case INVITED_TO_CHAT_ROOM: //4
//            returnString = "INVITED_TO_CHAT_ROOM";
//            break;
//        case MESSAGE_EDITED: //5
//            returnString = "MESSAGE_EDITED";
//            break;
//        case MESSAGE_DELETED: //6
//            returnString = "MESSAGE_DELETED";
//            break;
//        case USER_KICKED_FROM_CHAT_ROOM: //7
//            returnString = "USER_KICKED_FROM_CHAT_ROOM";
//            break;
//        case USER_BANNED_FROM_CHAT_ROOM: //8
//            returnString = "USER_BANNED_FROM_CHAT_ROOM";
//            break;
//        case DIFFERENT_USER_JOINED_CHAT_ROOM: //9
//            returnString = "DIFFERENT_USER_JOINED_CHAT_ROOM";
//            break;
//        case DIFFERENT_USER_LEFT_CHAT_ROOM: //10
//            returnString = "DIFFERENT_USER_LEFT_CHAT_ROOM";
//            break;
//        case UPDATE_CHAT_ROOM_OBSERVED_TIME: //11
//            returnString = "UPDATE_CHAT_ROOM_OBSERVED_TIME";
//            break;
//        case THIS_USER_JOINED_CHAT_ROOM_START: //12
//            returnString = "THIS_USER_JOINED_CHAT_ROOM";
//            break;
//        case THIS_USER_JOINED_CHAT_ROOM_MEMBER: //13
//            returnString = "THIS_USER_JOINED_CHAT_ROOM_MEMBER";
//            break;
//        case THIS_USER_JOINED_CHAT_ROOM_FINISHED: //14
//            returnString = "THIS_USER_JOINED_CHAT_ROOM_FINISHED";
//            break;
//        case THIS_USER_LEFT_CHAT_ROOM: //15
//            returnString = "THIS_USER_LEFT_CHAT_ROOM";
//            break;
//        case USER_ACTIVITY_DETECTED: //16
//            returnString = "USER_ACTIVITY_DETECTED";
//            break;
//        case CHAT_ROOM_NAME_UPDATED: //17
//            returnString = "CHAT_ROOM_NAME_UPDATED";
//            break;
//        case CHAT_ROOM_PASSWORD_UPDATED: //18
//            returnString = "CHAT_ROOM_PASSWORD_UPDATED";
//            break;
//        case NEW_ADMIN_PROMOTED: //19
//            returnString = "NEW_ADMIN_PROMOTED";
//            break;
//        case MATCH_CANCELED: //20
//            returnString = "MATCH_CANCELED";
//            break;
//        case SERVER_DOWN: //21
//            returnString = "SERVER_DOWN";
//            break;
//        case HISTORY_CLEARED: //22
//            returnString = "HISTORY_CLEARED";
//            break;
//        case LOADING_MESSAGE: //23
//            returnString = "LOADING_MESSAGE";
//            break;
//        case MessageInstruction_INT_MIN_SENTINEL_DO_NOT_USE_: //err
//            returnString = "MessageInstruction_INT_MIN_SENTINEL_DO_NOT_USE_";
//            break;
//        case MessageInstruction_INT_MAX_SENTINEL_DO_NOT_USE_: //err
//            returnString = "MessageInstruction_INT_MAX_SENTINEL_DO_NOT_USE_";
//            break;
//        default:
//            break;
//    }
//
//    return returnString;
//
//}