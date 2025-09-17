//
// Created by jeremiah on 6/3/22.
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
#include <icons_info_keys.h>

#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this IconsInfoDoc object to a document and saves it to the passed builder
void IconsInfoDoc::convertToDocument(
        bsoncxx::builder::stream::document& document_result,
        bool skip_long_strings
) const {

    document_result
            << icons_info_keys::INDEX << index
            << icons_info_keys::TIMESTAMP_LAST_UPDATED << timestamp_last_updated;

    if (skip_long_strings) {
        document_result
                << icons_info_keys::ICON_IN_BYTES << "icon_in_bytes skipped";
    } else {
        document_result
                << icons_info_keys::ICON_IN_BYTES << icon_in_bytes;
    }

    document_result
            << icons_info_keys::ICON_SIZE_IN_BYTES << icon_size_in_bytes
            << icons_info_keys::ICON_ACTIVE << icon_active;

}

bool IconsInfoDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection icons_info_collection = accounts_db[collection_names::ICONS_INFO_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument, false);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            icons_info_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = icons_info_collection.insert_one(insertDocument.view());

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

bool IconsInfoDoc::getFromCollection(long passed_index) {
    return getFromCollection(
        document{}
            << "_id" << bsoncxx::types::b_int64{passed_index}
        << finalize
    );
}

bool IconsInfoDoc::getFromCollection(const bsoncxx::oid& find_oid) {
    return getFromCollection(
        document{}
            << "_id" << find_oid
        << finalize
    );
}

bool IconsInfoDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection icons_info_collection = accounts_db[collection_names::ICONS_INFO_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = icons_info_collection.find_one(find_doc);
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

bool IconsInfoDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool IconsInfoDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {

        index = extractFromBsoncxx_k_int64(
                user_account_document,
                icons_info_keys::INDEX
        );

        timestamp_last_updated = extractFromBsoncxx_k_date(
                user_account_document,
                icons_info_keys::TIMESTAMP_LAST_UPDATED
        );

        icon_in_bytes = extractFromBsoncxx_k_utf8(
                user_account_document,
                icons_info_keys::ICON_IN_BYTES
        );

        icon_size_in_bytes = extractFromBsoncxx_k_int64(
                user_account_document,
                icons_info_keys::ICON_SIZE_IN_BYTES
        );

        icon_active = extractFromBsoncxx_k_bool(
                user_account_document,
                icons_info_keys::ICON_ACTIVE
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

bool IconsInfoDoc::operator==(const IconsInfoDoc& other) const {
    bool return_value = true;

    checkForEquality(
            index,
            other.index,
            "INDEX",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            timestamp_last_updated.value.count(),
            other.timestamp_last_updated.value.count(),
            "TIMESTAMP_LAST_UPDATED",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            icon_in_bytes,
            other.icon_in_bytes,
            "ICON_IN_BYTES",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            icon_size_in_bytes,
            other.icon_size_in_bytes,
            "ICON_SIZE_IN_BYTES",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            icon_active,
            other.icon_active,
            "ICON_ACTIVE",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const IconsInfoDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result, true);
    o << makePrettyJson(document_result.view());
    return o;
}
