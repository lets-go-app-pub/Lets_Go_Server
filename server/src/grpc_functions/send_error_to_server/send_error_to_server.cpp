//proto file is SendErrorToServer.proto

#include "mongocxx/client.hpp"
#include "mongocxx/uri.hpp"

#include <bsoncxx/builder/stream/document.hpp>
#include <handle_function_operation_exception.h>

#include "send_error_to_server.h"

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "fresh_errors_keys.h"
#include "handled_errors_list_keys.h"
#include "server_parameter_restrictions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void sendErrorToServerMongoDbImplementation(
        const send_error_to_server::SendErrorRequest* request,
        send_error_to_server::SendErrorResponse* response
);

//primary function for SendErrorToServer, called from gRPC server implementation
void sendErrorToServerMongoDb(
        const send_error_to_server::SendErrorRequest* request,
        send_error_to_server::SendErrorResponse* response
) {
    handleFunctionOperationException(
            [&] {
                sendErrorToServerMongoDbImplementation(request, response);
            },
            [&] {
                response->set_return_status(send_error_to_server::SendErrorResponse_Status_DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
            },
            __LINE__, __FILE__, request);
}

void sendErrorToServerMongoDbImplementation(
        const send_error_to_server::SendErrorRequest* request,
        send_error_to_server::SendErrorResponse* response
) {

    if (!request->has_message()) {
        response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
        return;
    }

    if (isInvalidLetsGoAndroidVersion(
            request->message().version_number())) { //check if meets minimum version requirement
        response->set_return_status(send_error_to_server::SendErrorResponse_Status_OUTDATED_VERSION);
        return;
    }

    if (request->message().error_origin() != ErrorOriginType::ERROR_ORIGIN_ANDROID) {
        response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
        return;
    }

    if (!ErrorUrgencyLevel_IsValid(request->message().error_urgency_level())) {
        response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
        return;
    }

    if (server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES < request->message().file_name().size()) {
        response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
        return;
    }

    if (server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE < request->message().stack_trace().length()) {
        response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
        return;
    }

    if (server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES < request->message().device_name().size()) {
        response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
        return;
    }

    if (request->message().error_message().length() < server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE ||
        server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE <
        request->message().error_message().length()) { //check if reasonable char size
        response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
        return;
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

#ifndef _RELEASE
    std::cout
            << "error_urgency_level: " << ErrorUrgencyLevel_Name(request->message().error_urgency_level()) << '\n'
            << "version_number: " << request->message().version_number() << '\n'
            << "file_name: " << request->message().file_name() << '\n'
            << "line_number: " << request->message().line_number() << '\n'
            << "stack_trace: " << request->message().stack_trace() << '\n'
            << "api_number: " << request->message().api_number() << '\n'
            << "device_name: " << request->message().device_name() << '\n'
            << request->message().error_message() << '\n';
#endif // DEBUG

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database error_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection fresh_errors_collection = error_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];
    mongocxx::collection handled_errors_list_collection = error_db[collection_names::HANDLED_ERRORS_LIST_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> find_errors_list;
    try {
        mongocxx::options::find opts;

        opts.projection(
                document{}
                        << "_id" << 1
                << finalize
        );

        find_errors_list = handled_errors_list_collection.find_one(
                document{}
                    << handled_errors_list_keys::ERROR_ORIGIN << (int) request->message().error_origin()
                    << handled_errors_list_keys::VERSION_NUMBER << (int) request->message().version_number()
                    << handled_errors_list_keys::FILE_NAME << request->message().file_name()
                    << handled_errors_list_keys::LINE_NUMBER << (int) request->message().line_number()
                << finalize,
                opts
        );
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "error_message", request->message().error_message()
        );

        response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
    }

    if (find_errors_list) { //Error has been set to handled
        response->set_return_status(send_error_to_server::SendErrorResponse_Status_SUCCESSFUL);
        return;
    }

    try {

        fresh_errors_collection.insert_one(
            document{}
                << fresh_errors_keys::ERROR_ORIGIN << request->message().error_origin()
                << fresh_errors_keys::ERROR_URGENCY << request->message().error_urgency_level()
                << fresh_errors_keys::VERSION_NUMBER << (int) request->message().version_number()
                << fresh_errors_keys::FILE_NAME << request->message().file_name()
                << fresh_errors_keys::LINE_NUMBER << (int) request->message().line_number()
                << fresh_errors_keys::STACK_TRACE << request->message().stack_trace()
                << fresh_errors_keys::TIMESTAMP_STORED << bsoncxx::types::b_date{current_timestamp}
                << fresh_errors_keys::API_NUMBER << (int) request->message().api_number()
                << fresh_errors_keys::DEVICE_NAME << request->message().device_name()
                << fresh_errors_keys::ERROR_MESSAGE << request->message().error_message()
            << finalize
        );

        response->set_return_status(send_error_to_server::SendErrorResponse_Status_SUCCESSFUL);
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "error_message", request->message().error_message()
        );

        response->set_return_status(send_error_to_server::SendErrorResponse_Status_FAIL);
    }

}