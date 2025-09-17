//
// Created by jeremiah on 11/16/21.
//

#include "build_debug_string_response.h"
#include "server_parameter_restrictions.h"

std::string buildDebugStringResponse(const ::google::protobuf::Message* debug_response) {
    if(debug_response != nullptr) {
        const unsigned long debug_response_size = debug_response->ByteSizeLong();

        return debug_response_size < server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE ?
        (debug_response_size == 0 ? "Response debug string is empty.": debug_response->DebugString()) :
        "Response debug string too long.";
    } else {
        return "Response pointer not set.";
    }
}