//
// Created by jeremiah on 3/21/21.
//
#pragma once

#include <SendPictureForTesting.grpc.pb.h>

void byteArrayStorage(const send_picture_for_testing::SendPicturesForTestingRequest* request,
                      send_picture_for_testing::SendPicturesForTestingResponse* response);

//stores multiple pictures that are sent to the server for building testing accounts
class SendPicturesForTestingServiceImpl final
        : public send_picture_for_testing::SendPicturesForTestingService::Service {

public:
    grpc::Status SendPictureForTestingRPC(::grpc::ServerContext* context [[maybe_unused]],
                                          const ::send_picture_for_testing::SendPicturesForTestingRequest* request,
                                          ::send_picture_for_testing::SendPicturesForTestingResponse* response) override {
#ifndef _RELEASE
        std::cout << "Starting SendPictureForTestingRPC Function...\n";
#endif

        byteArrayStorage(request, response);

#ifndef _RELEASE
        std::cout << "Finishing SendPictureForTestingRPC Function...\n";
#endif

        return grpc::Status::OK;
    }

};