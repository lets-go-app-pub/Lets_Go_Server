//
// Created by jeremiah on 9/10/21.
//

#pragma once

#include "RequestUserAccountInfo.grpc.pb.h"

void runRequestUserAccountInfo(
        const RequestUserAccountInfoRequest* request,
        RequestUserAccountInfoResponse* response
        );

class RequestUserAccountInfoServiceImpl final : public RequestUserAccountInfoService::Service {
public:
    grpc::Status RequestUserAccountInfoRPC(
            grpc::ServerContext*,
            const RequestUserAccountInfoRequest* request,
            RequestUserAccountInfoResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting RequestUserAccountInfoRPC...\n";
#endif // DEBUG

        runRequestUserAccountInfo(request, response);

#ifdef _DEBUG
        std::cout << "Finishing RequestUserAccountInfoRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

};