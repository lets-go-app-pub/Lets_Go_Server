//
// Created by jeremiah on 9/21/21.
//

#include "admin_chat_room_commands.h"

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>
#include <utility_chat_functions.h>
#include <admin_functions_for_request_values.h>


#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void getSingleMessageImplementation(const ::admin_chat_commands::GetSingleMessageRequest* request,
                                    ::admin_chat_commands::GetSingleMessageResponse* response);

void getSingleMessage(const ::admin_chat_commands::GetSingleMessageRequest* request,
                      ::admin_chat_commands::GetSingleMessageResponse* response
) {
    handleFunctionOperationException(
            [&] {
                getSingleMessageImplementation(request, response);
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            }, __LINE__, __FILE__, request);
}

void getSingleMessageImplementation(const ::admin_chat_commands::GetSingleMessageRequest* request,
                                    ::admin_chat_commands::GetSingleMessageResponse* response) {
    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationID;

    auto error_func = [&response](const std::string& error_str) {
        response->set_successful(false);
        response->set_error_message(error_str);
    };

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
            request->login_info(),
            userAccountOIDStr,
            loginTokenStr,
            installationID,
            store_error_message
    );

    if (basicInfoReturnStatus != ReturnStatus::SUCCESS) {
        response->set_successful(false);
        error_func("ReturnStatus: " + ReturnStatus_Name(basicInfoReturnStatus) + " " + error_message);
        return;
    } else if (!admin_info_doc_value) {
        error_func("Could not find admin document.");
        return;
    }

    bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;

    auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
    if (admin_privilege_element &&
        admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__,
                        admin_privilege_element,
                        admin_info_doc_view, bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME);

        error_func("Error stored on server.");
        return;
    }

    if (!admin_privileges[admin_level].request_chat_messages()) {
        error_func("Admin level " + AdminLevelEnum_Name(admin_level) +
                   " does not have 'request_chat_messages' access.");
        return;
    }

    if (isInvalidChatRoomId(request->chat_room_id())) {
        error_func("Chat room id '" + request->chat_room_id() +
                   "' is invalid.");
        return;
    }

    if (isInvalidUUID(request->message_uuid())) {
        error_func("Message UUID '" + request->message_uuid() +
                   "' is invalid.");
        return;
    }

    if (request->timestamp_of_message() <= 0) {
        error_func("Message timestamp '" +
                   getDateTimeStringFromTimestamp(std::chrono::milliseconds{request->timestamp_of_message()}) +
                   "' is invalid.");
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + request->chat_room_id()];

    if(extractSingleMessageAtTimeStamp(
            mongo_cpp_client, accounts_db,
            user_accounts_collection,
            chat_room_collection,
            request->chat_room_id(),
            request->message_uuid(),
            std::chrono::milliseconds{request->timestamp_of_message()},
            response->mutable_message(),
            error_func
    )) {
        response->set_successful(true);
    }

}


