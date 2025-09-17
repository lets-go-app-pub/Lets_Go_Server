//
// Created by jeremiah on 9/10/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <AdminLevelEnum.grpc.pb.h>
#include <admin_functions_for_request_values.h>

#include "request_user_account_info.h"

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "admin_privileges_vector.h"
#include "connection_pool_global_variable.h"
#include "request_helper_functions.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "user_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void runRequestUserAccountInfoImplementation(
        const RequestUserAccountInfoRequest* request,
        RequestUserAccountInfoResponse* response
);

void runRequestUserAccountInfo(
        const RequestUserAccountInfoRequest* request,
        RequestUserAccountInfoResponse* response
) {
    handleFunctionOperationException(
            [&] {
                runRequestUserAccountInfoImplementation(request, response);
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request
    );
}

void runRequestUserAccountInfoImplementation(
        const RequestUserAccountInfoRequest* request,
        RequestUserAccountInfoResponse* response
) {

    {
        bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
        std::string error_message;
        std::string user_account_oid_str;
        std::string login_token_str;
        std::string installation_id;

        auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                       const std::string& passed_error_message) {
            error_message = passed_error_message;
            admin_info_doc_value = std::move(returned_admin_info_doc);
        };

        const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id,
                store_error_message
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS || !admin_info_doc_value) {
            response->set_success(false);
            response->set_error_msg("ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
            return;
        }

        const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
        AdminLevelEnum admin_level;

        auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
        if (admin_privilege_element
            && admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
        } else { //if element does not exist or is not type oid
            logElementError(__LINE__, __FILE__,
                            admin_privilege_element,
                            admin_info_doc_view, bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME);

            response->set_success(false);
            response->set_error_msg("Error stored on server.");
            return;
        }

        if (!request->request_event() && !admin_privileges[admin_level].find_single_users()) {
            response->set_success(false);
            response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                    " does not have 'find_single_users' access.");
            return;
        } else if (request->request_event() && !admin_privileges[admin_level].find_single_event()) {
            response->set_success(false);
            response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                    " does not have 'find_single_event' access.");
            return;
        }
    }

    bsoncxx::builder::stream::document query_doc;

    if (request->account_identification_case() != RequestUserAccountInfoRequest::AccountIdentificationCase::kUserAccountOid
         && request->account_identification_case() != RequestUserAccountInfoRequest::AccountIdentificationCase::kUserPhoneNumber
    ) {
        response->set_success(false);
        response->set_error_msg("Must send in an account ID or a phone number for account.");
        return;
    } else if (request->account_identification_case() == RequestUserAccountInfoRequest::AccountIdentificationCase::kUserAccountOid) {
        if (isInvalidOIDString(request->user_account_oid())) {
            response->set_success(false);
            response->set_error_msg("Invalid account ID passed.");
            return;
        }

        query_doc
                << "_id" << bsoncxx::oid{request->user_account_oid()};
    } else { //phone number
        if (!isValidPhoneNumber(request->user_phone_number())) {
            response->set_success(false);
            response->set_error_msg("Invalid account phone number passed.");
            return;
        }

        query_doc
                << user_account_keys::PHONE_NUMBER << request->user_phone_number();
    }

    if(request->request_event()) {
        query_doc
            << user_account_keys::ACCOUNT_TYPE << open_document
                << "$gte" << UserAccountType::ADMIN_GENERATED_EVENT_TYPE
            << close_document;
    } else {
        query_doc
            << user_account_keys::ACCOUNT_TYPE << UserAccountType::USER_ACCOUNT_TYPE;
    }

    auto error_fxn = [&response](const std::string& error_str){
        response->set_success(false);
        response->set_error_msg(error_str);
    };

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection deleted_user_accounts_collection = deleted_db[collection_names::DELETED_DELETED_ACCOUNTS_COLLECTION_NAME];

    if(extractUserInfo(
            mongo_cpp_client,
            accounts_db,
            deleted_db,
            user_accounts_collection,
            deleted_user_accounts_collection,
            !request->user_account_oid().empty(),
            query_doc,
            response->mutable_user_account_info(),
            error_fxn)
   ) {
        response->set_success(true);
    }
}


