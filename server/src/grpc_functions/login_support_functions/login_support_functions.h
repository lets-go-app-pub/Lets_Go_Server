//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include "LoginSupportFunctions.grpc.pb.h"

//primary delete account function (calls deleteAccount), called from gRPC server implementation
void deleteAccount(const loginsupport::LoginSupportRequest* request, loginsupport::LoginSupportResponse* response);

//primary function for logout, called from gRPC server implementation
void logoutFunctionMongoDb(const loginsupport::LoginSupportRequest* request, loginsupport::LoginSupportResponse* response);

//primary find missing info needed for verification function, called from gRPC server implementation
void findNeededVerificationInfo(const loginsupport::NeededVeriInfoRequest* request,
                                loginsupport::NeededVeriInfoResponse* response);

class LoginSupportFunctionsServiceImpl final : public loginsupport::LoginSupportService::Service {

    grpc::Status LogoutRPC(
            grpc::ServerContext*,
            const loginsupport::LoginSupportRequest* request,
            loginsupport::LoginSupportResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting Logout Function...\n";
#endif // DEBUG

        logoutFunctionMongoDb(request, response);

#ifdef _DEBUG
        std::cout << "Finishing Logout Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status DeleteAccountRPC(
            grpc::ServerContext*,
            const loginsupport::LoginSupportRequest* request,
            loginsupport::LoginSupportResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting deleteAccount Function...\n";
#endif // DEBUG

        deleteAccount(request, response);

#ifdef _DEBUG
        std::cout << "Finishing deleteAccount Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status NeededVeriInfoRPC(
            grpc::ServerContext*,
            const loginsupport::NeededVeriInfoRequest* request,
            loginsupport::NeededVeriInfoResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting FindNeededVerificationInfo Function...\n";
#endif // DEBUG

        findNeededVerificationInfo(request, response);

#ifdef _DEBUG
        std::cout << "Finishing FindNeededVerificationInfo Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

};
