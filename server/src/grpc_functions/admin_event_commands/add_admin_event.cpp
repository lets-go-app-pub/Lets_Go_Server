//
// Created by jeremiah on 3/7/23.
//

#include "admin_event_commands.h"

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "matching_algorithm.h"
#include "user_pictures_keys.h"
#include "create_complete_chat_room_with_user.h"
#include "event_request_message_is_valid.h"
#include "create_event.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void addAdminEventImplementation(admin_event_commands::AddAdminEventRequest* request,
                                 admin_event_commands::AddAdminEventResponse* response);

void addAdminEvent(admin_event_commands::AddAdminEventRequest* request,
                   admin_event_commands::AddAdminEventResponse* response
) {
    handleFunctionOperationException(
            [&] {
                addAdminEventImplementation(request, response);
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

void addAdminEventImplementation(
        admin_event_commands::AddAdminEventRequest* request,
        admin_event_commands::AddAdminEventResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    auto error_func = [&response](const std::string& error_str) {
        response->set_successful(false);
        response->set_error_message(error_str);
    };

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

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_successful(false);
        error_func("ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
        return;
    } else if (!admin_info_doc_value) {
        error_func("Could not find admin document.");
        return;
    }

    bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;

    auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
    if (admin_privilege_element
        && admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__,
                        admin_privilege_element,
                        admin_info_doc_view, bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME);

        error_func("Error stored on server.");
        return;
    }

    if (!admin_privileges[admin_level].add_events()) {
        error_func("Admin level " + AdminLevelEnum_Name(admin_level) +
                   " does not have 'add_events' access.");
        return;
    }

    EventRequestMessage* event_info = request->mutable_event_request();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    const std::chrono::milliseconds& current_timestamp = getCurrentTimestamp();

    EventRequestMessageIsValidReturnValues event_request_returns = eventRequestMessageIsValid(
            event_info,
            activities_info_collection,
            current_timestamp
    );

    if(!event_request_returns.successful) {
        response->set_successful(false);
        response->set_error_message(event_request_returns.error_message);
        return;
    }

    const EventChatRoomAdminInfo chat_room_admin_info {
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    };

    const auto return_value = createEvent(
            mongo_cpp_client,
            accounts_db,
            chat_room_db,
            user_accounts_collection,
            user_pictures_collection,
            current_timestamp,
            request->login_info().admin_name(),
            chat_room_admin_info,
            event_info,
            event_request_returns
    );

    if(return_value.return_status != ReturnStatus::SUCCESS) {
        if(return_value.return_status != ReturnStatus::LG_ERROR) {
            response->set_error_message("Invalid return status was returned.\nReturnStatus: " + ReturnStatus_Name(return_value.return_status));
        } else {
            response->set_error_message(return_value.error_message);
        }
        response->set_successful(false);
        return;
    }

    response->set_chat_room_id(return_value.chat_room_return_info.chat_room_id());
    response->set_successful(true);

    //Note: These values are returned for testing purposes.
    response->set_timestamp_ms(current_timestamp.count());
    response->set_event_oid(return_value.event_account_oid);
}