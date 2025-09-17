//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>
#include "general_values.h"
#include "server_parameter_restrictions.h"

namespace grpc_values {

    //Adding an event requires taking the maximum number of pictures, so it must be large enough
    // to hold all picture, plus some extra.
    inline const int MAX_RECEIVE_MESSAGE_LENGTH =
            (server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES +
             server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES)
            * general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT
            + 2 * 1024 * 1024;

    //NOTE: MemberSharedInfoMessage & CompleteUserAccountInfo send an array of pictures, so it must be large enough
    // to hold an entire user, plus some extra
    inline const int MAX_SEND_MESSAGE_LENGTH = (server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES +
                                                server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES)
                                               * general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT
                                               + 2 * 1024 * 1024;

    //used as the time to close idle connections on both server and client.
    inline const long CONNECTION_IDLE_TIMEOUT_IN_MS = 15L * 60L * 1000L;

    //(in bytes) the maximum message size for sending a chat message to the client
    inline const int MAXIMUM_SIZE_FOR_SENDING_CHAT_MESSAGE_TO_CLIENT = MAX_SEND_MESSAGE_LENGTH * .95;

    //the maximum size meta can be when the server is receiving it from the client
    inline const int MAXIMUM_RECEIVING_META_DATA_SIZE = 1 * 1024 * 1024;

    //the maximum size the client should make the metadata
    inline const int MAXIMUM_META_DATA_SIZE_TO_SEND_FROM_CLIENT = MAXIMUM_RECEIVING_META_DATA_SIZE - 1024;
}