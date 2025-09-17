//
// Created by jeremiah on 9/16/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "setup_login_info.h"
#include "HandleErrors.pb.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "handle_errors.h"
#include "errors_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetErrorToHandledTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    handle_errors::SetErrorToHandledRequest request;
    handle_errors::SetErrorToHandledResponse response;

    bsoncxx::oid error_oid{};

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        auto error_parameters = request.mutable_error_parameters();

        error_parameters->set_error_origin(ErrorOriginType(rand() % (ErrorOriginType_MAX-1) + 1));
        error_parameters->set_version_number(rand() % 100 + 1);
        error_parameters->set_file_name(gen_random_alpha_numeric_string(rand() % 100 + 10));
        error_parameters->set_line_number(rand() % 10000);

        request.set_reason(ErrorHandledMoveReason(rand() % ErrorHandledMoveReason_MAX));
        request.set_reason_for_description(gen_random_alpha_numeric_string(rand() % 10 + server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON));
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        setErrorToHandled(&request, &response);
    }

    FreshErrorsDoc generateFreshErrorMatchingRequest() {
        FreshErrorsDoc fresh_error;
        fresh_error.generateRandomValues();
        fresh_error.error_origin = request.error_parameters().error_origin();
        fresh_error.version_number = (int)request.error_parameters().version_number();
        fresh_error.file_name = request.error_parameters().file_name();
        fresh_error.line_number = (int)request.error_parameters().line_number();
        fresh_error.setIntoCollection();

        return fresh_error;
    }

    void checkIfFreshErrorWasMoved(const FreshErrorsDoc& fresh_error) {
        //make sure error was moved
        FreshErrorsDoc extracted_first_fresh_error(fresh_error.current_object_oid);
        EXPECT_EQ(extracted_first_fresh_error.current_object_oid.to_string(), "000000000000000000000000");

        HandledErrorsDoc extracted_first_handled_error(fresh_error.current_object_oid);
        HandledErrorsDoc generated_first_handled_error(fresh_error);

        generated_first_handled_error.admin_name = TEMP_ADMIN_ACCOUNT_NAME;
        generated_first_handled_error.timestamp_handled = extracted_first_handled_error.timestamp_handled;
        generated_first_handled_error.error_handled_move_reason = ErrorHandledMoveReason::ERROR_HANDLED_REASON_SINGLE_ITEM_DELETED;
        generated_first_handled_error.errors_description = request.reason_for_description();

        EXPECT_EQ(generated_first_handled_error, extracted_first_handled_error);
    }

    void compareRequestToHandledErrorsList() {
        HandledErrorsListDoc extracted_handled_errors_list_doc;
        extracted_handled_errors_list_doc.getFromCollection();

        HandledErrorsListDoc generated_handled_errors_list_doc;
        generated_handled_errors_list_doc.current_object_oid = extracted_handled_errors_list_doc.current_object_oid;
        generated_handled_errors_list_doc.error_origin = request.error_parameters().error_origin();
        generated_handled_errors_list_doc.version_number = (int)request.error_parameters().version_number();
        generated_handled_errors_list_doc.file_name = request.error_parameters().file_name();
        generated_handled_errors_list_doc.line_number = (int)request.error_parameters().line_number();
        generated_handled_errors_list_doc.error_handled_move_reason = request.reason();
        generated_handled_errors_list_doc.description = std::make_unique<std::string>(request.reason_for_description());

        EXPECT_EQ(generated_handled_errors_list_doc, extracted_handled_errors_list_doc);
    }

};

TEST_F(SetErrorToHandledTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info)->bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_msg().empty());
                return response.success();
            }
    );
}

TEST_F(SetErrorToHandledTesting, invalidParam_errorOrigin) {
    request.mutable_error_parameters()->set_error_origin(ErrorOriginType(-1));

    runFunction();

    EXPECT_FALSE(response.success());
    EXPECT_FALSE(response.error_msg().empty());
}

TEST_F(SetErrorToHandledTesting, invalidParam_fileName) {
    request.mutable_error_parameters()->set_file_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1));

    runFunction();

    EXPECT_FALSE(response.success());
    EXPECT_FALSE(response.error_msg().empty());
}

TEST_F(SetErrorToHandledTesting, invalidParam_reason) {
    request.set_reason(ErrorHandledMoveReason(-1));

    runFunction();

    EXPECT_FALSE(response.success());
    EXPECT_FALSE(response.error_msg().empty());
}

TEST_F(SetErrorToHandledTesting, invalidParam_reason_for_description_tooSmall) {
    request.set_reason_for_description(gen_random_alpha_numeric_string(server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON - 1));

    runFunction();

    EXPECT_FALSE(response.success());
    EXPECT_FALSE(response.error_msg().empty());
}

TEST_F(SetErrorToHandledTesting, invalidParam_reason_for_description_tooLarge) {
    request.set_reason_for_description(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON + 1));

    runFunction();

    EXPECT_FALSE(response.success());
    EXPECT_FALSE(response.error_msg().empty());
}

TEST_F(SetErrorToHandledTesting, invalidAdminPrivilegeLevel) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.success());
    EXPECT_FALSE(response.error_msg().empty());
}

TEST_F(SetErrorToHandledTesting, noErrorsExist) {

    FreshErrorsDoc fresh_error;
    fresh_error.generateRandomValues();
    fresh_error.version_number = (int)request.error_parameters().version_number() - 1;
    fresh_error.setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.success());
    EXPECT_TRUE(response.error_msg().empty());

    //make sure error was not moved
    FreshErrorsDoc extracted_fresh_error(fresh_error.current_object_oid);
    EXPECT_EQ(fresh_error, extracted_fresh_error);

    compareRequestToHandledErrorsList();
}

TEST_F(SetErrorToHandledTesting, successful) {
    FreshErrorsDoc first_fresh_error = generateFreshErrorMatchingRequest();
    FreshErrorsDoc second_fresh_error = generateFreshErrorMatchingRequest();

    runFunction();

    EXPECT_TRUE(response.success());
    EXPECT_TRUE(response.error_msg().empty());

    checkIfFreshErrorWasMoved(first_fresh_error);
    checkIfFreshErrorWasMoved(second_fresh_error);

    compareRequestToHandledErrorsList();
}
