//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <SMSVerification.grpc.pb.h>


//primary function for SMS Verification, called from gRPC server implementation
void sMSVerification(
        const sms_verification::SMSVerificationRequest* request,
        sms_verification::SMSVerificationResponse* response
);

class SMSVerificationServiceImpl final : public sms_verification::SMSVerificationService::Service {
    grpc::Status SMSVerificationRPC(
            grpc::ServerContext*,
            const sms_verification::SMSVerificationRequest* request,
            sms_verification::SMSVerificationResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SMSVerification Function...\n";
#endif // DEBUG

        sMSVerification(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SMSVerification Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};



