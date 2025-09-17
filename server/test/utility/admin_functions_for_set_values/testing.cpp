//
// Created by jeremiah on 6/16/22.
//

#include <gtest/gtest.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <admin_functions_for_set_values.h>
#include <admin_account_keys.h>
#include <AdminLevelEnum.grpc.pb.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class AdminFunctionsForSetValues : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(AdminFunctionsForSetValues, updateUserAccountWithDocument) {
    //NOTE: This is just a wrapper for calling update_one on a document.
}

TEST_F(AdminFunctionsForSetValues, checkForUpdateUserPrivilege) {
    setfields::SetFieldResponse error_response;

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value{
        document{}
            << admin_account_key::PRIVILEGE_LEVEL << AdminLevelEnum::FULL_ACCESS_ADMIN
        << finalize
    };

    bool return_val = checkForUpdateUserPrivilege(
        error_response.mutable_error_string(),
        admin_info_doc_value
    );

    EXPECT_EQ(return_val, true);

    admin_info_doc_value = bsoncxx::stdx::optional<bsoncxx::document::value>{
        document{}
        << admin_account_key::PRIVILEGE_LEVEL << AdminLevelEnum::NO_ADMIN_ACCESS
        << finalize
    };

    return_val = checkForUpdateUserPrivilege(
            error_response.mutable_error_string(),
            admin_info_doc_value
            );

    EXPECT_EQ(return_val, false);
}