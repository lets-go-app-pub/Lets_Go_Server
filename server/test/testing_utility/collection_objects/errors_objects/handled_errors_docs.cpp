//
// Created by jeremiah on 6/4/22.
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
#include <handled_errors_keys.h>

#include "errors_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this HandledErrorsDoc object to a document and saves it to the passed builder
void HandledErrorsDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {
    FreshErrorsDoc::convertToDocument(document_result);

    document_result
            << handled_errors_keys::ADMIN_NAME << admin_name
            << handled_errors_keys::TIMESTAMP_HANDLED << timestamp_handled
            << handled_errors_keys::ERROR_HANDLED_MOVE_REASON << error_handled_move_reason
            << handled_errors_keys::ERRORS_DESCRIPTION << errors_description;
}

bool HandledErrorsDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection handle_errors_collection = errors_db[collection_names::HANDLED_ERRORS_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            handle_errors_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = handle_errors_collection.insert_one(insertDocument.view());

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

bool HandledErrorsDoc::getFromCollection(const bsoncxx::oid& findOID) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection handle_errors_collection = errors_db[collection_names::HANDLED_ERRORS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = handle_errors_collection.find_one(document{}
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
HandledErrorsDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool HandledErrorsDoc::convertDocumentToClass(const bsoncxx::document::view& user_account_document) {

    FreshErrorsDoc::convertDocumentToClass(user_account_document);

    try {

        admin_name = extractFromBsoncxx_k_utf8(
                user_account_document,
                handled_errors_keys::ADMIN_NAME
        );

        timestamp_handled = extractFromBsoncxx_k_date(
                user_account_document,
                handled_errors_keys::TIMESTAMP_HANDLED
        );

        error_handled_move_reason = ErrorHandledMoveReason(
                extractFromBsoncxx_k_int32(
                        user_account_document,
                        handled_errors_keys::ERROR_HANDLED_MOVE_REASON
                )
        );

        errors_description = extractFromBsoncxx_k_utf8(
                user_account_document,
                handled_errors_keys::ERRORS_DESCRIPTION
        );

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

bool HandledErrorsDoc::operator==(const HandledErrorsDoc& other) const {
    bool return_value = FreshErrorsDoc::operator==(other);

    checkForEquality(
            admin_name,
            other.admin_name,
            "ADMIN_NAME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            timestamp_handled.value.count(),
            other.timestamp_handled.value.count(),
            "TIMESTAMP_HANDLED",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            error_handled_move_reason,
            other.error_handled_move_reason,
            "ERROR_HANDLED_MOVE_REASON",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            errors_description,
            other.errors_description,
            "ERRORS_DESCRIPTION",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const HandledErrorsDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
