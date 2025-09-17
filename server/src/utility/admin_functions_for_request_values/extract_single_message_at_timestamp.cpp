//
// Created by jeremiah on 9/23/21.
//

#include "admin_functions_for_request_values.h"

#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>


#include "store_mongoDB_error_and_exception.h"
#include "utility_chat_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_room_shared_keys.h"
#include "general_values.h"
#include "chat_room_message_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool extractSingleMessageAtTimeStamp(mongocxx::client& mongo_cpp_client, mongocxx::database& accounts_db,
                                     mongocxx::collection& user_accounts_collection,
                                     mongocxx::collection& chat_room_collection, const std::string& chat_room_id,
                                     const std::string& message_uuid,
                                     const std::chrono::milliseconds& timestamp_of_message,
                                     ChatMessageToClient* responseMsg,
                                     const std::function<void(const std::string& error_str)>& error_func) {

    bsoncxx::stdx::optional<bsoncxx::document::value> message_doc;
    try {
        message_doc = chat_room_collection.find_one(
            document{}
                << "_id" << message_uuid
            << finalize
        );
    }
    catch (const mongocxx::logic_error& e) {
        std::optional<std::string> dummy_exception_string = e.what();
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, dummy_exception_string,
                std::string(e.what()),
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                "message uuid", message_uuid);

        error_func("Exception when requesting message from database.");
        return false;
    }

    if (!message_doc) {
        error_func("Requested message was not found in database");
        return false;
    }

    bsoncxx::document::view message_doc_view = message_doc->view();

    ExtractUserInfoObjects extractUserInfoObjects(
            mongo_cpp_client, accounts_db, user_accounts_collection
            );

    if (!convertChatMessageDocumentToChatMessageToClient(
            message_doc_view,
            chat_room_id,
            general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT,
            false,
            responseMsg,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            &extractUserInfoObjects
    )
            ) {
        responseMsg->Clear();
        error_func("Error stored on server when saving message to response.");
        return false;
    }

    //get the message before any edits took place
    if (responseMsg->message().message_specifics().message_body_case() == MessageSpecifics::kTextMessage
        && responseMsg->message().message_specifics().text_message().is_edited()) {

        //find the first edited message AFTER the passed message timestamp (the previous message will be saved inside it)
        mongocxx::stdx::optional<mongocxx::cursor> edited_message_doc;
        try {

            auto message_time = bsoncxx::types::b_date{timestamp_of_message};

            mongocxx::options::find opts;
            opts.sort(
                document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                << finalize
            );

            opts.limit(1);

            edited_message_doc = chat_room_collection.find(
                document{}
                    << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::kEditedMessage
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID << message_uuid
                    << chat_room_shared_keys::TIMESTAMP_CREATED << open_document
                        << "$gt" << message_time
                    << close_document
                << finalize
            );
        }
        catch (const mongocxx::logic_error& e) {
            std::optional<std::string> dummy_exception_string = e.what();
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, dummy_exception_string,
                    std::string(e.what()),
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                    "message uuid", message_uuid);

            error_func("Exception when requesting message from database.");
            return false;
        }

        if (!edited_message_doc) {
            error_func("Requested message edits were not found in database.");
            return false;
        }

        bsoncxx::document::view edited_message_view;

        //should only be 1 element at MOST (limit 1 is used in query)
        for (const auto& doc : *edited_message_doc) {
            edited_message_view = doc;
        }

        //if edited_message_view is Empty that is perfectly acceptable, it could simply
        // mean the message was edited BEFORE the message_time
        if (!edited_message_view.empty()) {
            bsoncxx::document::view specifics_document;

            auto specifics_document_element = edited_message_view[chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT];
            if (specifics_document_element
                && specifics_document_element.type() == bsoncxx::type::k_document) { //if element exists and is type document
                specifics_document = specifics_document_element.get_document().value;
            } else { //if element does not exist or is not type int32
                logElementError(__LINE__, __FILE__, specifics_document_element,
                                edited_message_view, bsoncxx::type::k_document,
                                chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id);

                error_func("Error stored on server.");
                return false;
            }

            auto previous_message_element = specifics_document[chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT];
            if (previous_message_element
                && previous_message_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                responseMsg->mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
                        previous_message_element.get_string().value.to_string()
                        );
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, previous_message_element,
                                specifics_document, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id);

                error_func("Error stored on server.");
                return false;
            }

        }

    }

    return true;
}