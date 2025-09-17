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
#include <handled_errors_list_keys.h>

#include "errors_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this HandledErrorsListDoc object to a document and saves it to the passed builder
void HandledErrorsListDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    document_result
            << handled_errors_list_keys::ERROR_ORIGIN << error_origin
            << handled_errors_list_keys::VERSION_NUMBER << version_number
            << handled_errors_list_keys::FILE_NAME << file_name
            << handled_errors_list_keys::LINE_NUMBER << line_number
            << handled_errors_list_keys::ERROR_HANDLED_MOVE_REASON << error_handled_move_reason;

    if(description != nullptr) {
        document_result
            << handled_errors_list_keys::DESCRIPTION << *description;
    }

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool HandledErrorsListDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection handled_errors_list_collection = errors_db[collection_names::HANDLED_ERRORS_LIST_COLLECTION_NAME];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            handled_errors_list_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = handled_errors_list_collection.insert_one(insertDocument.view());

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

bool HandledErrorsListDoc::getFromCollection() {
    return getFromCollection(document{} << finalize);
}

bool HandledErrorsListDoc::getFromCollection(const bsoncxx::oid& find_oid) {
    return getFromCollection(
            document{}
                << "_id" << find_oid
            << finalize
    );
}

bool HandledErrorsListDoc::getFromCollection(const bsoncxx::document::view& doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection handled_errors_list_collection = errors_db[collection_names::HANDLED_ERRORS_LIST_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = handled_errors_list_collection.find_one(doc);
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
HandledErrorsListDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool HandledErrorsListDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        error_origin = ErrorOriginType(
                extractFromBsoncxx_k_int32(
                        user_account_document,
                        handled_errors_list_keys::ERROR_ORIGIN
                )
        );

        version_number = extractFromBsoncxx_k_int32(
                user_account_document,
                handled_errors_list_keys::VERSION_NUMBER
        );

        file_name = extractFromBsoncxx_k_utf8(
                user_account_document,
                handled_errors_list_keys::FILE_NAME
        );

        line_number = extractFromBsoncxx_k_int32(
                user_account_document,
                handled_errors_list_keys::LINE_NUMBER
        );

        error_handled_move_reason = ErrorHandledMoveReason(
                extractFromBsoncxx_k_int32(
                        user_account_document,
                        handled_errors_list_keys::ERROR_HANDLED_MOVE_REASON
                )
        );

        auto description_element = user_account_document[handled_errors_list_keys::DESCRIPTION];

        if(description_element) {
            description = std::make_unique<std::string>(description_element.get_string().value.to_string());
        } else {
            description = nullptr;
        }

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

bool HandledErrorsListDoc::operator==(const HandledErrorsListDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            error_origin,
            other.error_origin,
            "ERROR_ORIGIN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            version_number,
            other.version_number,
            "VERSION_NUMBER",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            file_name,
            other.file_name,
            "FILE_NAME",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            line_number,
            other.line_number,
            "LINE_NUMBER",
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

    if(description != nullptr
        && other.description != nullptr) {
        checkForEquality(
                *description,
                *other.description,
                "*DESCRIPTION",
                OBJECT_CLASS_NAME,
                return_value
                );
    } else {
        checkForEquality(
                description,
                other.description,
                "DESCRIPTION",
                OBJECT_CLASS_NAME,
                return_value
                );
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const HandledErrorsListDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
