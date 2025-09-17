//
// Created by jeremiah on 10/4/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "chat_room_shared_keys.h"
#include "setup_login_info.h"
#include "connection_pool_global_variable.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "ManageServerCommands.pb.h"
#include "RequestAdminInfo.grpc.pb.h"
#include "request_admin_info.h"
#include "admin_privileges_vector.h"
#include "save_activities_and_categories.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class InitialProgramOpenRequestInfoTesting : public ::testing::Test {
protected:

    request_admin_info::InitialProgramOpenRequestInfoRequest request;
    request_admin_info::InitialProgramOpenRequestInfoResponse response;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        initialProgramOpenRequestInfo(
                &request, &response
        );
    }

};

TEST_F(InitialProgramOpenRequestInfoTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_msg().empty());
                return response.success();
            }
    );
}

TEST_F(InitialProgramOpenRequestInfoTesting, successful) {

    const AdminLevelEnum admin_level = AdminLevelEnum::HANDLE_REPORTS_ONLY;

    runFunction(admin_level);

    request_admin_info::InitialProgramOpenRequestInfoResponse generated_response;

    generated_response.set_success(true);
    generated_response.set_admin_level(admin_level);
    generated_response.mutable_admin_privileges()->CopyFrom(admin_privileges[admin_level]);

    saveActivitiesAndCategories(
            accounts_db,
            generated_response.mutable_server_categories(),
            generated_response.mutable_server_activities()
    );

    generated_response.mutable_globals()->CopyFrom(response.globals());

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            generated_response,
            response
    );

    if(!equivalent) {
        std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
        std::cout << "response\n" << response.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}
