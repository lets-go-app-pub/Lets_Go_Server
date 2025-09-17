//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include <string>
#include <vector>
#include <chrono>

//used as the return value for the function sendMessageToChatRoom()
struct SendMessageToChatRoomReturn {

    enum class SuccessfulReturn {
        SEND_MESSAGE_SUCCESSFUL,
        SEND_MESSAGE_FAILED,
        MESSAGE_ALREADY_EXISTED
    };

    SuccessfulReturn successful = SuccessfulReturn::SEND_MESSAGE_FAILED;
    std::string picture_oid; //only used if passed message type was PICTURE_MESSAGE
    std::chrono::milliseconds time_stored_on_server{
            -1}; //can be different from the passed time (although will almost never happen)
    int placeholder_stored_random_int = -1; //only used with createAndStoreMessagePlaceHolder()

    SendMessageToChatRoomReturn() = default;

    SendMessageToChatRoomReturn(
            SuccessfulReturn _successful,
            std::string _pictureOid,
            std::chrono::milliseconds _timeStoredOnServer
            ) :
            successful(_successful),
            picture_oid(std::move(_pictureOid)),
            time_stored_on_server(_timeStoredOnServer) {}

    SendMessageToChatRoomReturn(
            SuccessfulReturn _successful,
            std::string _pictureOid,
            std::chrono::milliseconds _timeStoredOnServer,
            int _placeholder_stored_random_int) :
            successful(_successful),
            picture_oid(std::move(_pictureOid)),
            time_stored_on_server(_timeStoredOnServer),
            placeholder_stored_random_int(_placeholder_stored_random_int) {}

};
