//
// Created by jeremiah on 10/29/21.
//

#include "handle_errors.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "fresh_errors_keys.h"
#include "error_values.h"

#include <StatusEnum.pb.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include <handle_function_operation_exception.h>
#include <utility_general_functions.h>
#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>
#include <connection_pool_global_variable.h>
#include <store_mongoDB_error_and_exception.h>
#include <extract_data_from_bsoncxx.h>
#include <global_bsoncxx_docs.h>
#include <helper_functions/handle_errors_helper_functions.h>


//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void extractErrorsImplementation(
        const std::function<void(const std::string& /*key*/, const std::string& /*value*/)>& send_trailing_meta_data,
        const ::handle_errors::ExtractErrorsRequest* request,
        grpc::ServerWriterInterface<::handle_errors::ExtractErrorsResponse>* writer
);

void extractErrors(
        const std::function<void(const std::string& /*key*/, const std::string& /*value*/)>& send_trailing_meta_data,
        const handle_errors::ExtractErrorsRequest* request,
        grpc::ServerWriterInterface<::handle_errors::ExtractErrorsResponse>* writer
) {

    handleFunctionOperationException(
            [&] {
                extractErrorsImplementation(send_trailing_meta_data, request, writer);
            },
            [&] {
                handle_errors::ExtractErrorsResponse response;
                response.set_success(false);
                response.set_error_msg(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
                writer->Write(response);
            },
            [&] {
                handle_errors::ExtractErrorsResponse response;
                response.set_success(false);
                response.set_error_msg(ReturnStatus_Name(ReturnStatus::LG_ERROR));
                writer->Write(response);
            },
            __LINE__, __FILE__, request);

}

void extractErrorsImplementation(
        const std::function<void(const std::string& /*key*/, const std::string& /*value*/)>& send_trailing_meta_data,
        const handle_errors::ExtractErrorsRequest* request,
        grpc::ServerWriterInterface<::handle_errors::ExtractErrorsResponse>* writer
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    auto store_error_message = [&](
            bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
            const std::string& passed_error_message
    ) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id,
            store_error_message
    );

    auto return_error_to_client = [&](const std::string& error_message) {
        handle_errors::ExtractErrorsResponse response;
        response.set_success(false);
        response.set_error_msg(error_message);
        writer->Write(response);
    };

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        return_error_to_client("ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
        return;
    } else if (!admin_info_doc_value) {
        return_error_to_client("Could not find admin document.");
        return;
    }

    const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;

    auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
    if (admin_privilege_element
        && admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(
                __LINE__, __FILE__,
                admin_privilege_element, admin_info_doc_view,
                bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );

        return_error_to_client("Error stored on server.");
        return;
    }

    if (!admin_privileges[admin_level].request_error_messages()) {
        return_error_to_client("Admin level " + AdminLevelEnum_Name(admin_level) +
                               " does not have 'request_error_messages' access.");
        return;
    }

    if (!errorParametersGood(request->error_parameters(), return_error_to_client)) {
        //error was already stored
        return;
    }

    if (request->max_number_bytes() < 0
        || error_values::MAXIMUM_NUMBER_ALLOWED_BYTES_TO_REQUEST_ERROR_MESSAGE < request->max_number_bytes()) {
        return_error_to_client(std::string("Invalid number of max bytes requested in a single call."));
        return;
    }

    //NOTE: This will attempt to only send back a maximum size of documents. This is in case a very large error message
    // is stored (say a byte array for a picture is accidentally stored, multiplied by hundreds of errors).

    //Queries
    //1) extract total number of documents that take up the maximum size the user requested
    //2) extract only the total number of documents

    //Results from testing.
    //base documents with just _id: ObjectId(...) is 22 bytes
    //documents with just _id: ObjectId(...) and a key of 1 char for an empty array is 30 bytes (combined)
    //each int32 added to the document seems to increase the # of bytes by 7
    //max document size is 16,777,216 bytes
    //so (16777216 - 30)/7 = 2,396,740 int32 values can be stored
    //NOTE: the values will always be int32 because the max document size is 16777216 bytes which is always smaller than max int32 size
    //because I have no guarantee that this is general info across platforms I am going to increase all the numbers
    //so (16777216 - 100)/11(11 is size of int64) = 1,525,192 documents (variable EXTRACT_ERRORS_MAX_SEARCH_RESULTS)

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection fresh_errors_collection = errors_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    static const std::string PROJECTED_EXTRACT_INTERMEDIATE_KEY = "iT";

    static const std::string PROJECTED_TOTAL_COUNT_KEY = "t";
    static const std::string PROJECTED_EXTRACTED_COUNT_KEY = "s";

    static const std::string PROJECTED_TOTAL_BYTES = "tB";
    static const std::string PROJECTED_EXTRACTED_BYTES = "eB";

    mongocxx::pipeline pipeline;

    pipeline.match(
            document{}
                    << fresh_errors_keys::ERROR_ORIGIN << request->error_parameters().error_origin()
                    << fresh_errors_keys::VERSION_NUMBER << (int) request->error_parameters().version_number()
                    << fresh_errors_keys::FILE_NAME << request->error_parameters().file_name()
                    << fresh_errors_keys::LINE_NUMBER << (int) request->error_parameters().line_number()
            << finalize
    );

    //Sort by newest timestamps first.
    pipeline.sort(
            document{}
                    << fresh_errors_keys::TIMESTAMP_STORED << -1
            << finalize
    );

    pipeline.limit(error_values::EXTRACT_ERRORS_MAX_SEARCH_RESULTS);

    //Group all documents into one.
    pipeline.project(
            document{}
                    << PROJECTED_EXTRACT_INTERMEDIATE_KEY << open_document
                            << "$bsonSize" << "$$ROOT"
                    << close_document
            << finalize
    );

    //$group always honors the order the documents are arriving into the $group
    // stage, so because $sort was run and then $group $push then the order very
    // much matters and will make a difference to each document within $group.
    //What is never guaranteed (at least at this point) is that after the group
    // stage the entire stream of documents would be ordered by "something".
    pipeline.group(
            document{}
                    << "_id" << bsoncxx::types::b_null{}
                    << PROJECTED_EXTRACT_INTERMEDIATE_KEY << open_document
                            << "$push" << "$" + PROJECTED_EXTRACT_INTERMEDIATE_KEY
                    << close_document
           << finalize
    );

    pipeline.project(
            buildProjectExtractedErrorDocumentStatistics(
                    PROJECTED_EXTRACT_INTERMEDIATE_KEY,
                    PROJECTED_TOTAL_BYTES,
                    PROJECTED_EXTRACTED_BYTES,
                    PROJECTED_TOTAL_COUNT_KEY,
                    PROJECTED_EXTRACTED_COUNT_KEY,
                    request->max_number_bytes()
            )
    );

    pipeline.replace_root(
            document{}
                    << "newRoot" << "$" + PROJECTED_EXTRACT_INTERMEDIATE_KEY
            << finalize
    );

    mongocxx::stdx::optional<mongocxx::cursor> extract_num_error_docs_cursor;
    try {

        mongocxx::options::aggregate aggregation_options;

        //Pipeline stages have a limit of 100 megabytes of RAM. If a stage exceeds this limit, MongoDB will produce an
        // error. To allow for the handling of large datasets, use the allowDiskUse option to enable aggregation
        // pipeline stages to write data to temporary files.
        //The aggregation_options should not be used unless necessary, so it doesn't hurt to have it on. Otherwise,
        // could randomly get errors.
        aggregation_options.allow_disk_use(true);

        extract_num_error_docs_cursor = fresh_errors_collection.aggregate(
                pipeline,
                aggregation_options
        );

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::FRESH_ERRORS_COLLECTION_NAME,
                "Document_passed", pipeline.view_array()
        );

        return_error_to_client("Error exception thrown when extracting error documents.");
        return;
    }

    if (!extract_num_error_docs_cursor) {
        const std::string error_string = "MongoDB cursor was not set after running an 'aggregate' query.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::FRESH_ERRORS_COLLECTION_NAME,
                "Document_passed", pipeline.view_array()
        );

        return_error_to_client("Error cursor was not set after aggregating from error documents.");
        return;
    }

    long bytes_total = 0;
    long bytes_extracted = 0;
    int total_num_docs = 0;
    int num_docs_to_extract = 0;

    //There should only be 1 document in this cursor.
    for (const bsoncxx::document::view& doc_view : extract_num_error_docs_cursor.value()) {

        try {

            bytes_total = extractFromBsoncxx_k_int64(
                    doc_view,
                    PROJECTED_TOTAL_BYTES);

            bytes_extracted = extractFromBsoncxx_k_int64(
                    doc_view,
                    PROJECTED_EXTRACTED_BYTES);

            total_num_docs = extractFromBsoncxx_k_int32(
                    doc_view,
                    PROJECTED_TOTAL_COUNT_KEY);

            num_docs_to_extract = extractFromBsoncxx_k_int32(
                    doc_view,
                    PROJECTED_EXTRACTED_COUNT_KEY);

        } catch (const ErrorExtractingFromBsoncxx& e) {
            return_error_to_client(e.what());
            return;
        }
    }

    if (num_docs_to_extract == 0) {
        return_error_to_client("No errors matching criteria for errors exist inside 'Fresh Errors' collection.");
        return;
    }

    mongocxx::stdx::optional<mongocxx::cursor> extract_requested_error_docs;
    try {

        mongocxx::options::find find_opts;

        //sort by newest timestamps first
        find_opts.sort(
                document{}
                        << fresh_errors_keys::TIMESTAMP_STORED << -1
                << finalize
        );

        find_opts.limit(num_docs_to_extract);

        find_opts.projection(
                document{}
                         << fresh_errors_keys::ERROR_ORIGIN << 0
                         << fresh_errors_keys::ERROR_URGENCY << 0
                         << fresh_errors_keys::VERSION_NUMBER << 0
                         << fresh_errors_keys::FILE_NAME << 0
                         << fresh_errors_keys::LINE_NUMBER << 0
                << finalize
        );

        extract_requested_error_docs = fresh_errors_collection.find(
                document{}
                        << fresh_errors_keys::ERROR_ORIGIN << request->error_parameters().error_origin()
                        << fresh_errors_keys::VERSION_NUMBER << (int) request->error_parameters().version_number()
                        << fresh_errors_keys::FILE_NAME << request->error_parameters().file_name()
                        << fresh_errors_keys::LINE_NUMBER << (int) request->error_parameters().line_number()
                << finalize,
                find_opts
        );

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::FRESH_ERRORS_COLLECTION_NAME,
                "Document_passed", pipeline.view_array()
        );

        return_error_to_client("Error exception thrown when extracting error documents.");
        return;
    }

    if (!extract_requested_error_docs) {
        const std::string error_string = "MongoDB cursor was not set after running 'find' query.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::FRESH_ERRORS_COLLECTION_NAME,
                "Document_passed", pipeline.view_array()
        );

        return_error_to_client(error_string);
        return;
    }

    //re-using objects is supposed to be cheaper in gRPC
    handle_errors::ExtractErrorsResponse response;

    for (const bsoncxx::document::view& doc_view : extract_requested_error_docs.value()) {
        response.Clear();
        auto* response_error_message = response.mutable_error_message();
        response.set_success(true);

        try {

            response_error_message->set_error_id(
                    extractFromBsoncxx_k_oid(
                            doc_view,
                            "_id").to_string()
            );

            response_error_message->set_stack_trace(
                    extractFromBsoncxx_k_utf8(
                            doc_view,
                            fresh_errors_keys::STACK_TRACE)
            );

            response_error_message->set_timestamp_stored(
                    extractFromBsoncxx_k_date(
                            doc_view,
                            fresh_errors_keys::TIMESTAMP_STORED).value.count()
            );

            //This element could NOT exist.
            auto api_number_element = doc_view[fresh_errors_keys::API_NUMBER];
            if (api_number_element &&
                api_number_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                response_error_message->set_api_number(api_number_element.get_int32().value);
            } else if (api_number_element) { //if element exists but is not type int32
                logElementError(__LINE__, __FILE__, api_number_element,
                                doc_view, bsoncxx::type::k_int32, fresh_errors_keys::API_NUMBER,
                                database_names::ERRORS_DATABASE_NAME, collection_names::FRESH_ERRORS_COLLECTION_NAME);

                throw ErrorExtractingFromBsoncxx(
                        "Error requesting user info value " + fresh_errors_keys::API_NUMBER + ".");
            }

            //This element could NOT exist.
            auto device_name_element = doc_view[fresh_errors_keys::DEVICE_NAME];
            if (device_name_element &&
                device_name_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                response_error_message->set_device_name(device_name_element.get_string().value.to_string());
            } else if (device_name_element) { //if element exists but is not type utf8
                logElementError(__LINE__, __FILE__, device_name_element,
                                doc_view, bsoncxx::type::k_utf8, fresh_errors_keys::DEVICE_NAME,
                                database_names::ERRORS_DATABASE_NAME, collection_names::FRESH_ERRORS_COLLECTION_NAME);

                throw ErrorExtractingFromBsoncxx(
                        "Error requesting user info value " + fresh_errors_keys::DEVICE_NAME + ".");
            }

            response_error_message->set_error_message(
                    extractFromBsoncxx_k_utf8(
                            doc_view,
                            fresh_errors_keys::ERROR_MESSAGE)
            );

            writer->Write(response);
        } catch (const ErrorExtractingFromBsoncxx& e) {

            //NOTE: error already stored
            //NOTE: ok to continue here
            continue;
        }
    }

    send_trailing_meta_data(error_values::EXTRACT_ERRORS_TOTAL_BYTES_KEY, std::to_string(bytes_total));
    send_trailing_meta_data(error_values::EXTRACT_ERRORS_EXTRACTED_BYTES_KEY, std::to_string(bytes_extracted));
    send_trailing_meta_data(error_values::EXTRACT_ERRORS_TOTAL_DOCS_KEY, std::to_string(total_num_docs));
    send_trailing_meta_data(error_values::EXTRACT_ERRORS_EXTRACTED_DOCS_KEY, std::to_string(num_docs_to_extract));
}