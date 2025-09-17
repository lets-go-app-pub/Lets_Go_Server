//
// Created by jeremiah on 9/1/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include "user_account_keys.h"
#include "utility_general_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GenerateDistanceFromLocationsTesting : public ::testing::Test {
protected:

    bsoncxx::oid match_account_oid;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection dummy_collection = accounts_db["dummy_collection"];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
        dummy_collection.drop();
    }

    void checkDistance(
        const double user_longitude,
        const double user_latitude,
        const double match_longitude,
        const double match_latitude,
        const double expected_distance
    ) {

        dummy_collection.insert_one(
                document{}
                    << "_id" << match_account_oid
                << finalize
                );

        mongocxx::pipeline pipe;

        pipe.match(
            document{}
                << "_id" << match_account_oid
            << finalize
        );

        pipe.project(
            document{}
                << user_account_keys::LOCATION << open_document
                    << "type" << "Point"
                    << "coordinates" << open_array
                        << bsoncxx::types::b_double{match_longitude}
                        << bsoncxx::types::b_double{match_latitude}
                    << close_array
                << close_document
            << finalize
        );

        static const std::string DISTANCE_KEY = "dist";

        pipe.project(
                document{}
                        << DISTANCE_KEY << open_document
                        << "$toDouble" << generateDistanceFromLocations(user_longitude, user_latitude)
                        << close_document
                << finalize
        );

        auto cursor_result = dummy_collection.aggregate(pipe);

        int num_results = 0;
        for(const auto& result_doc : cursor_result) {
            double extracted_distance = result_doc[DISTANCE_KEY].get_double().value;

            EXPECT_NEAR(expected_distance, extracted_distance, 1);

            num_results++;
        }

        EXPECT_EQ(num_results, 1);
    }

};

TEST_F(GenerateDistanceFromLocationsTesting, allZeros) {
    checkDistance(
        0,
        0,
        0,
        0,
        0
    );
}

TEST_F(GenerateDistanceFromLocationsTesting, zeroDistance) {

    const double longitude = -134.7345;
    const double latitude = 72.7345;

    checkDistance(
            longitude,
            latitude,
            longitude,
            latitude,
            0
    );
}

TEST_F(GenerateDistanceFromLocationsTesting, distanceByLongitudeOnly) {
    const double latitude = -12.345678;

    checkDistance(
            136,
            latitude,
            137,
            latitude,
            67.48
    );
}

TEST_F(GenerateDistanceFromLocationsTesting, distanceByLatitudeOnly) {
    const double longitude = -45.374652;

    checkDistance(
            longitude,
            40.7486,
            longitude,
            40.7886,
            2.7639
    );
}

TEST_F(GenerateDistanceFromLocationsTesting, bigDistance) {
    checkDistance(
            132.374652,
            40.7486,
            0.374652,
            -73.7886,
            9687.1769
    );
}
