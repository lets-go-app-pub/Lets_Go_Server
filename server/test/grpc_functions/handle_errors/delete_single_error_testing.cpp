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

class DeleteSingleErrorTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    handle_errors::DeleteSingleErrorRequest request;
    handle_errors::DeleteSingleErrorResponse response;

    bsoncxx::oid error_oid{};

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_error_oid(error_oid.to_string());
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

        deleteSingleError(
            &request, &response
        );
    }

};

TEST_F(DeleteSingleErrorTesting, invalidLoginInfo) {
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

TEST_F(DeleteSingleErrorTesting, invalidParam_error_oid) {
    request.set_error_oid("123");

    runFunction();

    EXPECT_FALSE(response.success());
    EXPECT_FALSE(response.error_msg().empty());
}

TEST_F(DeleteSingleErrorTesting, invalidParam_reason_for_description_tooShort) {
    request.set_reason_for_description(gen_random_alpha_numeric_string(server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON - 1));

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(DeleteSingleErrorTesting, invalidParam_reason_for_description_tooLong) {
    request.set_reason_for_description(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON + 1));

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(DeleteSingleErrorTesting, invalidAdminPrivilegeLevel) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(DeleteSingleErrorTesting, oidDoesNotExist) {
    FreshErrorsDoc fresh_error;
    fresh_error.generateRandomValues();
    fresh_error.setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.success());
    EXPECT_TRUE(response.error_msg().empty());

    //make sure nothing changed
    FreshErrorsDoc extracted_fresh_error(fresh_error.current_object_oid);
    EXPECT_EQ(fresh_error, extracted_fresh_error);
}

TEST_F(DeleteSingleErrorTesting, successful) {

    FreshErrorsDoc fresh_error;
    fresh_error.generateRandomValues();
    fresh_error.setIntoCollection();

    request.set_error_oid(fresh_error.current_object_oid.to_string());

    runFunction();

    EXPECT_TRUE(response.success());
    EXPECT_TRUE(response.error_msg().empty());

    FreshErrorsDoc extracted_fresh_error(fresh_error.current_object_oid);
    EXPECT_EQ(extracted_fresh_error.current_object_oid.to_string(), "000000000000000000000000");

    HandledErrorsDoc extracted_handled_errors_doc(fresh_error.current_object_oid);
    HandledErrorsDoc generated_handled_errors_doc(fresh_error);

    generated_handled_errors_doc.admin_name = TEMP_ADMIN_ACCOUNT_NAME;
    generated_handled_errors_doc.timestamp_handled = extracted_handled_errors_doc.timestamp_handled;
    generated_handled_errors_doc.error_handled_move_reason = ErrorHandledMoveReason::ERROR_HANDLED_REASON_SINGLE_ITEM_DELETED;
    generated_handled_errors_doc.errors_description = request.reason_for_description();

    EXPECT_EQ(generated_handled_errors_doc, extracted_handled_errors_doc);
}
