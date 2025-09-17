//
// Created by jeremiah on 3/18/21.
//
#pragma once

#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/exception.hpp>

#include <LoginFunction.grpc.pb.h>
#include <StatusEnum.grpc.pb.h>
#include "handle_function_operation_exception.h"

//primary function called from header file (returns the verification code sent, used for testing purposes)
std::string loginFunctionMongoDb(const loginfunction::LoginRequest* request, loginfunction::LoginResponse* response, const std::string& caller_uri);

class LoginServiceImpl final : public loginfunction::LoginService::Service {
    grpc::Status LoginRPC(grpc::ServerContext* context [[maybe_unused]], const loginfunction::LoginRequest* request,
                          loginfunction::LoginResponse* response) override {

        try {
#ifdef _DEBUG
            std::cout << "Starting Login Function...\n";
#endif // DEBUG

            loginFunctionMongoDb(request, response, context->peer());

#ifdef _DEBUG
            std::cout << "Finishing Login Function...\n";
#endif // DEBUG
        }
        catch (std::exception& e) {
            std::cout << e.what() << '\n';
        }

        return grpc::Status::OK;
    }
};

