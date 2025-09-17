//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <RequestFields.grpc.pb.h>

void requestPhoneNumber(
        const request_fields::InfoFieldRequest* request,
        request_fields::InfoFieldResponse* response
);

void requestBirthday(
        const request_fields::InfoFieldRequest* request,
        request_fields::BirthdayResponse* response
);

void requestEmail(
        const request_fields::InfoFieldRequest* request,
        request_fields::EmailResponse* response
);

void requestGender(
        const request_fields::InfoFieldRequest* request,
        request_fields::InfoFieldResponse* response
);

void requestName(
        const request_fields::InfoFieldRequest* request,
        request_fields::InfoFieldResponse* response
);

void requestPictures(
        const request_fields::PictureRequest* request,
        grpc::ServerWriterInterface<request_fields::PictureResponse>* response
);

void requestCategories(
        const request_fields::InfoFieldRequest* request,
        request_fields::CategoriesResponse* response
);

void requestPostLoginInfo(
        const request_fields::InfoFieldRequest* request,
        request_fields::PostLoginInfoResponse* response
);

void requestServerIcons(
        const request_fields::ServerIconsRequest* request,
        grpc::ServerWriterInterface<request_fields::ServerIconsResponse>* response
);

void requestServerActivities(
        const request_fields::InfoFieldRequest* request,
        request_fields::ServerActivitiesResponse* response
);

class RequestFieldsServiceImpl final : public request_fields::RequestFieldsService::Service {

    grpc::Status RequestPhoneNumberRPC(
            grpc::ServerContext*,
            const request_fields::InfoFieldRequest* request,
            request_fields::InfoFieldResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting requestPhoneNumber Function...\n";
#endif // DEBUG

        requestPhoneNumber(request, response);

#ifdef _DEBUG
        std::cout << "Finishing requestPhoneNumber Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RequestBirthdayRPC(
            grpc::ServerContext*,
            const request_fields::InfoFieldRequest* request,
            request_fields::BirthdayResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting requestBirthday Function...\n";
#endif // DEBUG

        requestBirthday(request, response);

#ifdef _DEBUG
        std::cout << "Finishing requestBirthday Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RequestEmailRPC(
            grpc::ServerContext*,
            const request_fields::InfoFieldRequest* request,
            request_fields::EmailResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting requestEmail Function...\n";
#endif // DEBUG

        requestEmail(request, response);

#ifdef _DEBUG
        std::cout << "Finishing requestEmail Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RequestGenderRPC(
            grpc::ServerContext*,
            const request_fields::InfoFieldRequest* request,
            request_fields::InfoFieldResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting RequestGender Function...\n";
#endif // DEBUG

        requestGender(request, response);

#ifdef _DEBUG
        std::cout << "Finishing RequestGender Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RequestNameRPC(
            grpc::ServerContext*,
            const request_fields::InfoFieldRequest* request,
            request_fields::InfoFieldResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting requestName Function...\n";
#endif // DEBUG

        requestName(request, response);

#ifdef _DEBUG
        std::cout << "Finishing requestName Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RequestPicturesRPC(
            grpc::ServerContext*,
            const request_fields::PictureRequest* request,
            grpc::ServerWriter<request_fields::PictureResponse>* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting RequestPictures Function...\n";
#endif // DEBUG

        requestPictures(request, response);

#ifdef _DEBUG
        std::cout << "Finishing requestPictures Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RequestCategoriesRPC(
            grpc::ServerContext*,
            const request_fields::InfoFieldRequest* request,
            request_fields::CategoriesResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting requestCategories Function...\n";
#endif // DEBUG

        requestCategories(request, response);

#ifdef _DEBUG
        std::cout << "Finishing requestCategories Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RequestPostLoginInfoRPC(
            grpc::ServerContext*,
            const request_fields::InfoFieldRequest* request,
            request_fields::PostLoginInfoResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting RequestAgeRange Function...\n";
#endif // DEBUG

        requestPostLoginInfo(request, response);

#ifdef _DEBUG
        std::cout << "Finishing RequestAgeRange Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RequestServerIconsRPC(
            grpc::ServerContext*,
            const request_fields::ServerIconsRequest* request,
            grpc::ServerWriter<request_fields::ServerIconsResponse>* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting RequestServerIconsRPC Function...\n";
#endif // DEBUG

        requestServerIcons(request, response);

#ifdef _DEBUG
        std::cout << "Finishing RequestServerIconsRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RequestServerActivitiesRPC(
            grpc::ServerContext*,
            const request_fields::InfoFieldRequest* request,
            request_fields::ServerActivitiesResponse* response
    ) override {

#ifdef _DEBUG
        std::cout << "Starting RequestServerActivitiesRPC Function...\n";
#endif // DEBUG

        requestServerActivities(request, response);

#ifdef _DEBUG
        std::cout << "Finishing RequestServerActivitiesRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

};
