//
// Created by jeremiah on 8/26/21.
//

#pragma once

#include "RequestAdminInfo.grpc.pb.h"

void initialProgramOpenRequestInfo(
        const request_admin_info::InitialProgramOpenRequestInfoRequest* request,
        request_admin_info::InitialProgramOpenRequestInfoResponse* response
);

class RequestAdminInfoImpl final : public request_admin_info::RequestAdminInfoService::Service {
public:
    grpc::Status InitialProgramOpenRequestInfoRPC(
            grpc::ServerContext*,
            const request_admin_info::InitialProgramOpenRequestInfoRequest* request,
            request_admin_info::InitialProgramOpenRequestInfoResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting InitialProgramOpenRequestInfoRPC...\n";
#endif // DEBUG

        initialProgramOpenRequestInfo(request, response);

#ifdef _DEBUG
        std::cout << "Finishing InitialProgramOpenRequestInfoRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};