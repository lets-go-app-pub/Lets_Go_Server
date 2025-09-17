//
// Created by jeremiah on 4/15/21.
//

#include <functional>

#include <mongocxx/exception/operation_exception.hpp>
#include <build_debug_string_response.h>

#include "store_mongoDB_error_and_exception.h"
#include "handle_function_operation_exception.h"

void handleFunctionOperationException(const std::function<void()>& runFunction,
                                      const std::function<void()>& setDatabaseDown,
                                      const std::function<void()>& setError,
                                      const int line_number,
                                      const std::string& file_name,
                                      const ::google::protobuf::Message* debug_response
) {

    try {
        runFunction();
    } catch (const mongocxx::operation_exception& e) {

        const std::string error_string =
                "mongocxx::operation_exception occurred.\n" + buildDebugStringResponse(debug_response);

        std::string raw_server_error = "raw_server_error Does not exist";

        if (e.raw_server_error()) { //raw_server_error exists
            raw_server_error = makePrettyJson(e.raw_server_error()->view());
        }

        std::optional<std::string> dummy_exception_string = e.what();
        storeMongoDBErrorAndException(line_number, file_name,
                                      dummy_exception_string, error_string,
                                      "raw_server_error", raw_server_error,
                                      "error_code_value", std::to_string(e.code().value()));

        setDatabaseDown();
    } catch (const std::exception& e) {
        const std::string& error_string = "std::exception occurred.\n" + buildDebugStringResponse(debug_response);

        std::optional<std::string> exceptionString = e.what();
        storeMongoDBErrorAndException(line_number, file_name, exceptionString, error_string);

        setError();
    }
}