//
// Created by jeremiah on 9/20/21.
//

#pragma once

#include "HandleReports.grpc.pb.h"

void requestReports(
        const handle_reports::RequestReportsRequest* request,
        grpc::ServerWriterInterface<::handle_reports::RequestReportsResponse>* writer
);

void timeOutUser(
        const handle_reports::TimeOutUserRequest* request,
        handle_reports::TimeOutUserResponse* response
);

void dismissReport(
        const handle_reports::ReportUnaryCallRequest* request,
        handle_reports::ReportResponseUnaryCallResponse* response
);

void setReportToSpam(
        const handle_reports::ReportUnaryCallRequest* request,
        handle_reports::ReportResponseUnaryCallResponse* response
);

class HandleReportsServiceImpl final : public handle_reports::HandleReportsService::Service {
public:

    grpc::Status RequestReportsRPC(
            grpc::ServerContext*,
            const handle_reports::RequestReportsRequest* request,
            grpc::ServerWriter<::handle_reports::RequestReportsResponse>* response_writer
    ) override {
        #ifdef _DEBUG
        std::cout << "Starting RequestReportsRPC Function...\n";
#endif // DEBUG

        requestReports(request, response_writer);

#ifdef _DEBUG
        std::cout << "Finishing RequestReportsRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status TimeOutUserRPC(
            grpc::ServerContext*,
            const handle_reports::TimeOutUserRequest* request,
            handle_reports::TimeOutUserResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting TimeOutUserRPC Function...\n";
#endif // DEBUG

        timeOutUser(request, response);

#ifdef _DEBUG
        std::cout << "Finishing TimeOutUserRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status DismissReportRPC(
            grpc::ServerContext*,
            const handle_reports::ReportUnaryCallRequest* request,
            handle_reports::ReportResponseUnaryCallResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting DismissReportRPC Function...\n";
#endif // DEBUG

        dismissReport(request, response);

#ifdef _DEBUG
        std::cout << "Finishing DismissReportRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetReportToSpamRPC(
            grpc::ServerContext*,
            const handle_reports::ReportUnaryCallRequest* request,
            handle_reports::ReportResponseUnaryCallResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetReportToSpamRPC Function...\n";
#endif // DEBUG

        setReportToSpam(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetReportToSpamRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};