//
// Created by jeremiah on 8/9/22.
//

#include "chat_room_commands_helper_functions.h"

#include <optional>

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include "collection_names.h"
#include "user_account_keys.h"
#include "database_names.h"
#include "store_mongoDB_error_and_exception.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool updateUserLastViewedTime(
    const std::string& chat_room_id,
    const bsoncxx::oid& user_account_oid,
    const bsoncxx::types::b_date& mongodb_current_date,
    mongocxx::collection& user_accounts_collection,
    mongocxx::client_session* session
) {

    std::optional<std::string> update_document_exception_string;

    if(session == nullptr) {
        std::string errorString = "nullptr was passed for a session when this should never be possible.\n";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                update_document_exception_string, errorString,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "Object_oid_used", user_account_oid.to_string(),
                "chat_room_id", chat_room_id
        );

        return false;
    }

    //NOTE: Updating the last time viewed for user account.
    bsoncxx::stdx::optional<mongocxx::result::update> update_user_account;
    try {

        const std::string ELEM = "e";
        const std::string ELEM_UPDATE = user_account_keys::CHAT_ROOMS + ".$[" + ELEM + "].";

        mongocxx::options::update update_user_opts;

        bsoncxx::builder::basic::array arrayBuilder{};
        arrayBuilder.append(
                document{}
                        << ELEM + "." + user_account_keys::chat_rooms::CHAT_ROOM_ID << bsoncxx::types::b_string{chat_room_id}
                        << finalize
        );

        update_user_opts.array_filters(arrayBuilder.view());

        update_user_account = user_accounts_collection.update_one(
                *session,
                document{}
                    << "_id" << user_account_oid
                << finalize,
                document{}
                    << "$max" << open_document
                    << ELEM_UPDATE + user_account_keys::chat_rooms::LAST_TIME_VIEWED << mongodb_current_date
                    << close_document
                << finalize,
                update_user_opts
        );
    }
    catch (const mongocxx::logic_error& e) {
        update_document_exception_string = std::string(e.what());
    }

    if (!update_user_account || update_user_account->matched_count() == 0) {
        std::string errorString = "Updating user account last_time_viewed failed.\n";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                update_document_exception_string, errorString,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "Object_oid_used", user_account_oid.to_string(),
                "chat_room_id", chat_room_id
        );

        return false;
    }

    return true;
}