//
// Created by jeremiah on 11/7/21.
//

#pragma once

#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

void storeInfoToUserStatistics(
        mongocxx::client& callback_session,
        mongocxx::database& accounts_db,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::document::view& push_update_doc,
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::client_session* session = nullptr
        );