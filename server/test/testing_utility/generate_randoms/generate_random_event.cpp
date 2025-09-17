//
// Created by jeremiah on 3/15/23.
//

#include <iostream>
#include <fstream>

#include <mongocxx/client.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include "generate_randoms.h"

#include "connection_pool_global_variable.h"
#include "database_names.h"

#include "generate_random_account_info/generate_random_user_activities.h"
#include "general_values.h"
#include "event_request_message_is_valid.h"
#include "collection_names.h"
#include "assert_macro.h"
#include "create_event.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

CreateEventReturnValues generateRandomEvent(
        const EventChatRoomAdminInfo& chat_room_admin_info,
        const std::string& created_by,
        const std::chrono::milliseconds& current_timestamp,
        EventRequestMessage event_info
) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    EventRequestMessageIsValidReturnValues event_request_returns = eventRequestMessageIsValid(
            &event_info,
            activities_info_collection,
            current_timestamp
    );

    assert_msg(event_request_returns.successful, "eventRequestMessageIsValid() failed when generating a random event.\nmessage: " + event_request_returns.error_message);

    return createEvent(
            mongo_cpp_client,
            accounts_db,
            chat_room_db,
            user_accounts_collection,
            user_pictures_collection,
            current_timestamp,
            created_by,
            chat_room_admin_info,
            &event_info,
            event_request_returns
    );
}