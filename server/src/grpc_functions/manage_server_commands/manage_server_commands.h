//
// Created by jeremiah on 10/26/21.
//

#pragma once

#include "ManageServerCommands.grpc.pb.h"

//NOTE: it shouldn't ever be this long but the purpose is mostly to avoid working with
// large strings
inline const size_t MAXIMUM_NUMBER_ALLOWED_CHARS_ADDRESS = 300;

void requestServerShutdown(
        const manage_server_commands::RequestServerShutdownRequest* request,
        manage_server_commands::RequestServerShutdownResponse* response
);

class ManageServerCommandsImpl final : public manage_server_commands::ManageServerCommandsService::Service {
public:
    grpc::Status RequestServerShutdownRPC(
            grpc::ServerContext*,
            const manage_server_commands::RequestServerShutdownRequest* request,
            manage_server_commands::RequestServerShutdownResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting RequestServerShutdownRPC Function...\n";
#endif // DEBUG

        requestServerShutdown(request, response);

#ifdef _DEBUG
        std::cout << "Finishing RequestServerShutdownRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};