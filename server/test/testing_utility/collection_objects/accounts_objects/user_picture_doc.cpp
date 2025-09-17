//
// Created by jeremiah on 5/30/22.
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

#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this UserPictureDoc object to a document and saves it to the passed builder
void UserPictureDoc::convertToDocument(
        bsoncxx::v_noabi::builder::stream::document& document_result,
        bool skip_long_strings
) const {

    document_result
            << user_pictures_keys::USER_ACCOUNT_REFERENCE << user_account_reference;

    bsoncxx::builder::basic::array thumbnail_references_doc;

    for (const std::string& thumbnail_reference : thumbnail_references) {
        thumbnail_references_doc.append(thumbnail_reference);
    }

    document_result
            << user_pictures_keys::THUMBNAIL_REFERENCES << thumbnail_references_doc
            << user_pictures_keys::TIMESTAMP_STORED << timestamp_stored
            << user_pictures_keys::PICTURE_INDEX << picture_index;

    if (skip_long_strings) {
        document_result
                << user_pictures_keys::THUMBNAIL_IN_BYTES << "thumbnail_in_bytes was skipped"
                << user_pictures_keys::PICTURE_IN_BYTES << "picture_in_bytes was skipped";
    } else {
        document_result
                << user_pictures_keys::THUMBNAIL_IN_BYTES << thumbnail_in_bytes
                << user_pictures_keys::PICTURE_IN_BYTES << picture_in_bytes;
    }

    document_result
            << user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES << thumbnail_size_in_bytes
            << user_pictures_keys::PICTURE_SIZE_IN_BYTES << picture_size_in_bytes;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }

}

bool UserPictureDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument, false);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            user_pictures_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = user_pictures_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserPictureDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserPictureDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

void UserPictureDoc::getFromEventsPictureMessage(
        const EventsPictureMessage& events_picture_message,
        bsoncxx::oid _picture_reference,
        bsoncxx::types::b_date _timestamp_stored,
        bsoncxx::oid _event_oid,
        const int _index
        ) {
    current_object_oid = _picture_reference;
    user_account_reference = _event_oid;
    thumbnail_references.clear();
    timestamp_stored = _timestamp_stored;
    picture_index = _index;
    thumbnail_in_bytes = events_picture_message.thumbnail_in_bytes();
    thumbnail_size_in_bytes = events_picture_message.thumbnail_size();
    picture_in_bytes = events_picture_message.file_in_bytes();
    picture_size_in_bytes = events_picture_message.file_size();
}

bool UserPictureDoc::getFromCollection() {
    return getFromCollection(document{} << finalize);

}

bool UserPictureDoc::getFromCollection(const bsoncxx::oid& find_oid) {
    return getFromCollection(
            document{}
                << "_id" << find_oid
            << finalize
    );
}

bool UserPictureDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = user_pictures_collection.find_one(find_doc);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserPictureDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserPictureDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool UserPictureDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserPictureDoc::saveInfoToDocument\n";
        return false;
    }
}

bool UserPictureDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        user_account_reference = extractFromBsoncxx_k_oid(
                user_account_document,
                user_pictures_keys::USER_ACCOUNT_REFERENCE);

        bsoncxx::array::view thumbnail_references_arr = extractFromBsoncxx_k_array(
                user_account_document,
                user_pictures_keys::THUMBNAIL_REFERENCES);

        thumbnail_references.clear();
        for (const auto& doc : thumbnail_references_arr) {
            thumbnail_references.emplace_back(doc.get_string().value.to_string());
        }

        timestamp_stored = extractFromBsoncxx_k_date(
                user_account_document,
                user_pictures_keys::TIMESTAMP_STORED);

        picture_index = extractFromBsoncxx_k_int32(
                user_account_document,
                user_pictures_keys::PICTURE_INDEX);

        thumbnail_in_bytes = extractFromBsoncxx_k_utf8(
                user_account_document,
                user_pictures_keys::THUMBNAIL_IN_BYTES);

        thumbnail_size_in_bytes = extractFromBsoncxx_k_int32(
                user_account_document,
                user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES);

        picture_in_bytes = extractFromBsoncxx_k_utf8(
                user_account_document,
                user_pictures_keys::PICTURE_IN_BYTES);

        picture_size_in_bytes = extractFromBsoncxx_k_int32(
                user_account_document,
                user_pictures_keys::PICTURE_SIZE_IN_BYTES);

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserPictureDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool UserPictureDoc::operator==(const UserPictureDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            user_account_reference.to_string(),
            other.user_account_reference.to_string(),
            "USER_ACCOUNT_REFERENCE",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (other.thumbnail_references.size() == thumbnail_references.size()) {
        for (int i = 0; i < (int)thumbnail_references.size(); i++) {
            checkForEquality(
                    thumbnail_references[i],
                    other.thumbnail_references[i],
                    "THUMBNAIL_REFERENCES",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                thumbnail_references.size(),
                other.thumbnail_references.size(),
                "THUMBNAIL_REFERENCES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            timestamp_stored.value.count(),
            other.timestamp_stored.value.count(),
            "TIMESTAMP_STORED",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            picture_index,
            other.picture_index,
            "PICTURE_INDEX",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            thumbnail_in_bytes,
            other.thumbnail_in_bytes,
            "THUMBNAIL_IN_BYTES",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            thumbnail_size_in_bytes,
            other.thumbnail_size_in_bytes,
            "THUMBNAIL_SIZE_IN_BYTES",
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

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const UserPictureDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result, true);
    o << makePrettyJson(document_result.view());
    return o;
}
