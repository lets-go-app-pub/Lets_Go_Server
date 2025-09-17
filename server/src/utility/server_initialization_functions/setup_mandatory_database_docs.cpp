//
// Created by jeremiah on 3/20/21.
//

#include <fstream>

#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include "connection_pool_global_variable.h"
#include "server_initialization_functions.h"


#include "AdminLevelEnum.grpc.pb.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_room_info_keys.h"
#include "activities_info_keys.h"
#include "admin_account_keys.h"
#include "event_admin_values.h"
#include "assert_macro.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setupMandatoryDatabaseDocs() {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_number_collection = chat_rooms_db[collection_names::CHAT_ROOM_INFO];

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];
    mongocxx::collection admin_accounts_collection = accounts_db[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    //NOTE: this is perfectly fine to fail here (it will throw an exception if it exists already), it simply must be started
    //upsert chat room number document if it does not exist
    try {
        chat_room_number_collection.insert_one(
            document{}
                << "_id" << chat_room_info_keys::ID
                << chat_room_info_keys::PREVIOUSLY_USED_CHAT_ROOM_NUMBER << bsoncxx::types::b_int64{1}
            << finalize);
    }
    catch (const mongocxx::exception& e) {}

    //NOTE: It is perfectly fine to fail here (it will throw an exception if it exists already), they simply must exist
    // for the server to run properly.
    try {

        activities_info_collection.insert_one(
            document{}
                << "_id" << activities_info_keys::ID
                << activities_info_keys::CATEGORIES << open_array
                << close_array
                << activities_info_keys::ACTIVITIES << open_array
                << close_array
            << finalize);
    }
    catch (const mongocxx::exception& e) {}

    //NOTE: It is perfectly fine to fail here (it will throw an exception if it exists already), they simply must exist
    // for the server to run properly.
    std::int64_t document_count;
    try {
        document_count = user_accounts_collection.count_documents(
                document{}
                        << "_id" << event_admin_values::OID
                << finalize
        );
    }
    catch (const mongocxx::exception& e) {
        std::cout << "Exception occurred when checking event admin account.\nMessage: " << e.what() << '\n';
        throw e;
    }

    if(document_count < 1) {
        createAndStoreEventAdminAccount();
    }

#ifndef _RELEASE

    auto negative_one_date = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    std::vector<bsoncxx::document::value> documents;

    documents.emplace_back(
        document{}
            << admin_account_key::NAME << "Testing"
            << admin_account_key::PASSWORD << "abc"
            << admin_account_key::PRIVILEGE_LEVEL << AdminLevelEnum::PRIMARY_DEVELOPER

            << admin_account_key::LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS << negative_one_date

            << admin_account_key::LAST_TIME_EXTRACTED_REPORTS << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_BLOCKS << negative_one_date

            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG << negative_one_date

            << admin_account_key::NUMBER_FEEDBACK_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
            << admin_account_key::NUMBER_REPORTS_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
        << finalize
    );

    //NOTE: It is perfectly fine to fail here (it will throw an exception if it exists already), they simply must exist
    // for the server to run properly.
    try {
        admin_accounts_collection.insert_many(documents);
    }
    catch (const mongocxx::exception& e) {}

#endif

}