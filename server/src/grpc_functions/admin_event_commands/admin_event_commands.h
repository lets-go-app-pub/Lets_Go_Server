//
// Created by jeremiah on 3/7/23.
//

#pragma once

#include <AdminEventCommands.grpc.pb.h>

void addAdminEvent(
        admin_event_commands::AddAdminEventRequest* request,
        admin_event_commands::AddAdminEventResponse* response
);

void getEvents(
        const admin_event_commands::GetEventsRequest* request,
        admin_event_commands::GetEventsResponse* response
);

class AdminEventCommandsServiceImpl final : public admin_event_commands::AdminEventCommandsService::Service {
public:
    grpc::Status AddAdminEventRPC(
            grpc::ServerContext* context [[maybe_unused]],
            const admin_event_commands::AddAdminEventRequest* request,
            admin_event_commands::AddAdminEventResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting AddAdminEventRPC Function...\n";
#endif // DEBUG

        addAdminEvent(const_cast<admin_event_commands::AddAdminEventRequest*>(request), response);

#ifdef _DEBUG
        std::cout << "Finishing AddAdminEventRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status GetEventsRPC(
            grpc::ServerContext* context [[maybe_unused]],
            const admin_event_commands::GetEventsRequest* request,
            admin_event_commands::GetEventsResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting GetEventsRPC Function...\n";
#endif // DEBUG

        getEvents(request, response);

#ifdef _DEBUG
        std::cout << "Finishing GetEventsRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};