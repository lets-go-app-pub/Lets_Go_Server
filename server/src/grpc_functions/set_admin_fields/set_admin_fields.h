//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include "SetAdminFields.grpc.pb.h"

/** Function adds a single category or activity to its respective array **/
/** arrays stored inside unique document with _id == ID inside ACTIVITIES_INFO_COLLECTION_NAME **/
/** 2 arrays, 1 for categories and 1 for activities **/
/** name of each activity must be unique and name of each category must be unique **/
/** Function flow (done inside a single aggregation step) **/
/** 1) the category or activity is searched by name **/
/** 2) if the name exists inside the array, the array element is update to the passed element **/
/** 3) if the name does not in the array the passed element will be appended to the array **/
bool setServerCategoryOrActivity(
        set_admin_fields::SetServerActivityOrCategoryRequest& request,
        std::string& error_message,
        bool is_activity
);

//primary function for SetServerIconRPC, called from gRPC server implementation
bool setServerIcon(
        set_admin_fields::SetServerIconRequest& request,
        std::string& error_message
);

void setServerAccessStatus(
        const set_admin_fields::SetAccessStatusRequest* request,
        set_admin_fields::SetAccessStatusResponse* response
);

void removeUserPicture(
        const set_admin_fields::RemoveUserPictureRequest* request,
        set_admin_fields::SetAdminUnaryCallResponse* response
);

//visible for testing
std::string convertCategoryActivityNameToStoredName(const std::string& original_name);

class SetAdminFieldsImpl final : public set_admin_fields::SetAdminFieldsService::Service {
public:
    grpc::Status SetServerCategoryRPC(
            grpc::ServerContext*,
            grpc::ServerReader<set_admin_fields::SetServerActivityOrCategoryRequest>* request,
            set_admin_fields::SetServerValuesResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetServerCategoryRPC Function...\n";
#endif // DEBUG

        set_admin_fields::SetServerActivityOrCategoryRequest set_category_request;

        std::string error_message;
        bool successful = true;
        while (request->Read(&set_category_request)) {
            if (!setServerCategoryOrActivity(set_category_request, error_message, false)) {
                response->add_error_messages(error_message);
                successful = false;
                error_message = "";
            }
        }

        response->set_successful(successful);

#ifdef _DEBUG
        std::cout << "Finishing SetServerCategoryRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetServerActivityRPC(
            grpc::ServerContext*,
            grpc::ServerReader<set_admin_fields::SetServerActivityOrCategoryRequest>* request,
            set_admin_fields::SetServerValuesResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetServerActivityRPC Function...\n";
#endif // DEBUG

        set_admin_fields::SetServerActivityOrCategoryRequest set_activity_request;

        std::string error_message;
        bool successful = true;
        while (request->Read(&set_activity_request)) {
            if (!setServerCategoryOrActivity(set_activity_request, error_message, true)) {
                response->add_error_messages(error_message);
                successful = false;
                error_message = "";
            }
        }

        response->set_successful(successful);

#ifdef _DEBUG
        std::cout << "Finishing SetServerActivityRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetServerIconRPC(
            grpc::ServerContext*,
            grpc::ServerReader<set_admin_fields::SetServerIconRequest>* request,
            set_admin_fields::SetServerValuesResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetServerIconRPC Function...\n";
#endif // DEBUG

        set_admin_fields::SetServerIconRequest set_icons_request;

        std::string error_message;
        bool successful = true;
        while (request->Read(&set_icons_request)) {
            if (!setServerIcon(set_icons_request, error_message)) {
                successful = false;
            }
        }

        response->set_successful(successful);

#ifdef _DEBUG
        std::cout << "Finishing SetServerIconRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetServerAccessStatusRPC(
            grpc::ServerContext*,
            const ::set_admin_fields::SetAccessStatusRequest* request,
            set_admin_fields::SetAccessStatusResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting SetServerAccessStatusRPC Function...\n";
#endif // DEBUG

        setServerAccessStatus(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetServerAccessStatusRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }


    grpc::Status RemoveUserPictureRPC(
            grpc::ServerContext*,
            const set_admin_fields::RemoveUserPictureRequest* request,
            set_admin_fields::SetAdminUnaryCallResponse* response
    ) override {
#ifdef _DEBUG
        std::cout << "Starting RemoveUserPictureRPC Function...\n";
#endif // DEBUG

        removeUserPicture(request, response);

#ifdef _DEBUG
        std::cout << "Finishing RemoveUserPictureRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }
};
