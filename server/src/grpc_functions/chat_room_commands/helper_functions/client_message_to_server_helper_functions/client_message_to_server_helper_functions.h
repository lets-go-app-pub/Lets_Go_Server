//
// Created by jeremiah on 8/1/22.
//

#pragma once

#include <bsoncxx/document/value.hpp>
#include "ChatRoomCommands.pb.h"

bsoncxx::document::value buildFilterForCheckingInviteHeader(
        grpc_chat_commands::ClientMessageToServerRequest *request
        );