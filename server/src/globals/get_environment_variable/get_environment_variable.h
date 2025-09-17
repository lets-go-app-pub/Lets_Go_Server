//
// Created by jeremiah on 1/14/23.
//

#pragma once

#include <string>

inline const std::string ENVIRONMENT_VARIABLE_FAILED = "ENVIRONMENT_VARIABLE_FAILED";

inline std::string get_environment_variable(const char* name) {
    return std::string(getenv(name) == nullptr ? ENVIRONMENT_VARIABLE_FAILED : getenv(name));
}