//
// Created by jeremiah on 6/21/22.
//
#include <utility_general_functions.h>
#include <fstream>
#include <account_objects.h>
#include <ChatRoomCommands.pb.h>
#include "gtest/gtest.h"
#include <google/protobuf/util/message_differencer.h>

#include <sort_time_frames_and_remove_overlaps.h>
#include <user_account_keys.h>
#include <ChatMessageStream.pb.h>
#include <store_and_send_messages.h>
#include <grpc_values.h>
#include <grpc_mock_stream/mock_stream.h>
#include <clear_database_for_testing.h>
#include <store_mongoDB_error_and_exception.h>
#include <filesystem>
#include <errors_objects.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class StoreMongoDbErrorAndException : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        std::filesystem::remove(general_values::ERROR_LOG_OUTPUT);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();

        std::filesystem::remove(general_values::ERROR_LOG_OUTPUT);
    }
};

TEST_F(StoreMongoDbErrorAndException, logErrorToFile) {

    const std::string first_error_message = "first_error\n";
    const std::string second_error_message = "second_error\n";
    logErrorToFile(first_error_message);

    std::ifstream fileOutputStream(general_values::ERROR_LOG_OUTPUT);

    std::string extracted_string;
    for (char c; fileOutputStream.get(c);) {
        extracted_string += c;
    }

    EXPECT_NE(std::string::npos, extracted_string.find(first_error_message));

    fileOutputStream.close();

    logErrorToFile(second_error_message);

    fileOutputStream.open(general_values::ERROR_LOG_OUTPUT);

    extracted_string.clear();
    for (char c; fileOutputStream.get(c);) {
        extracted_string += c;
    }

    fileOutputStream.close();

    EXPECT_NE(std::string::npos, extracted_string.find(first_error_message));
    EXPECT_NE(std::string::npos, extracted_string.find(second_error_message));
}

TEST_F(StoreMongoDbErrorAndException, storeMongoDBErrorAndException_noExtraParams) {
    std::string error_string = "error_string";
    int line_number = 123;
    std::string file_name = "file_name";

    std::optional<std::string> exception_string = "exception_string";
    storeMongoDBErrorAndException(line_number, file_name, exception_string, error_string);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database errorDB = mongoCppClient[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection errorCollection = errorDB[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    auto result = errorCollection.find_one(document{} << finalize);

    ASSERT_TRUE(result);

    bsoncxx::document::view result_view = *result;
    bsoncxx::oid result_id = result_view["_id"].get_oid().value;

    FreshErrorsDoc extracted_fresh_errors_doc(result_id);

    FreshErrorsDoc generated_fresh_errors_doc;

    generated_fresh_errors_doc.current_object_oid = extracted_fresh_errors_doc.current_object_oid;
    generated_fresh_errors_doc.error_origin = ErrorOriginType::ERROR_ORIGIN_SERVER;
    generated_fresh_errors_doc.error_urgency = ErrorUrgencyLevel::ERROR_URGENCY_LEVEL_UNKNOWN;
    generated_fresh_errors_doc.version_number = (int) version_number::SERVER_CURRENT_VERSION_NUMBER;
    generated_fresh_errors_doc.file_name = file_name;
    generated_fresh_errors_doc.line_number = line_number;
    generated_fresh_errors_doc.stack_trace = "";
    generated_fresh_errors_doc.timestamp_stored = extracted_fresh_errors_doc.timestamp_stored;
    generated_fresh_errors_doc.api_number = nullptr;
    generated_fresh_errors_doc.device_name = nullptr;
    generated_fresh_errors_doc.error_message = "Exception Message: " + *exception_string + "\nERROR:\n" + error_string;

    EXPECT_EQ(extracted_fresh_errors_doc, generated_fresh_errors_doc);

}

TEST_F(StoreMongoDbErrorAndException, storeMongoDBErrorAndException_paramsPassed) {

    std::string error_string = "error_string";
    int line_number = 123;
    std::string file_name = "file_name";

    std::string first_param_key = "key_one";
    int first_param_value = 444;

    std::string second_param_key = "key_two";
    bsoncxx::oid second_param_value{};

    std::optional<std::string> dummy_exception_string;
    storeMongoDBErrorAndException(
            line_number, file_name, dummy_exception_string, error_string,
            first_param_key, first_param_value,
            second_param_key, second_param_value
            );

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database errorDB = mongoCppClient[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection errorCollection = errorDB[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    auto result = errorCollection.find_one(document{} << finalize);

    ASSERT_TRUE(result);

    bsoncxx::document::view result_view = *result;
    bsoncxx::oid result_id = result_view["_id"].get_oid().value;

    FreshErrorsDoc extracted_fresh_errors_doc(result_id);

    FreshErrorsDoc generated_fresh_errors_doc;

    generated_fresh_errors_doc.current_object_oid = extracted_fresh_errors_doc.current_object_oid;
    generated_fresh_errors_doc.error_origin = ErrorOriginType::ERROR_ORIGIN_SERVER;
    generated_fresh_errors_doc.error_urgency = ErrorUrgencyLevel::ERROR_URGENCY_LEVEL_UNKNOWN;
    generated_fresh_errors_doc.version_number = (int) version_number::SERVER_CURRENT_VERSION_NUMBER;
    generated_fresh_errors_doc.file_name = file_name;
    generated_fresh_errors_doc.line_number = line_number;
    generated_fresh_errors_doc.stack_trace = "";
    generated_fresh_errors_doc.timestamp_stored = extracted_fresh_errors_doc.timestamp_stored;
    generated_fresh_errors_doc.api_number = nullptr;
    generated_fresh_errors_doc.device_name = nullptr;
    generated_fresh_errors_doc.error_message = first_param_key + ": " + std::to_string(first_param_value) + "\n" + second_param_key + ": " + second_param_value.to_string() + "\nERROR:\n" + error_string;

    EXPECT_EQ(extracted_fresh_errors_doc, generated_fresh_errors_doc);
}

TEST_F(StoreMongoDbErrorAndException, storeMongoDBErrorAndException_exceptionHandled) {
    std::string error_string = "error_string";
    int line_number = 123;
    std::string file_name = "file_name";

    HandledErrorsListDoc handled_errors_list_doc;

    handled_errors_list_doc.error_origin = ErrorOriginType::ERROR_ORIGIN_SERVER;
    handled_errors_list_doc.version_number = (int) version_number::SERVER_CURRENT_VERSION_NUMBER;
    handled_errors_list_doc.file_name = file_name;
    handled_errors_list_doc.line_number = line_number;
    handled_errors_list_doc.error_handled_move_reason = ErrorHandledMoveReason::ERROR_HANDLED_REASON_BUG_FIXED;

    handled_errors_list_doc.setIntoCollection();

    std::optional<std::string> dummy_exception_string;
    storeMongoDBErrorAndException(line_number, file_name, dummy_exception_string, error_string);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database errorDB = mongoCppClient[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection errorCollection = errorDB[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    auto result = errorCollection.find_one(document{} << finalize);

    ASSERT_FALSE(result);
}