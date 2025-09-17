//
// Created by jeremiah on 3/8/23.
//

#pragma once

#include "UserEventCommands.grpc.pb.h"

void addUserEvent(user_event_commands::AddUserEventRequest* request,
                   user_event_commands::AddUserEventResponse* response);

void cancelEvent(const user_event_commands::CancelEventRequest* request,
                      user_event_commands::CancelEventResponse* response);

class UserEventCommandsServiceImpl final : public user_event_commands::UserEventCommandsService::Service {
public:
    grpc::Status AddUserEventRPC(
            grpc::ServerContext* context [[maybe_unused]],
            const user_event_commands::AddUserEventRequest* request,
            user_event_commands::AddUserEventResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting AddUserEventRPC Function...\n";
#endif // DEBUG

        addUserEvent(const_cast<user_event_commands::AddUserEventRequest*>(request), response);

#ifdef _DEBUG
        std::cout << "Finishing AddUserEventRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status CancelEventRPC(
            grpc::ServerContext* context [[maybe_unused]],
            const user_event_commands::CancelEventRequest* request,
            user_event_commands::CancelEventResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting CancelEventRPC Function...\n";
#endif // DEBUG

        cancelEvent(request, response);

#ifdef _DEBUG
        std::cout << "Finishing CancelEventRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};