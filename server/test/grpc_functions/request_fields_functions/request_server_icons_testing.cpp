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

#include "setup_login_info.h"
#include "connection_pool_global_variable.h"
#include "ManageServerCommands.pb.h"
#include "request_fields_functions.h"
#include "generate_multiple_random_accounts.h"
#include "save_activities_and_categories.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "grpc_mock_stream/mock_stream.h"
#include "icons_info_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RequestServerIconsTesting : public ::testing::Test {
protected:

    request_fields::ServerIconsRequest request;
    grpc::testing::MockServerWriterVector<request_fields::ServerIconsResponse> response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection icons_info_collection = accounts_db[collection_names::ICONS_INFO_COLLECTION_NAME];

    void setupValidClientRequest() {
        request.Clear();

        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        for(int i = 0; i < 10; ++i) {
            request.add_icon_index(i);
        }
    }

    void setupValidAdminRequest() {
        request.Clear();

        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(user_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        requestServerIcons(&request, &response);
    }

    void compareIconResponseByIndex(
            const bsoncxx::document::view& doc,
            int current_index
            ) {

        ASSERT_GT(response.write_params.size(), current_index);

        request_fields::ServerIconsResponse generated_response;

        generated_response.set_return_status(ReturnStatus::SUCCESS);
        generated_response.set_index_number(doc[icons_info_keys::INDEX].get_int64().value);
        generated_response.set_icon_in_bytes(doc[icons_info_keys::ICON_IN_BYTES].get_string().value.to_string());
        generated_response.set_icon_size_in_bytes(doc[icons_info_keys::ICON_SIZE_IN_BYTES].get_int64().value);
        generated_response.set_is_active(doc[icons_info_keys::ICON_ACTIVE].get_bool().value);

        EXPECT_GT(response.write_params[current_index].msg.icon_last_updated_timestamp(), 0);
        generated_response.set_icon_last_updated_timestamp(response.write_params[current_index].msg.icon_last_updated_timestamp());

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                generated_response,
                response.write_params[current_index].msg
        );

        if(!equivalent) {
            std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
            std::cout << "response\n" << response.write_params[current_index].msg.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }

    void compareResponse(
            const bsoncxx::document::view& find_document
            ) {
        mongocxx::options::find opts;

        opts.sort(
                document{}
                        << icons_info_keys::INDEX << 1
                        << finalize
        );

        auto find_icons_cursor = icons_info_collection.find(find_document, opts);

        int current_index = 0;
        for(const auto& doc : find_icons_cursor) {
            compareIconResponseByIndex(
                    doc,
                    current_index
            );
            current_index++;
        }
    }

};

TEST_F(RequestServerIconsTesting, invalidLoginInfo) {
    createTempAdminAccount(AdminLevelEnum::FULL_ACCESS_ADMIN);

    checkLoginInfoAdminAndClient(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            user_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                request.mutable_login_info()->CopyFrom(login_info);

                response.write_params.clear();

                runFunction();

                EXPECT_EQ(response.write_params.size(), 1);
                if(response.write_params.size() == 1) {
                    return response.write_params[0].msg.return_status();
                } else {
                    return ReturnStatus::UNKNOWN;
                }
            }
    );
}

TEST_F(RequestServerIconsTesting, emptyIconIndex_clientRequest) {
    setupValidClientRequest();
    request.clear_icon_index();

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    EXPECT_EQ(response.write_params[0].msg.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(RequestServerIconsTesting, successful_adminRequest) {
    setupValidAdminRequest();

    createTempAdminAccount(AdminLevelEnum::FULL_ACCESS_ADMIN);

    runFunction();

    compareResponse(document{} << finalize);
}

TEST_F(RequestServerIconsTesting, successful_clientRequest) {
    setupValidClientRequest();

    bsoncxx::builder::basic::array icons_to_find;
    request.clear_icon_index();

    request.add_icon_index(0);
    icons_to_find.append(0L);

    //100 is arbitrary, it is OK if it is too large
    for(long i = 1; i < 100; ++i) {
        if(rand() % 3 == 0) {
            request.add_icon_index(i);
            icons_to_find.append(i);
        }
    }

    runFunction();

    compareResponse(
        document{}
            << "_id" << open_document
                << "$in" << icons_to_find
            << close_document
        << finalize
    );
}
