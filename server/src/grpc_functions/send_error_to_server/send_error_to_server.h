//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <SendErrorToServer.grpc.pb.h>

//primary function for SendErrorToServer, called from gRPC server implementation
void sendErrorToServerMongoDb(const send_error_to_server::SendErrorRequest* request,
                              send_error_to_server::SendErrorResponse* response);

class SendErrorToServerServiceImpl final : public send_error_to_server::SendErrorService::Service {
    grpc::Status SendErrorRPC(grpc::ServerContext* context [[maybe_unused]], const send_error_to_server::SendErrorRequest* request,
                              send_error_to_server::SendErrorResponse* response) override {

#ifdef _DEBUG
        std::cout << "Starting SendErrorToServer Function...\n";
#endif // DEBUG

        sendErrorToServerMongoDb(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SendErrorToServer Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};
