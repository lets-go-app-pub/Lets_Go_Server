//
// Created by jeremiah on 3/20/21.
//

#include <session_to_run_functions.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_testing_functions.h>
#include <global_bsoncxx_docs.h>
#include "chat_room_commands_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "mongocxx/exception/operation_exception.hpp"
#include "database_names.h"
#include "chat_room_shared_keys.h"
#include "chat_room_message_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//upsert the document as a message into the chat room, this will add the mandatory fields to the document builder
SendMessageToChatRoomReturn handleMessageToBeSent(
        mongocxx::client_session* const session,
        mongocxx::collection& chatRoomCollection,
        const grpc_chat_commands::ClientMessageToServerRequest* request,
        bsoncxx::builder::stream::document& insertDocBuilder,
        const bsoncxx::oid& user_account_oid
) {
    int random_int_with_this_doc = generateRandomInt();

    insertDocBuilder
            << "_id" << request->message_uuid();

#ifndef LG_TESTING
    insertDocBuilder
            << chat_room_shared_keys::TIMESTAMP_CREATED << "$$NOW";
#else
    if(testing_delay_for_messages == std::chrono::milliseconds{-1}) {
        insertDocBuilder
            << chat_room_shared_keys::TIMESTAMP_CREATED << "$$NOW";
    }
    else {
        //Simulate gaps between the messages being stored.
        const std::chrono::milliseconds current_timestamp = generateTestingTimestamp();

        insertDocBuilder
                << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{current_timestamp};
    }
#endif

    //NOTE: This is not the only place that messages are finalized to be sent. Can look at
    // buildPipelineForInsertingDocument() (used below) and follow it backwards to see other
    // locations.
    insertDocBuilder
            << chat_room_message_keys::MESSAGE_SENT_BY << user_account_oid
            << chat_room_message_keys::MESSAGE_TYPE << bsoncxx::types::b_int32{(int) request->message().message_specifics().message_body_case()}
            << chat_room_message_keys::RANDOM_INT << bsoncxx::types::b_int32{random_int_with_this_doc};

    std::optional<std::string> exceptionString;
    bsoncxx::stdx::optional<bsoncxx::document::value> inserted_document_timestamp;
    try {

        mongocxx::options::find_one_and_update opts;
        opts.upsert(true);
        opts.return_document(mongocxx::options::return_document::k_after);
        opts.projection(
            document{}
                << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                << chat_room_message_keys::MESSAGE_TYPE << 1
                << chat_room_message_keys::RANDOM_INT << 1
                << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::PICTURE_OID << 1
            << finalize
        );

        mongocxx::pipeline pipeline = buildPipelineForInsertingDocument(insertDocBuilder.view());

        //upsert the document if it does not already exist
        inserted_document_timestamp = find_one_and_update_optional_session(
                session,
                chatRoomCollection,
                    document{}
                        << "_id" << request->message_uuid()
                    << finalize,
                pipeline,
                opts
        );

    } catch (const mongocxx::logic_error& e) {
        exceptionString = std::string(e.what());
    }

    if (inserted_document_timestamp) { //successfully inserted document

        bsoncxx::document::view returned_document = inserted_document_timestamp->view();

        MessageSpecifics::MessageBodyCase message_type;
        std::chrono::milliseconds time_stored_on_server;
        std::string final_picture_oid;
        int extracted_random_int;
        try {

            extracted_random_int = extractFromBsoncxx_k_int32(
                    returned_document,
                    chat_room_message_keys::RANDOM_INT
                    );

            message_type = MessageSpecifics::MessageBodyCase(
                    extractFromBsoncxx_k_int32(
                            returned_document,
                            chat_room_message_keys::MESSAGE_TYPE
                            )
                            );

            time_stored_on_server = extractFromBsoncxx_k_date(
                    returned_document,
                    chat_room_shared_keys::TIMESTAMP_CREATED
                    ).value;

            if(message_type != request->message().message_specifics().message_body_case()) {
                std::string errorString = "Duplicate UUID was found inside database, however messages were different types. This"
                                          "is unusual because this should only happen when a bug on Android double sends a specific"
                                          "message\n";

                storeMongoDBErrorAndException(__LINE__,
                                              __FILE__, exceptionString, errorString,
                                              "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                              "collection", chatRoomCollection.name().to_string(),
                                              "ObjectID_used", user_account_oid.to_string(),
                                              "message_type",
                                              convertMessageBodyTypeToString(
                                                      request->message().message_specifics().message_body_case()));

                //NOTE: This is not a corrupt document, it means a duplicate UUID was used for a different message type.

                return SendMessageToChatRoomReturn{};
            }

            if(request->message().message_specifics().message_body_case() == MessageSpecifics::kPictureMessage) {
                bsoncxx::document::view message_specifics_doc = extractFromBsoncxx_k_document(
                        returned_document,
                        chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT
                        );

                final_picture_oid = extractFromBsoncxx_k_utf8(
                        message_specifics_doc,
                        chat_room_message_keys::message_specifics::PICTURE_OID
                        );
            }

        }
        catch (const ErrorExtractingFromBsoncxx& e) {

            //NOTE: Error already stored here.
            /*std::string errorString = "Error occurred when extracting value from bsoncxx.\n" +
                    std::string(e.what());
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    exceptionString, errorString,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chatRoomCollection.name().to_string(),
                    "ObjectID_used", user_account_oid.to_string(),
                    "message_type", convertMessageBodyTypeToString(request->message().message_specifics().message_body_case())
            );*/

            //delete message, it is corrupt
            removeMessageOnError(
                request->message_uuid(),
                chatRoomCollection,
                session,
                user_account_oid
            );

            return SendMessageToChatRoomReturn{};
        }

        if(extracted_random_int != random_int_with_this_doc) { //this means a different document already existed with this uuid
            return SendMessageToChatRoomReturn{
                SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED,
                final_picture_oid,
                time_stored_on_server
            };
        } else {
            return SendMessageToChatRoomReturn{
                SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL,
                final_picture_oid,
                time_stored_on_server
            };
        }

    }
    else { //failed to upsert document
        std::string errorString = "Failed to upsert document to chat room.\n";

        storeMongoDBErrorAndException(__LINE__, __FILE__, exceptionString,
                                      errorString,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", chatRoomCollection.name().to_string(),
                                      "ObjectID_used", user_account_oid);

        return SendMessageToChatRoomReturn{};
    }

}

