//
// Created by jeremiah on 6/16/22.
//

#include <gtest/gtest.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <admin_account_keys.h>
#include <mongocxx/client_session.hpp>

#include "create_chat_room_helper.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

static std::string KEY = "key";

class ExtractDataFromBsoncxx : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_double) {

    bsoncxx::builder::stream::document extraction_doc;

    double initial_value = 52.3;

    extraction_doc
            << KEY << bsoncxx::types::b_double{initial_value};

    EXPECT_FLOAT_EQ(
            extractFromBsoncxx_k_double(
                    extraction_doc,
                    KEY
            ),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_utf8) {

    bsoncxx::builder::stream::document extraction_doc;

    std::string initial_value = "52.3";

    extraction_doc
            << KEY << bsoncxx::types::b_string{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_utf8(
                    extraction_doc,
                    KEY
            ),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_document) {

    bsoncxx::builder::stream::document extraction_doc;

    bsoncxx::document::value initial_value = document{}
        << "123" << "456"
        << finalize;

    extraction_doc
            << KEY << bsoncxx::types::b_document{initial_value};

    EXPECT_EQ(
        extractFromBsoncxx_k_document(
                extraction_doc,
                KEY
        ),
        initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_array) {

    bsoncxx::builder::stream::document extraction_doc;

    bsoncxx::builder::basic::array initial_value;

    initial_value.append("abc");
    initial_value.append("123");

    extraction_doc
            << KEY << bsoncxx::types::b_array{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_array(
                    extraction_doc,
                    KEY
            ),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_binary) {

    bsoncxx::builder::stream::document extraction_doc;

    extraction_doc
            << KEY << bsoncxx::types::b_binary{};

    EXPECT_NO_THROW(
            extractFromBsoncxx_k_binary(
                    extraction_doc,
                    KEY
            )
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_undefined) {

    bsoncxx::builder::stream::document extraction_doc;

    extraction_doc
            << KEY << bsoncxx::types::b_undefined{};

    ASSERT_NO_THROW(
        extractFromBsoncxx_k_undefined(
                extraction_doc,
                KEY
                )
        );
}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_oid) {

    bsoncxx::builder::stream::document extraction_doc;

    bsoncxx::oid initial_value{};

    extraction_doc
            << KEY << bsoncxx::types::b_oid{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_oid(
                    extraction_doc,
                    KEY
            ),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_bool) {

    bsoncxx::builder::stream::document extraction_doc;

    bool initial_value = true;

    extraction_doc
            << KEY << bsoncxx::types::b_bool{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_bool(
                    extraction_doc,
                    KEY
            ),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_date) {

    bsoncxx::builder::stream::document extraction_doc;

    std::chrono::milliseconds initial_value{1234};

    extraction_doc
            << KEY << bsoncxx::types::b_date{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_date(
                    extraction_doc,
                    KEY
            ).value,
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_null) {

    bsoncxx::builder::stream::document extraction_doc;

    extraction_doc
            << KEY << bsoncxx::types::b_null{};

    EXPECT_NO_THROW(
            extractFromBsoncxx_k_null(
                    extraction_doc,
                    KEY
            )
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_regex) {

    bsoncxx::builder::stream::document extraction_doc;

    std::string initial_value = "^p.*";

    extraction_doc
            << KEY << bsoncxx::types::b_regex{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_regex(
                    extraction_doc,
                    KEY
            ).regex.to_string(),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_dbpointer) {

    bsoncxx::builder::stream::document extraction_doc;

    extraction_doc
            << KEY << bsoncxx::types::b_dbpointer{};

    EXPECT_NO_THROW(
            extractFromBsoncxx_k_dbpointer(
                    extraction_doc,
                    KEY
            )
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_code) {

    bsoncxx::builder::stream::document extraction_doc;

    std::string initial_value = "52.3";

    extraction_doc
            << KEY << bsoncxx::types::b_code{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_code(
                    extraction_doc,
                    KEY
            ).code.to_string(),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_symbol) {

    bsoncxx::builder::stream::document extraction_doc;

    std::string initial_value = "52.3";

    extraction_doc
            << KEY << bsoncxx::types::b_symbol{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_symbol(
                    extraction_doc,
                    KEY
            ).symbol.to_string(),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_codewscope) {
    bsoncxx::builder::stream::document extraction_doc;

    extraction_doc
        << KEY << bsoncxx::types::b_codewscope{"123", document{} << finalize};

    EXPECT_NO_THROW(
            extractFromBsoncxx_k_codewscope(
                    extraction_doc,
                    KEY
            )
    );
}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_int32) {

    bsoncxx::builder::stream::document extraction_doc;

    int initial_value = 52;

    extraction_doc
            << KEY << bsoncxx::types::b_int32{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_int32(
                    extraction_doc,
                    KEY
            ),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_timestamp) {

    bsoncxx::builder::stream::document extraction_doc;

    extraction_doc
        << KEY << bsoncxx::types::b_timestamp{};

    EXPECT_NO_THROW(
            extractFromBsoncxx_k_timestamp(
                    extraction_doc,
                    KEY
            )
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_int64) {

    bsoncxx::builder::stream::document extraction_doc;

    long initial_value = 1234567890;

    extraction_doc
            << KEY << bsoncxx::types::b_int64{initial_value};

    EXPECT_EQ(
            extractFromBsoncxx_k_int64(
                    extraction_doc,
                    KEY
            ),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_decimal128) {

    bsoncxx::builder::stream::document extraction_doc;

    const std::string initial_value = "0.123456789";

    extraction_doc
            << KEY << bsoncxx::types::b_decimal128(initial_value);

    EXPECT_EQ(
            extractFromBsoncxx_k_decimal128(
                    extraction_doc,
                    KEY
            ).to_string(),
            initial_value
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_maxkey) {

    bsoncxx::builder::stream::document extraction_doc;

    extraction_doc
            << KEY << bsoncxx::types::b_maxkey{};

    EXPECT_NO_THROW(
        extractFromBsoncxx_k_maxkey(
                extraction_doc,
                KEY
        )
    );

}

TEST_F(ExtractDataFromBsoncxx, extractFromBsoncxx_k_minkey) {

    bsoncxx::builder::stream::document extraction_doc;

    extraction_doc
        << KEY << bsoncxx::types::b_minkey{};

    EXPECT_NO_THROW(
            extractFromBsoncxx_k_minkey(
                    extraction_doc,
                    KEY
            )
    );

}
