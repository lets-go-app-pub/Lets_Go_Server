//
// Created by jeremiah on 9/1/21.
//

#pragma once

#include "HandleFeedback.grpc.pb.h"

void updateFeedbackTimeViewed(
        const handle_feedback::UpdateFeedbackTimeViewedRequest* request,
        handle_feedback::UpdateFeedbackTimeViewedResponse* response
        );

void getInitialFeedback(
        const handle_feedback::GetInitialFeedbackRequest* request,
        handle_feedback::GetFeedbackResponse* response
        );

void getFeedbackUpdate(
        const handle_feedback::GetFeedbackUpdateRequest* request,
        handle_feedback::GetFeedbackResponse* response
        );

void setFeedbackToSpam(
        const handle_feedback::SetFeedbackToSpamRequest* request,
        handle_feedback::SetFeedbackToSpamResponse* response
        );

class HandleFeedbackImpl final : public handle_feedback::HandleFeedbackService::Service {
public:

    grpc::Status UpdateFeedbackTimeViewedRPC(
            grpc::ServerContext*,
            const handle_feedback::UpdateFeedbackTimeViewedRequest* request,
            handle_feedback::UpdateFeedbackTimeViewedResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting UpdateFeedbackTimeViewedRPC...\n";
#endif // DEBUG

        updateFeedbackTimeViewed(request, response);

#ifdef _DEBUG
        std::cout << "Finishing UpdateFeedbackTimeViewedRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status GetInitialFeedbackRPC(
            grpc::ServerContext* ,
            const handle_feedback::GetInitialFeedbackRequest* request,
            handle_feedback::GetFeedbackResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting GetInitialFeedbackRPC...\n";
#endif // DEBUG

        getInitialFeedback(request, response);

#ifdef _DEBUG
        std::cout << "Finishing GetInitialFeedbackRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status GetFeedbackUpdateRPC(
            grpc::ServerContext*,
            const handle_feedback::GetFeedbackUpdateRequest* request,
            handle_feedback::GetFeedbackResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting GetFeedbackUpdateRPC...\n";
#endif // DEBUG

        getFeedbackUpdate(request, response);

#ifdef _DEBUG
        std::cout << "Finishing GetFeedbackUpdateRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetFeedbackToSpamRPC(
            grpc::ServerContext*,
            const handle_feedback::SetFeedbackToSpamRequest* request,
            handle_feedback::SetFeedbackToSpamResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting SetFeedbackToSpamRPC...\n";
#endif // DEBUG

        setFeedbackToSpam(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetFeedbackToSpamRPC...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

};