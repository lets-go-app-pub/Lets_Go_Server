//
// Created by jeremiah on 10/12/22.
//

#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include  <mongocxx/client.hpp>
#include  <mongocxx/database.hpp>
#include  <mongocxx/collection.hpp>

#include "chat_room_shared_keys.h"
#include "connection_pool_global_variable.h"
#include "generate_multiple_random_accounts.h"
#include "global_bsoncxx_docs.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class BuildAgeRangeCheckerTesting : public testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database test_db = mongo_cpp_client["test_db"];
    mongocxx::collection test_collection = test_db["test_col"];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
        test_db.drop();
    }
};

TEST_F(BuildAgeRangeCheckerTesting, checkVariousAgeAndAgeRangeCombinations) {

    using namespace server_parameter_restrictions;

    for(int user_age = LOWEST_ALLOWED_AGE; user_age <= HIGHEST_ALLOWED_AGE; ++user_age) {

        for(int i = 0; i < 8; ++i) {

            int min_age;
            int max_age;

            switch (i) {
                case 0:
                    min_age = LOWEST_ALLOWED_AGE;
                    max_age = HIGHEST_ALLOWED_AGE;
                    break;
                case 1:
                    min_age = LOWEST_ALLOWED_AGE;
                    max_age = LOWEST_ALLOWED_AGE;
                    break;
                case 2:
                    min_age = HIGHEST_ALLOWED_AGE;
                    max_age = HIGHEST_ALLOWED_AGE;
                    break;
                case 3:
                    min_age = user_age + 1;
                    max_age = user_age - 1;
                    break;
                case 4:
                    min_age = user_age + 4;
                    max_age = user_age - 4;
                    break;
                case 5:
                    min_age = user_age + 4;
                    max_age = user_age - 4;
                    break;
                case 6:
                    min_age = user_age + rand() % 50;
                    max_age = user_age + rand() % 50;
                    break;
                case 7:
                    min_age = user_age - rand() % 50;
                    max_age = user_age - rand() % 50;
                    break;
                default:
                    min_age = -1;
                    max_age = -1;
                    break;
            }

            //function parameters are expected to be in range [LOWEST_ALLOWED_AGE, HIGHEST_ALLOWED_AGE]
            min_age = min_age > HIGHEST_ALLOWED_AGE ? HIGHEST_ALLOWED_AGE : min_age;
            min_age = min_age < LOWEST_ALLOWED_AGE ? LOWEST_ALLOWED_AGE : min_age;

            max_age = max_age > HIGHEST_ALLOWED_AGE ? HIGHEST_ALLOWED_AGE : max_age;
            max_age = max_age < LOWEST_ALLOWED_AGE ? LOWEST_ALLOWED_AGE : max_age;

            auto insert_result = test_collection.insert_one(
                    document{}
                            << user_account_keys::AGE_RANGE << -1
                            << user_account_keys::AGE << user_age
                            << finalize
            );

            ASSERT_EQ(insert_result->result().inserted_count(), 1);

            mongocxx::pipeline pipe;

            pipe.project(
                    document{}
                            << user_account_keys::AGE_RANGE << buildAgeRangeChecker(min_age, max_age)
                            << finalize
            );

            mongocxx::cursor cursor = test_collection.aggregate(pipe);

            bsoncxx::document::view age_range_doc;
            int cursor_size = 0;
            for (const auto& doc: cursor) {
                age_range_doc = doc[user_account_keys::AGE_RANGE].get_document().value;
                cursor_size++;
            }

            EXPECT_EQ(cursor_size, 1);

            const int extracted_min_age = age_range_doc[user_account_keys::age_range::MIN].get_int32().value;
            const int extracted_max_age = age_range_doc[user_account_keys::age_range::MAX].get_int32().value;

            buildAgeRangeChecker(user_age, min_age, max_age);

            EXPECT_EQ(extracted_min_age, min_age);
            EXPECT_EQ(extracted_max_age, max_age);

            if(extracted_min_age != min_age
                || extracted_max_age != max_age) {
                std::cout << "failed_age: " << user_age << '\n'
                    << "min_age: " << min_age << " max_age: " << max_age << '\n'
                    << "max_age: " << max_age << " extracted_max_age: " << extracted_max_age << '\n';
            }

            test_collection.drop();
        }
    }
}

