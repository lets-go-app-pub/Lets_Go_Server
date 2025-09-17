//
// Created by jeremiah on 8/28/21.
//

#pragma once

#include "RequestStatistics.grpc.pb.h"

void runMatchingActivityStatistics(
        const ::request_statistics::MatchingActivityStatisticsRequest* request,
        ::request_statistics::MatchingActivityStatisticsResponse* response
);

void runMatchingAgeGenderStatistics(
        const request_statistics::AgeGenderStatisticsRequest* request,
        request_statistics::AgeGenderStatisticsResponse* response
);

class RequestStatisticsImpl final : public request_statistics::RequestStatisticsService::Service {
public:

    grpc::Status MatchingActivityStatisticsRPC(::grpc::ServerContext* context [[maybe_unused]],
                                               const ::request_statistics::MatchingActivityStatisticsRequest* request,
                                               ::request_statistics::MatchingActivityStatisticsResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting MatchingActivityStatisticsRPC...\n";
#endif // DEBUG

        runMatchingActivityStatistics(request, response);

#ifdef _DEBUG
        std::cout << "Finishing MatchingActivityStatisticsRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status MatchingAgeGenderRPC(::grpc::ServerContext* context [[maybe_unused]],
                                      const ::request_statistics::AgeGenderStatisticsRequest* request,
                                      ::request_statistics::AgeGenderStatisticsResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting MatchingAgeGenderRPC...\n";
#endif // DEBUG

        runMatchingAgeGenderStatistics(request, response);

#ifdef _DEBUG
        std::cout << "Finishing MatchingAgeGenderRPC...\n";
#endif // DEBUG


        return grpc::Status::OK;
    }

};