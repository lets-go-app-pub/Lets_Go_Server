//
// Created by jeremiah on 9/21/21.
//

#pragma once

#include <AdminChatRoomCommands.grpc.pb.h>

void getSingleMessage(const ::admin_chat_commands::GetSingleMessageRequest* request,
                      ::admin_chat_commands::GetSingleMessageResponse* response);

class AdminChatRoomCommandsServiceImpl final : public admin_chat_commands::AdminChatRoomCommandsService::Service {
public:
    grpc::Status
    GetSingleMessageRPC(::grpc::ServerContext* context [[maybe_unused]], const ::admin_chat_commands::GetSingleMessageRequest* request,
                        ::admin_chat_commands::GetSingleMessageResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting GetSingleMessageRPC Function...\n";
#endif // DEBUG

        getSingleMessage(request, response);

#ifdef _DEBUG
        std::cout << "Finishing GetSingleMessageRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};