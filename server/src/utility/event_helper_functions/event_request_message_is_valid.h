//
// Created by jeremiah on 3/8/23.
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

struct EventRequestMessageIsValidReturnValues {
    ReturnStatus return_status;

    std::string error_message;
    bool successful;

    bool user_matches_with_everyone = false;
    std::vector<std::string> gender_range_vector;

    std::chrono::milliseconds event_start_time{};
    std::chrono::milliseconds event_stop_time{};

    int category_index = 0;

    std::vector<bsoncxx::oid> picture_oids{};
    bsoncxx::builder::basic::array pictures_array{};

    bool qr_code_is_set = false;

    EventRequestMessageIsValidReturnValues() = delete;

    EventRequestMessageIsValidReturnValues(
            ReturnStatus _return_status,
            std::string _error_message,
            bool _successful
    ) : return_status(_return_status),
        error_message(std::move(_error_message)),
        successful(_successful) {}

};

struct EventChatRoomAdminInfo {
    const UserAccountType event_type;

    //These values are needed for a user created event.
    const std::string user_account_oid_str;
    const std::string login_token_str;
    const std::string installation_id;

    EventChatRoomAdminInfo() = delete;

    //Constructor for user generated event.
    EventChatRoomAdminInfo(
            UserAccountType _event_type,
            std::string _user_account_oid_str,
            std::string _login_token_str,
            std::string _installation_id
    ) :
            event_type(_event_type),
            user_account_oid_str(std::move(_user_account_oid_str)),
            login_token_str(std::move(_login_token_str)),
            installation_id(std::move(_installation_id))
            {}

    //Constructor for admin generated event.
    explicit EventChatRoomAdminInfo(
            UserAccountType _event_type
    ) :
            event_type(_event_type) {}
};

EventRequestMessageIsValidReturnValues eventRequestMessageIsValid(
        EventRequestMessage* event_info,
        mongocxx::collection& activities_info_collection,
        const std::chrono::milliseconds& current_timestamp
);

