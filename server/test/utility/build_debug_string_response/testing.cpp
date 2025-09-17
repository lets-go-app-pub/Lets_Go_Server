//
// Created by jeremiah on 6/16/22.
//

#include <gtest/gtest.h>
#include <SetFields.pb.h>
#include <server_parameter_restrictions.h>
#include "build_debug_string_response.h"

class BuildDebugStringResponse : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(BuildDebugStringResponse, buildDebugStringResponse) {

    setfields::SetFieldResponse response;

    std::string return_string = buildDebugStringResponse(&response);

    EXPECT_EQ("Response debug string is empty.", return_string);

    response.set_error_string("123");

    return_string = buildDebugStringResponse(&response);

    EXPECT_EQ("error_string: \"123\"\n", return_string);

    response.set_error_string(
            std::string(
                    server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE + 1,
                    'a'
                    )
            );

    return_string = buildDebugStringResponse(&response);

    EXPECT_EQ("Response debug string too long.", return_string);

    return_string = buildDebugStringResponse(nullptr);

    EXPECT_EQ("Response pointer not set.", return_string);
}
