//
// Created by jeremiah on 9/10/21.
//

#include <utility_general_functions.h>
#include "extract_data_from_bsoncxx.h"

#include "bsoncxx/types.hpp"
#include "database_names.h"
#include "collection_names.h"

double extractFromBsoncxx_k_double(const bsoncxx::document::view& doc_view, const std::string& key) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_double) { //if element exists and is type double
        return element.get_double().value;
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_double, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

std::string extractFromBsoncxx_k_utf8(const bsoncxx::document::view& doc_view, const std::string& key) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        return element.get_string().value.to_string();
    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_utf8, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::document::view extractFromBsoncxx_k_document(const bsoncxx::document::view& doc_view, const std::string& key) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_document) { //if element exists and is type document
        return element.get_document().value;
    } else { //if element does not exist or is not type document
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_document, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::array::view extractFromBsoncxx_k_array(const bsoncxx::document::view& doc_view, const std::string& key) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_array) { //if element exists and is type array
        return element.get_array().value;
    } else { //if element does not exist or is not type array
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_array, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_binary extractFromBsoncxx_k_binary(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_binary) { //if element exists and is type binary
        return element.get_binary();
    } else { //if element does not exist or is not type binary
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_binary, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_undefined extractFromBsoncxx_k_undefined(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_undefined) { //if element exists and is type undefined
        return element.get_undefined();
    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_undefined, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::oid extractFromBsoncxx_k_oid(const bsoncxx::document::view& doc_view, const std::string& key) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_oid) { //if element exists and is type oid
        return element.get_oid().value;
    } else { //if element does not exist or is not type oid
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_oid, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bool extractFromBsoncxx_k_bool(const bsoncxx::document::view& doc_view, const std::string& key) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_bool) { //if element exists and is type bool
        return element.get_bool().value;
    } else { //if element does not exist or is not type bool
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_bool, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_date extractFromBsoncxx_k_date(const bsoncxx::document::view& doc_view, const std::string& key) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_date) { //if element exists and is type date
        return element.get_date();
    } else { //if element does not exist or is not type date
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_date, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_null extractFromBsoncxx_k_null(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_null) { //if element exists and is type null
        return element.get_null();
    } else { //if element does not exist or is not type null
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_null, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_regex extractFromBsoncxx_k_regex(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_regex) { //if element exists and is type regex
        return element.get_regex();
    } else { //if element does not exist or is not type regex
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_regex, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_dbpointer extractFromBsoncxx_k_dbpointer(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_dbpointer) { //if element exists and is type dbpointer
        return element.get_dbpointer();
    } else { //if element does not exist or is not type dbpointer
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_dbpointer, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_code extractFromBsoncxx_k_code(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_code) { //if element exists and is type code
        return element.get_code();
    } else { //if element does not exist or is not type code
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_code, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_symbol extractFromBsoncxx_k_symbol(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_symbol) { //if element exists and is type symbol
        return element.get_symbol();
    } else { //if element does not exist or is not type symbol
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_symbol, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_codewscope extractFromBsoncxx_k_codewscope(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_codewscope) { //if element exists and is type codewscope
        return element.get_codewscope();
    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_codewscope, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

int extractFromBsoncxx_k_int32(const bsoncxx::document::view& doc_view, const std::string& key) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        return element.get_int32().value;
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_int32, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_timestamp extractFromBsoncxx_k_timestamp(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_timestamp) { //if element exists and is type timestamp
        return element.get_timestamp();
    } else { //if element does not exist or is not type timestamp
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_timestamp, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

long long extractFromBsoncxx_k_int64(const bsoncxx::document::view& doc_view, const std::string& key) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_int64) { //if element exists and is type int64
        return element.get_int64().value;
    } else { //if element does not exist or is not type int64
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_int64, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::decimal128 extractFromBsoncxx_k_decimal128(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_decimal128) { //if element exists and is type decimal128
        return element.get_decimal128().value;
    } else { //if element does not exist or is not type decimal128
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_decimal128, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_maxkey extractFromBsoncxx_k_maxkey(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_maxkey) { //if element exists and is type maxkey
        return element.get_maxkey();
    } else { //if element does not exist or is not type maxkey
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_minkey, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::types::b_minkey extractFromBsoncxx_k_minkey(
        const bsoncxx::document::view& doc_view, const std::string& key
) {
    auto element = doc_view[key];
    if (element && element.type() == bsoncxx::type::k_minkey) { //if element exists and is type minkey
        return element.get_minkey();
    } else { //if element does not exist or is not type minkey
        logElementError(__LINE__, __FILE__, element,
                        doc_view, bsoncxx::type::k_minkey, key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value " + key + ".");
    }
}

bsoncxx::document::view extractFromBsoncxxArrayElement_k_document(
        const bsoncxx::array::element& arr_element
) {
    if (arr_element && arr_element.type() == bsoncxx::type::k_document) { //if element exists and is type document
        return arr_element.get_document().value;
    } else { //if element does not exist or is not type minkey
        bsoncxx::builder::stream::document dummy_doc;
        dummy_doc
            << "dummy_element" << arr_element.get_value();
        logElementError(__LINE__, __FILE__, dummy_doc.view()["dummy_element"],
                        dummy_doc, bsoncxx::type::k_document, "array_element_no_key",
                        database_names::ACCOUNTS_DATABASE_NAME,
                        collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        throw ErrorExtractingFromBsoncxx("Error requesting user info value from bsoncxx array element.");
    }
}