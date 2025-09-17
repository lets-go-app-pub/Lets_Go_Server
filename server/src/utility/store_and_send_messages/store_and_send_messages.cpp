//
// Created by jeremiah on 4/10/21.
//

#include <grpc_values.h>
#include <optional>
#include <store_mongoDB_error_and_exception.h>
#include <utility_testing_functions.h>
#include "store_and_send_messages.h"

#include "server_parameter_restrictions.h"

//NOTE: this parameter is passed by value on purpose.
void StoreAndSendMessagesVirtual::sendMessage(ChatMessageToClient chatMessageToClient)  {

    size_t chatMessageSize = chatMessageToClient.ByteSizeLong();
    currentByteSizeOfResponseMsg += chatMessageSize;

    if(chatMessageSize > grpc_values::MAXIMUM_SIZE_FOR_SENDING_CHAT_MESSAGE_TO_CLIENT) {
        std::string error_string = "Too large of a message was attempted to be sent";

        std::optional<std::string> dummy_exception_string;
        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            dummy_exception_string, error_string,
            "chatMessageSize", std::to_string(chatMessageSize),
            "MAXIMUM_SIZE_FOR_SENDING_CHAT_MESSAGE_TO_CLIENT", std::to_string(grpc_values::MAXIMUM_SIZE_FOR_SENDING_CHAT_MESSAGE_TO_CLIENT),
            "chat_room_id", chatMessageToClient.message().standard_message_info().chat_room_id_message_sent_from(),
            "message_uuid", chatMessageToClient.message_uuid(),
            "message_body_case", convertMessageBodyTypeToString(chatMessageToClient.message().message_specifics().message_body_case())
        );

        currentByteSizeOfResponseMsg -= chatMessageSize;
        return;
    } else if(currentByteSizeOfResponseMsg > grpc_values::MAXIMUM_SIZE_FOR_SENDING_CHAT_MESSAGE_TO_CLIENT) {
        writeMessageAndClearList();
        //The current message still needs to be taken into account.
        currentByteSizeOfResponseMsg = chatMessageSize;
    }

    mutableMessageList->Add(std::move(chatMessageToClient));

    number_messages_sent++;
}

void StoreAndSendMessagesVirtual::finalCleanup() {
    //send any leftover messages
    if(currentByteSizeOfResponseMsg > 0) {
        writeMessageAndClearList();
    }
}

void StoreAndSendMessagesVirtual::writeMessageAndClearList() {
    writeMessageToClient();
    currentByteSizeOfResponseMsg = 0;
    clearMessageList();
}

