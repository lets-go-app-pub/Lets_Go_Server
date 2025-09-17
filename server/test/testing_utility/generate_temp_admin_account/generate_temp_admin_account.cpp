//
// Created by jeremiah on 7/28/22.
//

#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <user_pictures_keys.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>
#include <account_recovery_keys.h>

#include "generate_temp_admin_account.h"
#include "admin_account_keys.h"
#include "database_names.h"
#include "collection_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

std::string createTempAdminAccount(AdminLevelEnum adminPrivilegeLevel) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection admin_accounts_collection = accounts_db[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];

    auto negative_one_date = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};

    mongocxx::options::find_one_and_update opts;
    opts.upsert(true);
    opts.return_document(mongocxx::options::return_document::k_after);
    opts.projection(
        document{}
            << "_id" << 1
        << finalize
    );

    auto find_and_update_result = admin_accounts_collection.find_one_and_update(
        document{}
                << admin_account_key::NAME << TEMP_ADMIN_ACCOUNT_NAME
                << admin_account_key::PASSWORD << TEMP_ADMIN_ACCOUNT_PASSWORD
        << finalize,
        document{}
            << "$setOnInsert" << open_document
                << admin_account_key::NAME << TEMP_ADMIN_ACCOUNT_NAME
                << admin_account_key::PASSWORD << TEMP_ADMIN_ACCOUNT_PASSWORD
            << close_document
            << "$set" << open_document
                << admin_account_key::PRIVILEGE_LEVEL << adminPrivilegeLevel

                << admin_account_key::LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS << negative_one_date

                << admin_account_key::LAST_TIME_EXTRACTED_REPORTS << negative_one_date
                << admin_account_key::LAST_TIME_EXTRACTED_BLOCKS << negative_one_date

                << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY << negative_one_date
                << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER << negative_one_date
                << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG << negative_one_date

                << admin_account_key::NUMBER_FEEDBACK_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
                << admin_account_key::NUMBER_REPORTS_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
            << close_document
        << finalize,
        opts
    );

    std::string upserted_id;

    EXPECT_TRUE(find_and_update_result);
    if(find_and_update_result) {
        upserted_id = find_and_update_result->view()["_id"].get_oid().value.to_string();
    }

    return upserted_id;
}
