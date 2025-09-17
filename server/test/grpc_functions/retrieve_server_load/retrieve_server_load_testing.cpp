//
// Created by jeremiah on 10/6/22.
//

#include <chat_room_commands.h>
#include "gtest/gtest.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "setup_login_info.h"
#include "compare_equivalent_messages.h"
#include "retrieve_server_load.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RetrieveServerLoadTesting : public ::testing::Test {
protected:

    retrieve_server_load::RetrieveServerLoadRequest request;
    retrieve_server_load::RetrieveServerLoadResponse response;

    bool previous_server_accepting_connections = server_accepting_connections;
    int previous_number_chat_streams_running = number_chat_streams_running;

    void SetUp() override {
        server_accepting_connections = true;
        number_chat_streams_running = rand() % 100 + 5;
    }

    void TearDown() override {
        server_accepting_connections = previous_server_accepting_connections;
        number_chat_streams_running = previous_number_chat_streams_running;
    }

    void runFunction() {
        RetrieveServerLoadImpl().RetrieveServerLoadRPC(
                nullptr,
                &request,
                &response
        );
    }

};

TEST_F(RetrieveServerLoadTesting, requestNumClients_false) {
    request.set_request_num_clients(false);

    runFunction();

    EXPECT_TRUE(response.accepting_connections());
    EXPECT_EQ(response.num_clients(), -1);
}

TEST_F(RetrieveServerLoadTesting, requestNumClients_true) {
    request.set_request_num_clients(true);

    runFunction();

    EXPECT_TRUE(response.accepting_connections());
    EXPECT_EQ(response.num_clients(), number_chat_streams_running);
}

TEST_F(RetrieveServerLoadTesting, serverNotAcceptingConnections) {
    server_accepting_connections = false;
    request.set_request_num_clients(true);

    runFunction();

    EXPECT_FALSE(response.accepting_connections());
    EXPECT_EQ(response.num_clients(), -1);
}
