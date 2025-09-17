//
// Created by jeremiah on 5/28/22.
//

#include "utility_testing_functions.h"

std::string convertBsonTypeToString(const bsoncxx::type& type) {

    std::string typeString = "(Sub Error: failedToFindType)";

    switch (type) {
        case bsoncxx::v_noabi::type::k_double:
            typeString = "double";
            break;
        case bsoncxx::v_noabi::type::k_utf8:
            typeString = "utf8";
            break;
        case bsoncxx::v_noabi::type::k_document:
            typeString = "document";
            break;
        case bsoncxx::v_noabi::type::k_array:
            typeString = "array";
            break;
        case bsoncxx::v_noabi::type::k_binary:
            typeString = "binary";
            break;
        case bsoncxx::v_noabi::type::k_undefined:
            typeString = "undefined";
            break;
        case bsoncxx::v_noabi::type::k_oid:
            typeString = "oid";
            break;
        case bsoncxx::v_noabi::type::k_bool:
            typeString = "bool";
            break;
        case bsoncxx::v_noabi::type::k_date:
            typeString = "date";
            break;
        case bsoncxx::v_noabi::type::k_null:
            typeString = "null";
            break;
        case bsoncxx::v_noabi::type::k_regex:
            typeString = "regex";
            break;
        case bsoncxx::v_noabi::type::k_dbpointer:
            typeString = "dbpointer";
            break;
        case bsoncxx::v_noabi::type::k_code:
            typeString = "code";
            break;
        case bsoncxx::v_noabi::type::k_symbol:
            typeString = "symbol";
            break;
        case bsoncxx::v_noabi::type::k_codewscope:
            typeString = "codewscope";
            break;
        case bsoncxx::v_noabi::type::k_int32:
            typeString = "int32";
            break;
        case bsoncxx::v_noabi::type::k_timestamp:
            typeString = "timestamp";
            break;
        case bsoncxx::v_noabi::type::k_int64:
            typeString = "int64";
            break;
        case bsoncxx::v_noabi::type::k_decimal128:
            typeString = "decimal128";
            break;
        case bsoncxx::v_noabi::type::k_maxkey:
            typeString = "maxkey";
            break;
        case bsoncxx::v_noabi::type::k_minkey:
            typeString = "minkey";
            break;
    }

    return typeString;
}