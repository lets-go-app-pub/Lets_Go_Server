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
#include "generate_multiple_random_accounts.h"
#include "setup_login_info.h"
#include "connection_pool_global_variable.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "ManageServerCommands.pb.h"
#include "manage_server_commands.h"
#include "server_values.h"
#include "server_accepting_connections_bool.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RequestServerShutdownTesting : public ::testing::Test {
protected:

    manage_server_commands::RequestServerShutdownRequest request;
    manage_server_commands::RequestServerShutdownResponse response;

    std::string previous_server_address;
    bool previous_server_accepting_connections = false;
    std::chrono::milliseconds previous_chat_change_stream_await_time{-1};
    std::chrono::milliseconds previous_delay_for_message_ordering{-1};
    std::chrono::milliseconds previous_message_ordering_thread_sleep_time{-1};

    std::unique_ptr<std::jthread> server_thread = nullptr;

    std::atomic_bool stopped = false;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_server_address(current_server_address);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        previous_server_address = current_server_address;
        previous_server_accepting_connections = server_accepting_connections;
        previous_chat_change_stream_await_time = chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME;
        previous_delay_for_message_ordering = chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING;
        previous_message_ordering_thread_sleep_time = chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME;

        server_accepting_connections = true;

        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = std::chrono::milliseconds{5};
        chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = std::chrono::milliseconds{5};
        chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = std::chrono::milliseconds{5};

        current_server_address = gen_random_alpha_numeric_string(rand() % 20 + 10);

        grpc_server_impl = std::make_unique<GrpcServerImpl>();

        server_thread = std::make_unique<std::jthread>(
                [&](){
                    grpc_server_impl->Run();
                    stopped = true;
                }
        );

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();

        grpc_server_impl->blockWhileWaitingForShutdown(std::chrono::milliseconds{5});
        grpc_server_impl = nullptr;

        server_thread->join();

        current_server_address = previous_server_address;
        server_accepting_connections = previous_server_accepting_connections;
        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = previous_chat_change_stream_await_time;
        chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = previous_delay_for_message_ordering;
        chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = previous_message_ordering_thread_sleep_time;
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        //Sleep to allow the server_thread to start.
        std::this_thread::sleep_for(std::chrono::milliseconds{100});

        createTempAdminAccount(admin_level);

        requestServerShutdown(
                &request, &response
        );
    }

    void expectServerFailedShutdown() {
        EXPECT_FALSE(response.successful());
        EXPECT_FALSE(response.error_message().empty());

        //allow the shutdown thread to start if it is going to
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        EXPECT_FALSE(stopped);
    }

    void expectServerSuccessfulShutdown() {
        EXPECT_TRUE(response.successful());
        EXPECT_TRUE(response.error_message().empty());
        EXPECT_EQ(SHUTDOWN_RETURN_CODE, SERVER_SHUTDOWN_REQUESTED_RETURN_CODE);

        if(!response.error_message().empty()) {
            std::cout << response.error_message() << '\n';
        }

        //This must send in the shutdown command through GrpcServerImpl::beginShutdownOnSeparateThread and
        // there is a wait time associated with it.
        std::this_thread::sleep_for(std::chrono::milliseconds{2 * 1000});
        EXPECT_TRUE(stopped);
    }
};

TEST_F(RequestServerShutdownTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_message().empty());
                return response.successful();
            }
    );

    expectServerFailedShutdown();
}

TEST_F(RequestServerShutdownTesting, noAdminAccess) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    expectServerFailedShutdown();
}

TEST_F(RequestServerShutdownTesting, serverAddressTooLong) {
    request.set_server_address(gen_random_alpha_numeric_string(MAXIMUM_NUMBER_ALLOWED_CHARS_ADDRESS));

    runFunction();

    expectServerFailedShutdown();
}

TEST_F(RequestServerShutdownTesting, wrongAddressPassed) {
    request.set_server_address(current_server_address + 'a');

    runFunction();

    expectServerFailedShutdown();
}

TEST_F(RequestServerShutdownTesting, successfullyShutDown) {
    runFunction();

    expectServerSuccessfulShutdown();
}
