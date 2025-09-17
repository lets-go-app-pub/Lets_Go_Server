//
// Created by jeremiah on 6/17/22.
//

#include <gtest/gtest.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <clear_database_for_testing.h>
#include <generate_multiple_random_accounts.h>
#include <account_objects.h>
#include <utility_general_functions.h>
#include <LoginFunction.pb.h>
#include <general_values.h>
#include <user_account_keys.h>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/exception/exception.hpp>

#include "grpc_function_server_template.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GrpcFunctionServerTemplate : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(GrpcFunctionServerTemplate, grpcValidateLoginFunctionTemplate) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account(generated_account_oid);

    std::chrono::milliseconds current_timestamp{getCurrentTimestamp()};

    const std::string gender = "$1234";

    bsoncxx::builder::stream::document merge_doc;
    merge_doc
        << user_account_keys::GENDER << open_document
            << "$literal" << bsoncxx::types::b_string{gender}
        << close_document;

    ReturnStatus return_status = ReturnStatus_MAX;

    auto setReturnStatus = [&](const ReturnStatus& returnStatus) {
        return_status = returnStatus;
    };

    auto setSuccess = [&](const bsoncxx::document::view&) {
        return_status = ReturnStatus::SUCCESS;
    };

    grpcValidateLoginFunctionTemplate<true>(
            generated_account_oid.to_string(),
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            current_timestamp,
            merge_doc,
            setReturnStatus,
            setSuccess,
            nullptr,
            nullptr
    );

    user_account.gender = gender;

    UserAccountDoc after_user_account(generated_account_oid);
    EXPECT_EQ(user_account, after_user_account);
    EXPECT_EQ(return_status, ReturnStatus::SUCCESS);
}

TEST_F(GrpcFunctionServerTemplate, grpcFunctionServerTemplate_nonNullParams) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account(generated_account_oid);

    std::chrono::milliseconds current_timestamp{getCurrentTimestamp()};

    const std::string gender = "$1234";

    bsoncxx::builder::stream::document merge_doc;
    merge_doc
    << user_account_keys::GENDER << open_document
        << "$literal" << bsoncxx::types::b_string{gender}
    << close_document;

    ReturnStatus return_status = ReturnStatus_MAX;

    auto setReturnStatus = [&](const ReturnStatus& returnStatus) {
        return_status = returnStatus;
    };

    auto setSuccess = [&](const bsoncxx::document::view& userAccountDocView) {
        return_status = ReturnStatus::SUCCESS;

        //make sure projection document below works
        EXPECT_TRUE(userAccountDocView[user_account_keys::AGE]);
        EXPECT_FALSE(userAccountDocView[user_account_keys::GENDER]);
    };

    std::shared_ptr<bsoncxx::builder::stream::document> projectionDocument = std::make_shared<document>();

    (*projectionDocument)
        << user_account_keys::AGE << 1;

    bool store_stats_called = false;
    auto store_info_to_user_statistics = [&](mongocxx::client&, mongocxx::database&) {
        store_stats_called = true;
    };

    grpcValidateLoginFunctionTemplate<true>(
            generated_account_oid.to_string(),
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            current_timestamp,
            merge_doc,
            setReturnStatus,
            setSuccess,
            projectionDocument,
            store_info_to_user_statistics
    );

    user_account.gender = gender;

    UserAccountDoc after_user_account(generated_account_oid);
    EXPECT_EQ(user_account, after_user_account);
    EXPECT_EQ(return_status, ReturnStatus::SUCCESS);
    EXPECT_TRUE(store_stats_called);

}

