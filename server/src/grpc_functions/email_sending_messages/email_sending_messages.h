//
// Created by jeremiah on 8/16/21.
//
#pragma once

#include "EmailSendingMessages.grpc.pb.h"
#include <python_send_email_module.h>
#include "helper_functions/helper_functions.h"

//primary function for emailVerification, called from gRPC server implementation
void emailVerification(
        const email_sending_messages::EmailVerificationRequest* request,
        email_sending_messages::EmailVerificationResponse* response,
        SendEmailInterface* send_email_object
);

//primary function for accountRecovery, called from gRPC server implementation
void accountRecovery(
        const email_sending_messages::AccountRecoveryRequest* request,
        email_sending_messages::AccountRecoveryResponse* response,
        SendEmailInterface* send_email_object
);

class EmailSendingMessagesImpl final : public email_sending_messages::EmailSendingMessagesService::Service {
public:
    grpc::Status EmailVerificationRPC(grpc::ServerContext* context [[maybe_unused]],
                                      const email_sending_messages::EmailVerificationRequest* request,
                                      email_sending_messages::EmailVerificationResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting EmailVerificationRPC Function...\n";
#endif // DEBUG

        SendEmailProduction send_email_object{};

        emailVerification(request, response, &send_email_object);

#ifdef _DEBUG
        std::cout << "Finishing EmailVerificationRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status
    AccountRecoveryRPC(grpc::ServerContext* context [[maybe_unused]], const email_sending_messages::AccountRecoveryRequest* request,
                       email_sending_messages::AccountRecoveryResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting AccountRecoveryRPC Function...\n";
#endif // DEBUG

        SendEmailProduction send_email_object{};

        accountRecovery(request, response, &send_email_object);

#ifdef _DEBUG
        std::cout << "Finishing AccountRecoveryRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};
