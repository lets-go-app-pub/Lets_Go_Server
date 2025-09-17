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
#include <fresh_errors_keys.h>

#include "errors_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void FreshErrorsDoc::generateRandomValues() {
    current_object_oid = bsoncxx::oid{};
    error_origin = ErrorOriginType(rand() % ErrorOriginType_MAX);
    error_urgency = ErrorUrgencyLevel(rand() % ErrorUrgencyLevel_MAX);
    version_number = rand() % 100 + 1;
    file_name = gen_random_alpha_numeric_string(rand() % 100 + 1);
    line_number = rand() % 1000 + 1;
    stack_trace = gen_random_alpha_numeric_string(rand() % 1000 + 1);
    timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{rand() % getCurrentTimestamp().count()}};

    if(error_origin == ErrorOriginType::ERROR_ORIGIN_ANDROID) {
        api_number = std::make_unique<int>(rand() % 32 + 1);
        device_name = std::make_unique<std::string>(gen_random_alpha_numeric_string(rand() % 100 + 1));
    } else {
        api_number = nullptr;
        device_name = nullptr;
    }

    error_message = gen_random_alpha_numeric_string(
            rand()
            % (server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE
              - server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE)
            + server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE
    );
}

//converts this FreshErrorsDoc object to a document and saves it to the passed builder
void FreshErrorsDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    document_result
            << fresh_errors_keys::ERROR_ORIGIN << error_origin
            << fresh_errors_keys::ERROR_URGENCY << error_urgency
            << fresh_errors_keys::VERSION_NUMBER << version_number
            << fresh_errors_keys::FILE_NAME << file_name
            << fresh_errors_keys::LINE_NUMBER << line_number
            << fresh_errors_keys::STACK_TRACE << stack_trace
            << fresh_errors_keys::TIMESTAMP_STORED << timestamp_stored;

    if(api_number != nullptr) {
        document_result
            << fresh_errors_keys::API_NUMBER << *api_number;
    }

    if(device_name != nullptr) {
        document_result
            << fresh_errors_keys::DEVICE_NAME << *device_name;
    }

    document_result
        << fresh_errors_keys::ERROR_MESSAGE << error_message;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool FreshErrorsDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection fresh_errors_collection = errors_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            fresh_errors_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = fresh_errors_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in FreshErrorsDoc::setIntoCollection\n"
                << e.what() << '\n'
                << makePrettyJson(insertDocument.view()) << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in FreshErrorsDoc::setIntoCollection\n"
                << e.what() << '\n'
                << makePrettyJson(insertDocument.view()) << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool FreshErrorsDoc::getFromCollection() {
    return getFromCollection(document{} << finalize);
}

bool FreshErrorsDoc::getFromCollection(const bsoncxx::oid& find_oid) {
    return getFromCollection(
            document{}
                    << "_id" << find_oid
            << finalize
    );
}

bool FreshErrorsDoc::getFromCollection(const bsoncxx::document::view& find_doc) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection fresh_errors_collection = errors_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = fresh_errors_collection.find_one(find_doc);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in FreshErrorsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in FreshErrorsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool FreshErrorsDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function FreshErrorsDoc::saveInfoToDocument\n";
        return false;
    }
}

bool FreshErrorsDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        error_origin = ErrorOriginType(
                extractFromBsoncxx_k_int32(
                        user_account_document,
                        fresh_errors_keys::ERROR_ORIGIN
                )
        );

        error_urgency = ErrorUrgencyLevel(
                extractFromBsoncxx_k_int32(
                        user_account_document,
                        fresh_errors_keys::ERROR_URGENCY
                )
        );

        version_number = extractFromBsoncxx_k_int32(
                user_account_document,
                fresh_errors_keys::VERSION_NUMBER
        );

        file_name = extractFromBsoncxx_k_utf8(
                user_account_document,
                fresh_errors_keys::FILE_NAME
        );

        line_number = extractFromBsoncxx_k_int32(
                user_account_document,
                fresh_errors_keys::LINE_NUMBER
        );

        stack_trace = extractFromBsoncxx_k_utf8(
                user_account_document,
                fresh_errors_keys::STACK_TRACE
        );

        timestamp_stored = extractFromBsoncxx_k_date(
                user_account_document,
                fresh_errors_keys::TIMESTAMP_STORED
        );

        auto api_number_element = user_account_document[fresh_errors_keys::API_NUMBER];

        if(api_number_element) {
            api_number = std::make_unique<int>(api_number_element.get_int32().value);
        } else {
            api_number = nullptr;
        }

        auto device_name_element = user_account_document[fresh_errors_keys::DEVICE_NAME];

        if(device_name_element) {
            device_name = std::make_unique<std::string>(device_name_element.get_string().value.to_string());
        } else {
            device_name = nullptr;
        }

        error_message = extractFromBsoncxx_k_utf8(
                user_account_document,
                fresh_errors_keys::ERROR_MESSAGE
        );

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in FreshErrorsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool FreshErrorsDoc::operator==(const FreshErrorsDoc& other) const {
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
            error_urgency,
            other.error_urgency,
            "ERROR_URGENCY",
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
            stack_trace,
            other.stack_trace,
            "STACK_TRACE",
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

    if(api_number != nullptr
        && other.api_number != nullptr) {

        checkForEquality(
                *api_number,
                *other.api_number,
                "*API_NUMBER",
                OBJECT_CLASS_NAME,
                return_value
                );

    } else {

        checkForEquality(
                api_number,
                other.api_number,
                "API_NUMBER",
                OBJECT_CLASS_NAME,
                return_value
                );
    }

    if(device_name != nullptr
        && other.device_name != nullptr) {

        checkForEquality(
                *device_name,
                *other.device_name,
                "*DEVICE_NAME",
                OBJECT_CLASS_NAME,
                return_value
                );

    } else {

        checkForEquality(
                device_name,
                other.device_name,
                "DEVICE_NAME",
                OBJECT_CLASS_NAME,
                return_value
                );
    }

    checkForEquality(
            error_message,
            other.error_message,
            "ERROR_MESSAGE",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const FreshErrorsDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
