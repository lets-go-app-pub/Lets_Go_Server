//proto file is UserMatchOptions.proto
#pragma once

#include "UserMatchOptions.grpc.pb.h"

//primary function for YesRPC, called from gRPC server implementation
void receiveMatchYes(const UserMatchOptionsRequest* request, UserMatchOptionsResponse* response);

//primary function for NoRPC, called from gRPC server implementation
void receiveMatchNo(const UserMatchOptionsRequest* request, UserMatchOptionsResponse* response);

//primary function for UpdateSingleMatchMember, called from gRPC server implementation
void updateSingleMatchMember(const user_match_options::UpdateSingleMatchMemberRequest* request,
                                     UpdateOtherUserResponse* response);

class UserMatchOptionsImpl final : public user_match_options::UserMatchOptionsService::Service {

    grpc::Status YesRPC(grpc::ServerContext* context [[maybe_unused]], const UserMatchOptionsRequest* request,
                        UserMatchOptionsResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting YesRPC Function...\n";
#endif // DEBUG

        receiveMatchYes(request, response);

#ifdef _DEBUG
        std::cout << "Finishing YesRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status NoRPC(grpc::ServerContext* context [[maybe_unused]], const UserMatchOptionsRequest* request,
                       UserMatchOptionsResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting NoRPC Function...\n";
#endif // DEBUG

        receiveMatchNo(request, response);

#ifdef _DEBUG
        std::cout << "Finishing NoRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status UpdateSingleMatchMember(grpc::ServerContext* context [[maybe_unused]],
                                         const user_match_options::UpdateSingleMatchMemberRequest* request,
                                         UpdateOtherUserResponse* response) override {

        #ifdef _DEBUG
        std::cout << "Starting UpdateSingleMatchMember Function...\n";
#endif // DEBUG

        updateSingleMatchMember(request, response);

#ifdef _DEBUG
        std::cout << "Finishing UpdateSingleMatchMember Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};