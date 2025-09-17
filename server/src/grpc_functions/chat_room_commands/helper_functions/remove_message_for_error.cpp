//
// Created by jeremiah on 8/4/22.
//

#include <optional>
#include <mongocxx/exception/logic_error.hpp>

#include "chat_room_commands_helper_functions.h"
#include "session_to_run_functions.h"
#include "database_names.h"
#include "store_mongoDB_error_and_exception.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool removeMessageOnError(
        const std::string& message_uuid,
        mongocxx::collection& chatRoomCollection,
        mongocxx::client_session* session,
        const bsoncxx::oid& user_account_oid
) {
    std::optional<std::string> exception_string;
    bsoncxx::stdx::optional<mongocxx::result::delete_result> delete_doc_result;
    try {

        //upsert the document if it does not already exist
        delete_doc_result = delete_one_optional_session(
                session,
                chatRoomCollection,
                document{}
                        << "_id" << message_uuid
                        << finalize
        );

    } catch (const mongocxx::logic_error& e) {
        exception_string = std::string(e.what());
    }

    if(!delete_doc_result
       || delete_doc_result->deleted_count() == 0) {

        std::string errorString = "Failed to delete document after error occurred with insertion.\n";

        storeMongoDBErrorAndException(__LINE__, __FILE__, exception_string,
                                      errorString,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", chatRoomCollection.name().to_string(),
                                      "ObjectID_used", user_account_oid.to_string());

        return false;
    }

    return true;
}