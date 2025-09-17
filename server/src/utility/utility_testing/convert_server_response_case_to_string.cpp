//
// Created by jeremiah on 6/29/21.
//
#include "utility_testing_functions.h"

std::string convertServerResponseCaseToString(grpc_stream_chat::ChatToClientResponse::ServerResponseCase serverResponseCase) {

    std::string returnString = "Error: invalid value";

    switch (serverResponseCase)
    {
        case grpc_stream_chat::ChatToClientResponse::kInitialConnectionPrimerResponse:
            returnString = "kInitialConnectionPrimerResponse";
            break;
        case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse:
            returnString = "kInitialConnectionMessagesResponse";
            break;
        case grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse:
            returnString = "kInitialConnectionMessagesCompleteResponse";
            break;
        case grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse:
            returnString = "kRequestFullMessageInfoResponse";
            break;
        case grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse:
            returnString = "kRefreshChatStreamResponse";
            break;
        case grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage:
            returnString = "kReturnNewChatMessage";
            break;
        case grpc_stream_chat::ChatToClientResponse::SERVER_RESPONSE_NOT_SET:
            returnString = "SERVER_RESPONSE_NOT_SET";
            break;
    }

    return returnString;

}