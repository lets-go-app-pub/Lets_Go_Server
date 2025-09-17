//
// Created by jeremiah on 10/29/21.
//

#pragma once

#include <HandleErrors.grpc.pb.h>

void searchErrors(
        const handle_errors::SearchErrorsRequest* request,
        handle_errors::SearchErrorsResponse* response
);

void extractErrors(
        const std::function<void(const std::string& /*key*/, const std::string& /*value*/)>& send_trailing_meta_data,
        const ::handle_errors::ExtractErrorsRequest* request,
        grpc::ServerWriterInterface<::handle_errors::ExtractErrorsResponse>* writer
);

void deleteSingleError(
        const handle_errors::DeleteSingleErrorRequest* request,
        handle_errors::DeleteSingleErrorResponse* response
);

void setErrorToHandled(
        const handle_errors::SetErrorToHandledRequest* request,
        handle_errors::SetErrorToHandledResponse* response
);

class HandleErrorsImpl final : public handle_errors::HandleErrorsService::Service {
public:

    grpc::Status SearchErrorsRPC(
            grpc::ServerContext*,
            const handle_errors::SearchErrorsRequest* request,
            handle_errors::SearchErrorsResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SearchErrorsRPC...\n";
#endif // DEBUG

        searchErrors(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SearchErrorsRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status ExtractErrorsRPC(
            grpc::ServerContext* context,
            const handle_errors::ExtractErrorsRequest* request,
            grpc::ServerWriter<::handle_errors::ExtractErrorsResponse>* writer
    ) override {
#ifdef _DEBUG
        std::cout << "Starting ExtractErrorsRPC...\n";
#endif // DEBUG

        extractErrors(
                [&context](
                        const std::string& key, const std::string& value
                ) {
                    context->AddTrailingMetadata(key, value);
                },
                request,
                writer
        );

#ifdef _DEBUG
        std::cout << "Finishing ExtractErrorsRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status DeleteSingleErrorRPC(
            grpc::ServerContext*,
            const ::handle_errors::DeleteSingleErrorRequest* request,
            handle_errors::DeleteSingleErrorResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting DeleteSingleErrorRPC...\n";
#endif // DEBUG

        deleteSingleError(request, response);

#ifdef _DEBUG
        std::cout << "Finishing DeleteSingleErrorRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetErrorToHandledRPC(
            grpc::ServerContext*,
            const ::handle_errors::SetErrorToHandledRequest* request,
            handle_errors::SetErrorToHandledResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting SetErrorToHandledRPC...\n";
#endif // DEBUG

        setErrorToHandled(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetErrorToHandledRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};