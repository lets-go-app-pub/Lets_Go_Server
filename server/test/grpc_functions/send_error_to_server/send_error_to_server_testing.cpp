//
// Created by jeremiah on 10/6/22.
//
#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "compare_equivalent_messages.h"
#include "SendErrorToServer.pb.h"
#include "send_error_to_server.h"
#include "grpc_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SendErrorToServerTesting : public ::testing::Test {
protected:

    send_error_to_server::SendErrorRequest request;
    send_error_to_server::SendErrorResponse response;

    void setupValidRequest() {
        request.mutable_message()->set_error_origin(ErrorOriginType::ERROR_ORIGIN_ANDROID);
        request.mutable_message()->set_error_urgency_level(ErrorUrgencyLevel::ERROR_URGENCY_LEVEL_UNKNOWN);
        request.mutable_message()->set_version_number(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
        request.mutable_message()->set_file_name(gen_random_alpha_numeric_string(rand() % 100 + 100));
        request.mutable_message()->set_line_number(rand() % 1000);
        request.mutable_message()->set_stack_trace(gen_random_alpha_numeric_string(rand() % 100 + 100));
        request.mutable_message()->set_api_number(rand() % 33 + 1);
        request.mutable_message()->set_device_name(gen_random_alpha_numeric_string(rand() % 100 + 100));
        request.mutable_message()->set_error_message(gen_random_alpha_numeric_string(rand() % 1000 + 1000));
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        sendErrorToServerMongoDb(&request, &response);
    }

};

TEST_F(SendErrorToServerTesting, noErrorMessageSet) {
    request.clear_message();

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_FAIL);
}

TEST_F(SendErrorToServerTesting, invalidErrorOrigin) {
    request.mutable_message()->set_error_origin(ErrorOriginType::ERROR_ORIGIN_WEB_SERVER);

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_FAIL);
}

TEST_F(SendErrorToServerTesting, invalidErrorUrgencyLevel) {
    request.mutable_message()->set_error_origin(ErrorOriginType::ERROR_ORIGIN_WEB_SERVER);

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_FAIL);
}

TEST_F(SendErrorToServerTesting, invalidVersionNumber) {
    request.mutable_message()->set_version_number(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION - 1);

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_OUTDATED_VERSION);
}

TEST_F(SendErrorToServerTesting, invalidFileName) {
    request.mutable_message()->set_file_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1));

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_FAIL);
}

TEST_F(SendErrorToServerTesting, invalidStackTrace) {
    request.mutable_message()->set_stack_trace(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE + 1));

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_FAIL);
}

TEST_F(SendErrorToServerTesting, invalidDeviceName) {
    request.mutable_message()->set_device_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1));

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_FAIL);
}

TEST_F(SendErrorToServerTesting, invalidErrorMessage_tooShort) {
    request.mutable_message()->set_error_message(gen_random_alpha_numeric_string(server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE - 1));

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_FAIL);
}

TEST_F(SendErrorToServerTesting, invalidErrorMessage_tooLong) {
    request.mutable_message()->set_error_message(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE + 1));

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_FAIL);
}

TEST_F(SendErrorToServerTesting, errorMessageHandled) {
    HandledErrorsListDoc handled_error;

    handled_error.error_origin = request.message().error_origin();
    handled_error.version_number = (int)request.message().version_number();
    handled_error.file_name = request.message().file_name();
    handled_error.line_number = (int)request.message().line_number();
    handled_error.error_handled_move_reason = ErrorHandledMoveReason::ERROR_HANDLED_REASON_BUG_NOT_RELEVANT;
    handled_error.setIntoCollection();

    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_SUCCESSFUL);

    FreshErrorsDoc fresh_error;
    fresh_error.getFromCollection();

    //error should not be stored
    EXPECT_EQ(fresh_error.current_object_oid.to_string(), "000000000000000000000000");
}

TEST_F(SendErrorToServerTesting, successfullyStoredMessage) {
    runFunction();

    EXPECT_EQ(response.return_status(), send_error_to_server::SendErrorResponse_Status_SUCCESSFUL);

    FreshErrorsDoc fresh_error;
    fresh_error.getFromCollection();

    EXPECT_NE(fresh_error.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_EQ(fresh_error.error_origin, request.message().error_origin());
    EXPECT_EQ(fresh_error.error_urgency, request.message().error_urgency_level());
    EXPECT_EQ(fresh_error.version_number, request.message().version_number());
    EXPECT_EQ(fresh_error.file_name, request.message().file_name());
    EXPECT_EQ(fresh_error.line_number, request.message().line_number());
    EXPECT_EQ(fresh_error.stack_trace, request.message().stack_trace());
    EXPECT_EQ(fresh_error.error_message, request.message().error_message());

    ASSERT_NE(fresh_error.api_number, nullptr);
    ASSERT_NE(fresh_error.device_name, nullptr);
    EXPECT_EQ(*fresh_error.api_number, request.message().api_number());
    EXPECT_EQ(*fresh_error.device_name, request.message().device_name());

    EXPECT_GT(fresh_error.timestamp_stored, 0);
}

TEST_F(SendErrorToServerTesting, checkLargestPossibleMessage) {

    request.mutable_message()->set_file_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES));
    request.mutable_message()->set_stack_trace(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE));
    request.mutable_message()->set_device_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES));
    request.mutable_message()->set_error_message(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE));

    size_t size_of_message = request.ByteSizeLong();

    EXPECT_LT(size_of_message, grpc_values::MAX_RECEIVE_MESSAGE_LENGTH);
}
