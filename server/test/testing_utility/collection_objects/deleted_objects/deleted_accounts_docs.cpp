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
#include <user_account_statistics_documents_completed_keys.h>
#include <deleted_user_pictures_keys.h>
#include <deleted_accounts_keys.h>

#include "deleted_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this DeletedAccountsDoc object to a document and saves it to the passed builder
void DeletedAccountsDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {
    UserAccountDoc::convertToDocument(document_result);

    document_result
        << deleted_accounts_keys::TIMESTAMP_REMOVED << timestamp_removed;

}

bool DeletedAccountsDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
    mongocxx::collection deleted_accounts_collection = deleted_db[collection_names::DELETED_DELETED_ACCOUNTS_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            deleted_accounts_collection.update_one(
                    document{}
                    << "_id" << current_object_oid
                    << finalize,
                    document{}
                    << "$set" << insertDocument.view()
                    << finalize,
                    updateOptions);
        } else {

            auto idVar = deleted_accounts_collection.insert_one(insertDocument.view());

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

bool DeletedAccountsDoc::getFromCollection(const bsoncxx::oid& findOID) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
    mongocxx::collection deleted_accounts_collection = deleted_db[collection_names::DELETED_DELETED_ACCOUNTS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = deleted_accounts_collection.find_one(document{}
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

bool
DeletedAccountsDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}


bool DeletedAccountsDoc::convertDocumentToClass(const bsoncxx::document::view& user_account_document) {

    UserAccountDoc::convertDocumentToClass(user_account_document);

    try {
        timestamp_removed = user_account_document[deleted_accounts_keys::TIMESTAMP_REMOVED].get_date();
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
        << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
        << e.what() << '\n'
        << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
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

bool DeletedAccountsDoc::operator==(const DeletedAccountsDoc& other) const {
    bool return_value = UserAccountDoc::operator==(other);

    checkForEquality(
            timestamp_removed,
            other.timestamp_removed,
            "TIMESTAMP_REMOVED",
            OBJECT_CLASS_NAME,
            return_value
            );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const DeletedAccountsDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
