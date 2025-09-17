//
// Created by jeremiah on 9/9/22.
//

#pragma once

#include <optional>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include <ReportMessages.grpc.pb.h>

#include "user_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

enum BlockReportFieldLocationsCalledFrom {
    BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER,
    BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER,
    USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER,
    USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER,
};

//Will build the necessary document for updating the relevant account for No, Block and Report response types.
//The document is meant to be passed to an aggregation pipeline (probably as add_fields or replace_root).
//If this is called for the user that is taking the action (doing the blocking for example) it will be added
// to the OTHER_USERS_BLOCKED array.
template <BlockReportFieldLocationsCalledFrom location_called_from>
void buildAddFieldsDocForBlockReportPipeline(
        const std::chrono::milliseconds& current_timestamp,
        bsoncxx::builder::stream::document& update_document,
        const ResponseType& response_type,
        const bsoncxx::oid& blocked_user_account_oid
) {

    if((location_called_from == BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER
        || location_called_from == USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER)
       && (response_type == ResponseType::USER_MATCH_OPTION_REPORT
            || response_type == ResponseType::USER_MATCH_OPTION_BLOCK)
            ) { //if updating current user AND responseType == (report || block)

        //add blocked user to OTHER_USERS_BLOCKED
        update_document
            << user_account_keys::OTHER_USERS_BLOCKED << open_document
                << "$concatArrays" << open_array
                    << "$" + user_account_keys::OTHER_USERS_BLOCKED << open_array
                        << open_document
                            << user_account_keys::other_users_blocked::OID_STRING << blocked_user_account_oid.to_string()
                            << user_account_keys::other_users_blocked::TIMESTAMP_BLOCKED << bsoncxx::types::b_date{current_timestamp}
                        << close_document
                    << close_array
                << close_array
            << close_document;
    }

    std::string no_response_string;
    std::string block_response_string;
    std::string report_response_string;

    switch (location_called_from) {
        case BlockReportFieldLocationsCalledFrom::BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER: {
            no_response_string = user_account_keys::NUMBER_TIMES_SWIPED_NO;
            block_response_string = user_account_keys::NUMBER_TIMES_THIS_USER_BLOCKED_FROM_CHAT_ROOM;
            report_response_string = user_account_keys::NUMBER_TIMES_THIS_USER_REPORTED_FROM_CHAT_ROOM;
            break;
        }
        case BlockReportFieldLocationsCalledFrom::BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER: {
            no_response_string = user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO;
            block_response_string = user_account_keys::NUMBER_TIMES_BLOCKED_BY_OTHERS_IN_CHAT_ROOM;
            report_response_string = user_account_keys::NUMBER_TIMES_REPORTED_BY_OTHERS_IN_CHAT_ROOM;
            break;
        }
        case BlockReportFieldLocationsCalledFrom::USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER: {
            no_response_string = user_account_keys::NUMBER_TIMES_SWIPED_NO;
            block_response_string = user_account_keys::NUMBER_TIMES_SWIPED_BLOCK;
            report_response_string = user_account_keys::NUMBER_TIMES_SWIPED_REPORT;
            break;
        }
        case BlockReportFieldLocationsCalledFrom::USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER: {
            no_response_string = user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO;
            block_response_string = user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_BLOCK;
            report_response_string = user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_REPORT;
            break;
        }
        default:
            return;
    }

    switch(response_type) {
        case USER_MATCH_OPTION_NO:
            //update number times swiped no if relevant
            update_document
                << no_response_string << open_document
                    << "$add" << open_array
                        << "$" + no_response_string << bsoncxx::types::b_int32{1}
                    << close_array
                << close_document;
            break;
        case USER_MATCH_OPTION_BLOCK:
            //update number times swiped block if relevant
            update_document
                << block_response_string << open_document
                    << "$add" << open_array
                        << "$" + block_response_string << bsoncxx::types::b_int32{1}
                    << close_array
                << close_document;
            break;
        case USER_MATCH_OPTION_REPORT:
            //update number times swiped report if relevant
            update_document
                << report_response_string << open_document
                    << "$add" << open_array
                        << "$" + report_response_string << bsoncxx::types::b_int32{1}
                    << close_array
                << close_document;
            break;
        case USER_MATCH_OPTION_CONNECTION_ERR:
        case USER_MATCH_OPTION_YES:
        case ResponseType_INT_MIN_SENTINEL_DO_NOT_USE_:
        case ResponseType_INT_MAX_SENTINEL_DO_NOT_USE_:
            break;
    }
}