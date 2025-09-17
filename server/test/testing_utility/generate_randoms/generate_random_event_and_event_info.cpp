//
// Created by jeremiah on 3/16/23.
//

#include <fstream>

#include <bsoncxx/builder/stream/document.hpp>

#include "generate_randoms.h"

#include "database_names.h"
#include "generate_random_account_info/generate_random_user_activities.h"
#include "general_values.h"
#include "event_request_message_is_valid.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

CreateEventReturnValues generateRandomEventAndEventInfo(
        const EventChatRoomAdminInfo& chat_room_admin_info,
        const std::string& created_by,
        const std::chrono::milliseconds& current_timestamp
) {

    EventRequestMessage event_info = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    return generateRandomEvent(
            chat_room_admin_info,
            created_by,
            current_timestamp,
            event_info
    );
}