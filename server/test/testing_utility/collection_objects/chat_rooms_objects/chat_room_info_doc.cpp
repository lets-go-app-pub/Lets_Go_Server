//
// Created by jeremiah on 6/1/22.
//

#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <user_pictures_keys.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>
#include <chat_room_info_keys.h>

#include "chat_rooms_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this ChatMessagePictureDoc object to a document and saves it to the passed builder
void ChatRoomInfoDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    //This does not use current_object_oid, it has a fixed _id of chat_room_info_keys::ID.
    document_result
        << "_id" << chat_room_info_keys::ID
        << chat_room_info_keys::PREVIOUSLY_USED_CHAT_ROOM_NUMBER << previously_used_chat_room_number;
}

bool ChatRoomInfoDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_message_info_collection = chat_rooms_db[collection_names::CHAT_ROOM_INFO];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        mongocxx::options::update updateOptions;
        updateOptions.upsert(true);

        chat_message_info_collection.update_one(
                document{}
                << "_id" << chat_room_info_keys::ID
                << finalize,
                document{}
                << "$set" << insertDocument.view()
                << finalize,
                updateOptions);

    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
        << "bsoncxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
        << e.what() << '\n'
        << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
        << "mongocxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
        << e.what() << '\n'
        << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool ChatRoomInfoDoc::getFromCollection(const bsoncxx::oid&) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_message_info_collection = chat_rooms_db[collection_names::CHAT_ROOM_INFO];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = chat_message_info_collection.find_one(document{}
            << "_id" << chat_room_info_keys::ID
        << finalize);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
        << "bsoncxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
        << e.what() << '\n'
        << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
        << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
        << e.what() << '\n'
        << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool ChatRoomInfoDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool ChatRoomInfoDoc::convertDocumentToClass(const bsoncxx::document::view& user_account_document) {
    try {
        previously_used_chat_room_number = extractFromBsoncxx_k_int64(
                user_account_document,
                chat_room_info_keys::PREVIOUSLY_USED_CHAT_ROOM_NUMBER
                );
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
        << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
        << e.what() << '\n'
        << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool ChatRoomInfoDoc::operator==(const ChatRoomInfoDoc&) const {
    bool return_value = true;

    checkForEquality(
            previously_used_chat_room_number,
            previously_used_chat_room_number,
            "PREVIOUSLY_USED_CHAT_ROOM_NUMBER",
            OBJECT_CLASS_NAME,
            return_value
            );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const ChatRoomInfoDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}