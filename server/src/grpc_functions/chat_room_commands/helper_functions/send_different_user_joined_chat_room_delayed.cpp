//
// Created by jeremiah on 7/8/21.
//

#include <mongocxx/client.hpp>
#include <handle_function_operation_exception.h>
#include <store_mongoDB_error_and_exception.h>
#include <global_bsoncxx_docs.h>

#include "chat_room_commands_helper_functions.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"

void sendDifferentUserJoinedChatRoomImplementation(
        const std::string& chatRoomId,
        const bsoncxx::oid& userAccountOID,
        const bsoncxx::builder::stream::document& different_user_joined_chat_room_message_doc,
        const std::string& message_uuid
);

//store a kDifferentUserJoinedChatRoom message inside the given chat room database with a short delay
void sendDifferentUserJoinedChatRoom(
        const std::string& chatRoomId,
        const bsoncxx::oid& userAccountOID,
        const bsoncxx::builder::stream::document& different_user_joined_chat_room_message_doc,
        const std::string& message_uuid
) {
    handleFunctionOperationException(
        [&] {
            sendDifferentUserJoinedChatRoomImplementation(
                    chatRoomId,
                    userAccountOID,
                    different_user_joined_chat_room_message_doc,
                    message_uuid
            );
        },
        [] {},
        [&] {},
        __LINE__,
        __FILE__
    );
}

void sendDifferentUserJoinedChatRoomImplementation(
        const std::string& chatRoomId,
        const bsoncxx::oid& userAccountOID,
        const bsoncxx::builder::stream::document& different_user_joined_chat_room_message_doc,
        const std::string& message_uuid
       ) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chatRoomId];

    const mongocxx::pipeline pipeline = buildPipelineForInsertingDocument(different_user_joined_chat_room_message_doc.view());

    mongocxx::stdx::optional<mongocxx::result::update> update_result;
    std::optional<std::string> exception_string;
    try {
        mongocxx::options::update opts;
        opts.upsert(true);

        update_result = chat_room_collection.update_one(
                bsoncxx::builder::stream::document{}
                    << "_id" << message_uuid
                << bsoncxx::builder::stream::finalize,
                pipeline,
                opts
            );
    }
    catch (const mongocxx::logic_error& e) {
        exception_string = e.what();
    }

    if(!update_result || update_result->result().upserted_count() < 1) {
        const std::string error_string = "Failed to insert kDifferentUserJoined message.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                exception_string, error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", chat_room_collection.name().to_string(),
                "ObjectID_used", userAccountOID,
                "pipeline", makePrettyJson(pipeline.view_array())
        );

        return;
    }

}