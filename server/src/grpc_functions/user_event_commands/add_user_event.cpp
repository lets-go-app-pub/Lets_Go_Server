//
// Created by jeremiah on 3/8/23.
//

#include "user_event_commands.h"

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <admin_privileges_vector.h>

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "matching_algorithm.h"
#include "user_pictures_keys.h"
#include "event_request_message_is_valid.h"
#include "create_event.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void addUserEventImplementation(user_event_commands::AddUserEventRequest* request,
                                 user_event_commands::AddUserEventResponse* response);

void addUserEvent(user_event_commands::AddUserEventRequest* request,
                  user_event_commands::AddUserEventResponse* response) {
    handleFunctionOperationException(
            [&] {
                addUserEventImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void addUserEventImplementation(user_event_commands::AddUserEventRequest* request,
                                 user_event_commands::AddUserEventResponse* response) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(basic_info_return_status);
        return;
    }

    //NOTE: Login token will be checked below inside

    //Qr code is not allowed for user events
    if(!request->event_request().qr_code_message().empty()
        || request->event_request().qr_code_file_size() > 0
        || !request->event_request().qr_code_file_in_bytes().empty()
        ) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
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

    //This variable cannot be const, createEvent will move values out of it.
    EventRequestMessageIsValidReturnValues event_request_returns = eventRequestMessageIsValid(
            event_info,
            activities_info_collection,
            current_timestamp
    );

    if(event_request_returns.return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(event_request_returns.return_status);
        return;
    }

    const EventChatRoomAdminInfo chat_room_admin_info {
        UserAccountType::USER_GENERATED_EVENT_TYPE,
        user_account_oid_str,
        login_token_str,
        installation_id
    };

    const auto return_value = createEvent(
        mongo_cpp_client,
        accounts_db,
        chat_room_db,
        user_accounts_collection,
        user_pictures_collection,
        current_timestamp,
        user_account_oid_str,
        chat_room_admin_info,
        request->mutable_event_request(),
        event_request_returns
    );

    if(return_value.return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(return_value.return_status);
        return;
    }

    response->mutable_chat_room_info()->CopyFrom(return_value.chat_room_return_info);
    response->set_event_oid(return_value.event_account_oid);
    response->set_timestamp_ms(current_timestamp.count());

    response->set_return_status(ReturnStatus::SUCCESS);
}