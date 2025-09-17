//
// Created by jeremiah on 4/15/21.
//
#pragma once

#include <google/protobuf/message.h>

void handleFunctionOperationException(
        const std::function<void()>& runFunction,
        const std::function<void()>& setDatabaseDown,
        const std::function<void()>& setError,
        int line_number,
        const std::string& file_name,
        const ::google::protobuf::Message* debug_response = nullptr
);
