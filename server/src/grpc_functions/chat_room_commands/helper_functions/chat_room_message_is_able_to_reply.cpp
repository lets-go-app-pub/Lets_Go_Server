//
// Created by jeremiah on 3/20/21.
//

#include <utility_general_functions.h>
#include "chat_room_commands_helper_functions.h"

//this function will return true if the message type can reply to other messages (the types listed inside checkIfChatRoomMessageCanBeRepliedTo) and false otherwise
bool chatRoomMessageIsAbleToReply(MessageSpecifics::MessageBodyCase messageType) {
    return isActiveMessageType(messageType);
}