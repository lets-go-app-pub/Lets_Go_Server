//
// Created by jeremiah on 9/23/21.
//

#pragma once

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include <ChatMessageToClientMessage.grpc.pb.h>
#include <RequestUserAccountInfo.grpc.pb.h>

bool extractSingleMessageAtTimeStamp(mongocxx::client& mongo_cpp_client, mongocxx::database& accounts_db,
                                     mongocxx::collection& user_accounts_collection,
                                     mongocxx::collection& chat_room_collection, const std::string& chat_room_id,
                                     const std::string& message_uuid,
                                     const std::chrono::milliseconds& timestamp_of_message,
                                     ChatMessageToClient* responseMsg,
                                     const std::function<void(const std::string& error_str)>& error_func);

bool extractUserInfo(mongocxx::client& mongo_cpp_client,
                     mongocxx::database& accounts_db,
                     mongocxx::database& deleted_db,
                     mongocxx::collection& user_accounts_collection,
                     mongocxx::collection& deleted_user_accounts_collection,
                     bool requesting_by_user_account_oid,
                     const bsoncxx::document::view& query_doc,
                     CompleteUserAccountInfo* user_account_info,
                     const std::function<void(const std::string& error_string)>& error_fxn);