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
#include <pending_account_keys.h>

#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this PendingAccountDoc object to a document and saves it to the passed builder
void PendingAccountDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    document_result
            << pending_account_keys::TYPE << type
            << pending_account_keys::PHONE_NUMBER << phone_number
            << pending_account_keys::INDEXING << indexing
            << pending_account_keys::ID << id
            << pending_account_keys::VERIFICATION_CODE << verification_code
            << pending_account_keys::TIME_VERIFICATION_CODE_WAS_SENT << time_verification_code_was_sent;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool PendingAccountDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection pending_account_collection = accounts_db[collection_names::PENDING_ACCOUNT_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            pending_account_collection.update_one(
                document{}
                    << "_id" << current_object_oid
                << finalize,
                document{}
                    << "$set" << insertDocument.view()
                << finalize,
                updateOptions
            );
        } else {

            auto idVar = pending_account_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in PendingAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in PendingAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool PendingAccountDoc::getFromCollection() {
    return getFromCollection(document{} << finalize);
}

bool PendingAccountDoc::getFromCollection(const bsoncxx::oid& find_oid) {
    return getFromCollection(
            document{}
                    << "_id" << find_oid
            << finalize
    );
}

bool PendingAccountDoc::getFromCollection(const std::string& _phone_number) {
    return getFromCollection(
            document{}
                    << pending_account_keys::PHONE_NUMBER << _phone_number
            << finalize
    );
}

bool PendingAccountDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection pending_account_collection = accounts_db[collection_names::PENDING_ACCOUNT_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = pending_account_collection.find_one(find_doc);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in PendingAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in PendingAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool PendingAccountDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function PendingAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool PendingAccountDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        type = AccountLoginType(
                extractFromBsoncxx_k_int32(
                        user_account_document,
                        pending_account_keys::TYPE
                )
        );

        phone_number = extractFromBsoncxx_k_utf8(
                user_account_document,
                pending_account_keys::PHONE_NUMBER
        );

        indexing = extractFromBsoncxx_k_utf8(
                user_account_document,
                pending_account_keys::INDEXING
        );

        id = extractFromBsoncxx_k_utf8(
                user_account_document,
                pending_account_keys::ID
        );

        verification_code = extractFromBsoncxx_k_utf8(
                user_account_document,
                pending_account_keys::VERIFICATION_CODE
        );

        time_verification_code_was_sent = extractFromBsoncxx_k_date(
                user_account_document,
                pending_account_keys::TIME_VERIFICATION_CODE_WAS_SENT
        );

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in PendingAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool PendingAccountDoc::operator==(const PendingAccountDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            type,
            other.type,
            "TYPE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            phone_number,
            other.phone_number,
            "PHONE_NUMBER",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            indexing,
            other.indexing,
            "INDEXING",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            id,
            other.id,
            "ID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            verification_code,
            other.verification_code,
            "VERIFICATION_CODE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            time_verification_code_was_sent.value.count(),
            other.time_verification_code_was_sent.value.count(),
            "TIME_VERIFICATION_CODE_WAS_SENT",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const PendingAccountDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
