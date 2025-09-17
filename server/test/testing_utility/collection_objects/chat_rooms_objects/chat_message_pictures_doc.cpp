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
#include <chat_message_pictures_keys.h>

#include "chat_rooms_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this ChatMessagePictureDoc object to a document and saves it to the passed builder
void ChatMessagePictureDoc::convertToDocument(
        bsoncxx::builder::stream::document& document_result,
        bool skip_long_strings
        ) const {

    document_result
            << chat_message_pictures_keys::CHAT_ROOM_ID << chat_room_id;

    if(skip_long_strings) {
        document_result
            << chat_message_pictures_keys::PICTURE_IN_BYTES << "picture_in_bytes skipped";
    } else {
        document_result
            << chat_message_pictures_keys::PICTURE_IN_BYTES << picture_in_bytes;
    }

    document_result
            << chat_message_pictures_keys::PICTURE_SIZE_IN_BYTES << picture_size_in_bytes
            << chat_message_pictures_keys::HEIGHT << height
            << chat_message_pictures_keys::WIDTH << width
            << chat_message_pictures_keys::TIMESTAMP_STORED << timestamp_stored;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool ChatMessagePictureDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_message_pictures_collection = chat_rooms_db[collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument, false);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            chat_message_pictures_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = chat_message_pictures_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
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

bool ChatMessagePictureDoc::getFromCollection(const bsoncxx::oid& findOID) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_message_pictures_collection = chat_rooms_db[collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = chat_message_pictures_collection.find_one(document{}
                                                                            << "_id" << findOID
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

bool ChatMessagePictureDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool ChatMessagePictureDoc::convertDocumentToClass(const bsoncxx::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        chat_room_id = extractFromBsoncxx_k_utf8(
                user_account_document,
                chat_message_pictures_keys::CHAT_ROOM_ID);

        picture_in_bytes = extractFromBsoncxx_k_utf8(
                user_account_document,
                chat_message_pictures_keys::PICTURE_IN_BYTES);

        picture_size_in_bytes = extractFromBsoncxx_k_int32(
                user_account_document,
                chat_message_pictures_keys::PICTURE_SIZE_IN_BYTES);

        height = extractFromBsoncxx_k_int32(
                user_account_document,
                chat_message_pictures_keys::HEIGHT);

        width = extractFromBsoncxx_k_int32(
                user_account_document,
                chat_message_pictures_keys::WIDTH);

        timestamp_stored = extractFromBsoncxx_k_date(
                user_account_document,
                chat_message_pictures_keys::TIMESTAMP_STORED);
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

bool ChatMessagePictureDoc::operator==(const ChatMessagePictureDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            chat_room_id,
            other.chat_room_id,
            "CHAT_ROOM_ID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            picture_in_bytes,
            other.picture_in_bytes,
            "PICTURE_IN_BYTES",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            picture_size_in_bytes,
            other.picture_size_in_bytes,
            "PICTURE_SIZE_IN_BYTES",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            height,
            other.height,
            "HEIGHT",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            width,
            other.width,
            "WIDTH",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            timestamp_stored.value.count(),
            other.timestamp_stored.value.count(),
            "TIMESTAMP_STORED",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const ChatMessagePictureDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result, true);
    o << makePrettyJson(document_result.view());
    return o;
}
