//
// Created by jeremiah on 6/18/22.
//
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include <create_chat_room_helper.h>
#include <chat_room_message_keys.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ServerInitializationFunctions : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(ServerInitializationFunctions, setupPythonModules) {
    //NOTE: Trivial and obvious if not working. Python functions are handled elsewhere.
}

TEST_F(ServerInitializationFunctions, setupMongoDBIndexing) {
    //NOTE: Redundant, would be a copy of the code and wouldn't prove anything.
}

TEST_F(ServerInitializationFunctions, setupMandatoryDatabaseDocs) {
    //NOTE: Redundant, would be a copy of the code and wouldn't prove anything.
}
