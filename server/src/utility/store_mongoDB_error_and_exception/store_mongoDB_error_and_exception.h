//
// Created by jeremiah on 3/18/21.
//
#pragma once

#include <fstream>
#include <optional>

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/view_or_value.hpp>
#include <bsoncxx/json.hpp>

#include <boost/stacktrace.hpp>

#include <ErrorOriginEnum.grpc.pb.h>

#include <mongocxx/collection.hpp>
#include <connection_pool_global_variable.h>
#include <sstream>
#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "fresh_errors_keys.h"
#include "handled_errors_list_keys.h"
#include "version_number.h"

//appends a document of type string
[[maybe_unused]] inline void
appendErrorBasicMongoDbDocument(std::stringstream& docInsertDocument,
                                const std::string& docKey, const std::string& docObject) {

    //if it is a valid type, append it to the document
    if (!docKey.empty()) {
        docInsertDocument
            << docKey << ": " << docObject << '\n';
    }
}

//appends a document of type document::view
[[maybe_unused]] inline void
appendErrorBasicMongoDbDocument(std::stringstream& docInsertDocument,
                                const std::string& docKey, const bsoncxx::v_noabi::document::view& docObject) {

    if (!docKey.empty()) {
        docInsertDocument
                << docKey << '\n' << makePrettyJson(docObject) << '\n';
    }
}

//appends a document of type oid
[[maybe_unused]] inline void
appendErrorBasicMongoDbDocument(std::stringstream& docInsertDocument,
                                const std::string& docKey, const bsoncxx::oid& docObject) {

    //if it is a valid type, append it to the document
    if (!docKey.empty()) {
        docInsertDocument
            << docKey << ": " << docObject.to_string() << '\n';
    }
}

//appends a document of type long long
[[maybe_unused]] inline void
appendErrorBasicMongoDbDocument(std::stringstream& docInsertDocument,
                                const std::string& docKey, const long long& docObject) {

    //if it is a valid type, append it to the document
    if (!docKey.empty()) {
        docInsertDocument
            << docKey << ": " << docObject << '\n';
    }
}

//appends a document of type int
[[maybe_unused]] inline void
appendErrorBasicMongoDbDocument(std::stringstream& docInsertDocument,
                                                             const std::string& docKey, int docObject) {

    //if it is a valid type, append it to the document
    if (!docKey.empty()) {
        docInsertDocument
            << docKey << ": " << docObject << '\n';
    }
}

//appends a document of type unsigned int
[[maybe_unused]] inline void
appendErrorBasicMongoDbDocument(std::stringstream& docInsertDocument,
                                            const std::string& docKey, unsigned int docObject) {

    //if it is a valid type, append it to the document
    if (!docKey.empty()) {
        docInsertDocument
                << docKey << ": " << docObject << '\n';
    }
}

void logErrorToFile(const std::string& error_message);

//primary error logging function, stores errors along with up to 5 other optional fields
//SUPPORTED TYPES
//long long, int, std::string, bsoncxx::oid ,bsoncxx::v_noabi::document::view
template<typename T = char, typename U = char, typename V = char, typename W = char, typename X = char>
void storeMongoDBErrorAndException(
        const int& line_number, const std::string& file_name,
        const std::optional<std::string>& exception_message, const std::string& error_message,
        const std::string& first_key = "", const T first = 0,
        const std::string& second_key = "", const U second = 0,
        const std::string& third_key = "", const V third = 0,
        const std::string& fourth_key = "", const W fourth = 0,
        const std::string& fifth_key = "", const X fifth = 0
) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database error_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection error_collection = error_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];
    mongocxx::collection handled_errors_list_collection = error_db[collection_names::HANDLED_ERRORS_LIST_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> find_errors_list;
    try {
        mongocxx::options::find opts;

        opts.projection(
                bsoncxx::builder::stream::document{}
                    << "_id" << 1
                << bsoncxx::builder::stream::finalize
                );

        find_errors_list = handled_errors_list_collection.find_one(bsoncxx::builder::stream::document{}
                    << handled_errors_list_keys::ERROR_ORIGIN << ErrorOriginType::ERROR_ORIGIN_SERVER
                    << handled_errors_list_keys::VERSION_NUMBER << (int) version_number::SERVER_CURRENT_VERSION_NUMBER
                    << handled_errors_list_keys::FILE_NAME << file_name
                    << handled_errors_list_keys::LINE_NUMBER << line_number
                << bsoncxx::builder::stream::finalize,
                opts);
    }
    //It is important here that mongocxx::exception is caught and NOT just logic_error. This is because this
    // function is run inside the catch block in many cases (handleFunctionOperationException() is an example).
    catch (const mongocxx::exception& e) {
        std::stringstream error_string;

        error_string
                << "line: " << __LINE__
                << "file: " << __FILE__
                << "exception message: " << e.what()
                << "ERROR:\n" << "Error accessing HANDLED_ERRORS_LIST_COLLECTION_NAME to find if error was handled.\n";

#ifndef _RELEASE
        std::cout << error_string.str();
#endif

        logErrorToFile(error_string.str());
    }

    if(find_errors_list) { //Error has been set to handled
        return;
    }

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    auto insertDocument = bsoncxx::builder::basic::document{};

    insertDocument.append(
            bsoncxx::builder::basic::kvp(fresh_errors_keys::ERROR_ORIGIN, ErrorOriginType::ERROR_ORIGIN_SERVER));

    insertDocument.append(bsoncxx::builder::basic::kvp(fresh_errors_keys::ERROR_URGENCY, ErrorUrgencyLevel::ERROR_URGENCY_LEVEL_UNKNOWN));

    insertDocument.append(bsoncxx::builder::basic::kvp(fresh_errors_keys::VERSION_NUMBER, (int) version_number::SERVER_CURRENT_VERSION_NUMBER));

    insertDocument.append(bsoncxx::builder::basic::kvp(fresh_errors_keys::FILE_NAME, file_name));

    insertDocument.append(bsoncxx::builder::basic::kvp(fresh_errors_keys::LINE_NUMBER, line_number));

    insertDocument.append(bsoncxx::builder::basic::kvp(fresh_errors_keys::STACK_TRACE, ""));
    //NOTE: the stack trace extraction is VERY slow, sometimes taking a full minute for 1 trace, so ignoring it for now
    //insertDocument.append(bsoncxx::builder::basic::kvp(STACK_TRACE, to_string(boost::stacktrace::basic_stacktrace())));

    insertDocument.append(
            bsoncxx::builder::basic::kvp(fresh_errors_keys::TIMESTAMP_STORED, bsoncxx::types::b_date{currentTimestamp}));

    std::stringstream final_error_message;

    appendErrorBasicMongoDbDocument(final_error_message, first_key, first);
    appendErrorBasicMongoDbDocument(final_error_message, second_key, second);
    appendErrorBasicMongoDbDocument(final_error_message, third_key, third);
    appendErrorBasicMongoDbDocument(final_error_message, fourth_key, fourth);
    appendErrorBasicMongoDbDocument(final_error_message, fifth_key, fifth);

    if (exception_message) {
        final_error_message
                << "Exception Message: " << exception_message.value() << '\n';
    }

    final_error_message
            << "ERROR:\n" << error_message;

    insertDocument.append(bsoncxx::builder::basic::kvp(fresh_errors_keys::ERROR_MESSAGE, final_error_message.str()));

#ifndef _RELEASE
    std::cout << "Printing Error:\n" << makePrettyJson(insertDocument.view());
#endif

    try {
        error_collection.insert_one(insertDocument.view());
    }
    //It is important here that mongocxx::exception is caught and NOT just logic_error. This is because this
    // function is run inside the catch block in many cases (handleFunctionOperationException() is an example).
    catch (const mongocxx::exception& e) {
        insertDocument.append(bsoncxx::builder::basic::kvp("ERROR_Exception:", std::string(e.what())));

#ifndef _RELEASE
        std::cout << "\nERROR Exception: " << std::string(e.what());
#endif
    }

    logErrorToFile(makePrettyJson(insertDocument) + "\n");
}