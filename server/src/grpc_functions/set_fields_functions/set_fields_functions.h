//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include "SetFields.grpc.pb.h"

void setBirthday(const setfields::SetBirthdayRequest* request, setfields::SetBirthdayResponse* response);

void setEmail(const setfields::SetEmailRequest* request, setfields::SetFieldResponse* response);

void setGender(const setfields::SetStringRequest* request, setfields::SetFieldResponse* response);

void setFirstName(const setfields::SetStringRequest* request, setfields::SetFieldResponse* response);

void setPicture(setfields::SetPictureRequest* request, setfields::SetFieldResponse* response);

void setCategories(const setfields::SetCategoriesRequest* request, setfields::SetFieldResponse* response);

void setAgeRange(const setfields::SetAgeRangeRequest* request, setfields::SetFieldResponse* response);

void setGenderRange(const setfields::SetGenderRangeRequest* request, setfields::SetFieldResponse* response);

void setUserBio(setfields::SetBioRequest* request, setfields::SetFieldResponse* response);

void setUserCity(setfields::SetStringRequest* request, setfields::SetFieldResponse* response);

void setMaxDistance(const setfields::SetMaxDistanceRequest* request, setfields::SetFieldResponse* response);

void setAlgorithmSearchOptions(
        const setfields::SetAlgorithmSearchOptionsRequest* request,
        setfields::SetFieldResponse* response
);

void setFeedback(const setfields::SetFeedbackRequest* request, setfields::SetFeedbackResponse* response);

void setOptedInToPromotionalEmail(
        const setfields::SetOptedInToPromotionalEmailRequest* request,
        setfields::SetFieldResponse* response
);

class SetFieldsServiceImpl final : public setfields::SetFieldsService::Service {

    grpc::Status SetBirthdayRPC(
            grpc::ServerContext*,
            const setfields::SetBirthdayRequest* request,
            setfields::SetBirthdayResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetBirthdayRPC Function...\n";
#endif // DEBUG

        setBirthday(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetBirthdayRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetEmailRPC(
            grpc::ServerContext*,
            const setfields::SetEmailRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetEmailRPC Function...\n";
#endif // DEBUG

        setEmail(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetEmailRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetGenderRPC(
            grpc::ServerContext*,
            const setfields::SetStringRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetGenderRPC Function...\n";
#endif // DEBUG

        setGender(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetGenderRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetFirstNameRPC(
            grpc::ServerContext*,
            const setfields::SetStringRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetNameRPC Function...\n";
#endif // DEBUG

        setFirstName(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetNameRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetPicturesRPC(
            grpc::ServerContext*,
            const setfields::SetPictureRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetPicturesRPC Function...\n";
#endif // DEBUG

        try {
            //in order to move things the picture and thumbnail byte arrays, moving it OUT of the request is
            // the grpc function implementation IS const, so casting it to non_const here
            setPicture(const_cast<setfields::SetPictureRequest*>(request), response);
        } catch (std::exception& e) {
            std::cout << "Line Number: " << __LINE__ << '\n';
            std::cout << "Server Exception : " << e.what() << '\n';
        }

#ifdef _DEBUG
        std::cout << "Finishing SetPicturesRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetCategoriesRPC(
            grpc::ServerContext*,
            const setfields::SetCategoriesRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetCategoriesRPC Function...\n";
#endif // DEBUG

        setCategories(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetCategoriesRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetAgeRangeRPC(
            grpc::ServerContext*,
            const setfields::SetAgeRangeRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetAgeRangeRPC Function...\n";
#endif // DEBUG

        setAgeRange(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetAgeRangeRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetGenderRangeRPC(
            grpc::ServerContext*,
            const setfields::SetGenderRangeRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetGenderRangeRPC Function...\n";
#endif // DEBUG

        setGenderRange(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetGenderRangeRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetUserBioRPC(
            grpc::ServerContext*,
            const setfields::SetBioRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetUserBioRPC Function...\n";
#endif // DEBUG

        //TESTING_TODO: test the const cast junk
        setUserBio(const_cast<setfields::SetBioRequest*>(request), response);

#ifdef _DEBUG
        std::cout << "Finishing SetUserBioRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetUserCityRPC(
            grpc::ServerContext*,
            const setfields::SetStringRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetUserCityRPC Function...\n";
#endif // DEBUG

        //TESTING_TODO: test the const cast junk
        setUserCity(const_cast<setfields::SetStringRequest*>(request), response);

#ifdef _DEBUG
        std::cout << "Finishing SetUserCityRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetMaxDistanceRPC(
            grpc::ServerContext*,
            const setfields::SetMaxDistanceRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetMaxDistanceRPC Function...\n";
#endif // DEBUG

        setMaxDistance(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetMaxDistanceRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetFeedbackRPC(
            grpc::ServerContext*,
            const setfields::SetFeedbackRequest* request,
            setfields::SetFeedbackResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetFeedbackRPC Function...\n";
#endif // DEBUG

        setFeedback(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetFeedbackRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetAlgorithmSearchOptionsRPC(
            grpc::ServerContext*,
            const setfields::SetAlgorithmSearchOptionsRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetAlgorithmSearchOptionsRPC Function...\n";
#endif // DEBUG

        setAlgorithmSearchOptions(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetAlgorithmSearchOptionsRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

public:
    grpc::Status SetOptedInToPromotionalEmailRPC(
            grpc::ServerContext* context [[maybe_unused]],
            const setfields::SetOptedInToPromotionalEmailRequest* request,
            setfields::SetFieldResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetOptedInToPromotionalEmailRPC Function...\n";
#endif // DEBUG

        setOptedInToPromotionalEmail(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetOptedInToPromotionalEmailRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};
