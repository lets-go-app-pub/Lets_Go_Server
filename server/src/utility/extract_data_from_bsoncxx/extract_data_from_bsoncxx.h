//
// Created by jeremiah on 9/10/21.
//

#pragma once

#include <string>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/array/view.hpp>
#include <bsoncxx/oid.hpp>

struct ErrorExtractingFromBsoncxx : public std::exception {

    explicit ErrorExtractingFromBsoncxx(std::string error_string) : error_string(std::move(error_string)) {}

    [[nodiscard]] const char* what() const noexcept override {
        return error_string.c_str();
    }

private:
    std::string error_string;
};

double extractFromBsoncxx_k_double(const bsoncxx::document::view& doc_view, const std::string& key);

std::string extractFromBsoncxx_k_utf8(const bsoncxx::document::view& doc_view, const std::string& key);

bsoncxx::document::view extractFromBsoncxx_k_document(const bsoncxx::document::view& doc_view, const std::string& key);

bsoncxx::array::view extractFromBsoncxx_k_array(const bsoncxx::document::view& doc_view, const std::string& key);

bsoncxx::types::b_binary extractFromBsoncxx_k_binary(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::types::b_undefined extractFromBsoncxx_k_undefined(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::oid extractFromBsoncxx_k_oid(const bsoncxx::document::view& doc_view, const std::string& key);

bool extractFromBsoncxx_k_bool(const bsoncxx::document::view& doc_view, const std::string& key);

bsoncxx::types::b_date extractFromBsoncxx_k_date(const bsoncxx::document::view& doc_view, const std::string& key);

bsoncxx::types::b_null extractFromBsoncxx_k_null(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::types::b_regex extractFromBsoncxx_k_regex(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::types::b_dbpointer extractFromBsoncxx_k_dbpointer(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::types::b_code extractFromBsoncxx_k_code(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::types::b_symbol extractFromBsoncxx_k_symbol(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::types::b_codewscope extractFromBsoncxx_k_codewscope(
        const bsoncxx::document::view& doc_view, const std::string& key
);

int extractFromBsoncxx_k_int32(const bsoncxx::document::view& doc_view, const std::string& key);

bsoncxx::types::b_timestamp extractFromBsoncxx_k_timestamp(
        const bsoncxx::document::view& doc_view, const std::string& key
);

long long extractFromBsoncxx_k_int64(const bsoncxx::document::view& doc_view, const std::string& key);

bsoncxx::decimal128 extractFromBsoncxx_k_decimal128(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::types::b_maxkey extractFromBsoncxx_k_maxkey(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::types::b_minkey extractFromBsoncxx_k_minkey(
        const bsoncxx::document::view& doc_view, const std::string& key
);

bsoncxx::document::view extractFromBsoncxxArrayElement_k_document(
        const bsoncxx::array::element& arr_element
);
