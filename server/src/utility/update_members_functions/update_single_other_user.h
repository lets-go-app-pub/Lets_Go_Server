//
// Created by jeremiah on 3/9/22.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include <UpdateOtherUserMessages.grpc.pb.h>

#include "how_to_handle_member_pictures.h"
#include "user_account_keys.h"

#pragma once

//update a single other user
// when other user is a chat room member, member is expected to be IN_CHAT_ROOM or CHAT_ROOM_ADMIN, passed parameters are in
// memberFromClient, responseFunction will be called when user data has been compiled
void updateSingleOtherUser(
        mongocxx::client& passed_thumbnail,
        mongocxx::database& thumbnail_size,
        const bsoncxx::document::view& thumbnail_timestamp,
        mongocxx::collection& user_accounts_collection,
        const std::string& chat_room_collection_name,
        const std::string& user_oid_string,
        const OtherUserInfoForUpdates& user_info_from_client,
        const std::chrono::milliseconds& current_timestamp,
        bool always_set_response_message,
        AccountStateInChatRoom account_state_from_chat_room,
        const std::chrono::milliseconds& user_last_activity_time,
        HowToHandleMemberPictures how_to_handle_member_pictures,
        UpdateOtherUserResponse* response,
        const std::function<void()>& response_function
);

//builds a projection document with the fields expected by the updateSingleOtherUser() function
bsoncxx::document::value buildUpdateSingleOtherUserProjectionDoc();

//builds a document to update LAST_TIME_DISPLAYED_INFO_UPDATED
// set aggregation to true if used for aggregation pipeline
// set aggregation to false if used for an update
template <bool aggregation>
inline bsoncxx::document::value buildUpdateSingleOtherUserProjectionDoc(
        const bsoncxx::types::b_date& mongodb_current_date
) {

    //mongoDB
    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;
    using bsoncxx::builder::stream::open_array;
    using bsoncxx::builder::stream::open_document;

    if(aggregation) {
        return bsoncxx::builder::stream::document{}
                << "$cond" << open_document
                    << "if" << open_document
                        << "$gt" << open_array
                            << mongodb_current_date
                            << "$" + user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED
                        << close_array
                    << close_document
                    << "then" << mongodb_current_date
                    << "else" << "$" + user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED
                << close_document
            << finalize;
    } else {
        return bsoncxx::builder::stream::document{}
            << user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED << mongodb_current_date
        << finalize;
    }
}