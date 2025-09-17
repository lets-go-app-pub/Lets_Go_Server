//
// Created by jeremiah on 3/13/23.
//

#pragma once

#include <string>

#include <mongocxx/collection.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include "EventRequestMessage.grpc.pb.h"
#include "StatusEnum.grpc.pb.h"
#include "UserAccountType.pb.h"

#include "event_request_message_is_valid.h"
#include "CreatedChatRoomInfo.pb.h"

struct CreateEventReturnValues {

    //Will set as SUCCESS if function completed successfully. If an error occurred, LG_ERROR will be set and
    // error_message will have a string attached.
    ReturnStatus return_status = ReturnStatus::LG_ERROR;
    std::string error_message = "error_message not yet set.";

    std::string event_account_oid;

    void setError(std::string _error_message) {
        return_status = ReturnStatus::LG_ERROR;
        error_message = std::move(_error_message);
    }

    CreatedChatRoomInfo chat_room_return_info;
};

//If an error occurs, the function setError() will be called, and it will return LG_ERROR with an error message.
// ReturnStatus can also be set to other values if the login had a problem. If ReturnStatus::SUCCESS is set, then
// the function completed successfully. This will also mean that the basic chat room info is saved to
// chat_room_return_info. If this is a user generated chat room, the initial chat room messages will also be saved
// inside chat_room_return_info.
//Unfortunately there are a few 'parameter packs' that need to be passed in.
// 1) EventChatRoomAdminInfo has two constructors, one for USER_GENERATED_EVENT_TYPE and one for ADMIN_GENERATED_EVENT_TYPE.
// 2) EventRequestMessageIsValidReturnValues is a return from the parameters being checked by eventRequestMessageIsValid().
//  However, it should be assumed that the original value is consumed (values are moved out) after this function has
//  completed.
//filtered_event_info is expected to be checked by eventRequestMessageIsValid() before being sent in.
CreateEventReturnValues createEvent(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::database& chat_room_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::collection& user_pictures_collection,
        const std::chrono::milliseconds& current_timestamp,
        const std::string& created_by,
        const EventChatRoomAdminInfo& chat_room_admin_info,
        EventRequestMessage* filtered_event_info,
        EventRequestMessageIsValidReturnValues& event_request_returns
);