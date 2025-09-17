//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <FindMatches.grpc.pb.h>
#include <utility_testing_functions.h>

#include "matching_algorithm.h"

//primary function for findMatches, called from gRPC server implementation
void findMatches(
        const findmatches::FindMatchesRequest* request,
        grpc::ServerWriterInterface<findmatches::FindMatchesResponse>* response
);

#ifdef _DEBUG
static int counter = 0;
#endif // DEBUG

class FindMatchesServiceImpl final : public findmatches::FindMatchesService::Service {

    grpc::Status FindMatchRPC(grpc::ServerContext*, const findmatches::FindMatchesRequest* request,
                              grpc::ServerWriter<findmatches::FindMatchesResponse>* response) override {
#ifndef _RELEASE
        std::cout << counter++ << " Starting FindMatchRPC Function...\n";
#endif

        findMatches(request, response);

#ifndef _RELEASE
        std::cout << counter++ << " Finishing FindMatchRPC Function...\n";
#endif

        return grpc::Status::OK;
    }

};
