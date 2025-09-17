//
// Created by jeremiah on 11/4/21.
//

#include "handle_errors_helper_functions.h"
#include "server_parameter_restrictions.h"

bool errorParametersGood(
        const handle_errors::ErrorParameters& error_parameters,
        const std::function<void(const std::string& /*error_message*/)>& store_error_message
) {
    if (!ErrorOriginType_IsValid(error_parameters.error_origin())) {
        store_error_message(std::string("Invalid ErrorOriginType of value ").append(
                std::to_string(error_parameters.error_origin())).append("'."));
        return false;
    }

    if ((int) error_parameters.file_name().size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES) {
        store_error_message(std::string("File name length is too long."));
        return false;
    }

    return true;
}