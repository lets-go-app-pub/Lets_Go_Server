//
// Created by jeremiah on 1/31/22.
//

#include <chrono>
#include <utility_general_functions.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>
#include <user_account_keys.h>
#include <random>
#include "generate_indexing_for_accounts.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

const unsigned long ONE_YEAR_IN_MILLIS = 1000L*60L*60L*24L*365L;
const unsigned long THREE_MONTHS_IN_MILLIS = 1000L*60L*60L*24L*7L*4L*3L;

void generateIndexingForAccounts(
        const std::vector<std::string>& account_oids,
        int start_index_to_update,
        int stop_index_to_update,
        int thread_index
) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;
    mongocxx::collection accounts_collection = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME][collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::vector<std::string> account_oid_copy;
    account_oid_copy.reserve(account_oids.size());
    for(const auto& oid : account_oids) {
        account_oid_copy.emplace_back(oid);
    }

    long seed = thread_index + time(nullptr);
    srand(seed);

    for(int i=start_index_to_update; i<=stop_index_to_update;i++) {

        if(i % 20 == 0) {
            const std::string index_num_str =
                    std::string("Generating index for account: ").
                    append(std::to_string(i-start_index_to_update)).
                    append("/").
                    append(std::to_string(stop_index_to_update-start_index_to_update)).
                    append("\n");

            std::cout << index_num_str;
        }

        const std::string& current_oid = account_oids[i];

        std::chrono::milliseconds last_time_matches_ran = getCurrentTimestamp();
        last_time_matches_ran -= std::chrono::milliseconds{rand() % ONE_YEAR_IN_MILLIS};

        seed = thread_index + time(nullptr);

        std::shuffle(account_oid_copy.begin(), account_oid_copy.end(), std::default_random_engine(seed));

        size_t num_to_update = rand() % (25000 < account_oids.size() ? 25000:account_oids.size());

        bsoncxx::builder::basic::array previously_matched_accounts;

        for(size_t j=0;j<num_to_update;j++) {

            if(current_oid==account_oid_copy[j]) {
                continue;
            }

            std::chrono::milliseconds timestamp = std::chrono::milliseconds{last_time_matches_ran.count() - THREE_MONTHS_IN_MILLIS};

            previously_matched_accounts.append(
                    document{}
                        << user_account_keys::previously_matched_accounts::OID << bsoncxx::oid{account_oid_copy[j]}
                        << user_account_keys::previously_matched_accounts::TIMESTAMP << bsoncxx::types::b_date{timestamp}
                        << user_account_keys::previously_matched_accounts::NUMBER_TIMES_MATCHED << bsoncxx::types::b_int32{(rand() % 20)+1} //don't let this be 0
                    << finalize
            );
        }

        bsoncxx::builder::basic::array blocked_accounts;

        num_to_update = rand() % (account_oid_copy.size()<200?account_oid_copy.size():200);

        std::shuffle(account_oid_copy.begin(), account_oid_copy.end(), std::default_random_engine(seed));

        for(size_t j=0;j<num_to_update;j++) {

            if(current_oid==account_oid_copy[j]) {
                continue;
            }

            std::chrono::milliseconds timestamp = std::chrono::milliseconds{last_time_matches_ran.count() - THREE_MONTHS_IN_MILLIS};

            blocked_accounts.append(
                    document{}
                    << user_account_keys::other_users_blocked::OID_STRING << bsoncxx::types::b_string{account_oid_copy[j]}
                    << user_account_keys::other_users_blocked::TIMESTAMP_BLOCKED << bsoncxx::types::b_date{timestamp}
                    << finalize
                    );
        }

        accounts_collection.update_one(
                document{}
                    << "_id" << bsoncxx::oid{current_oid}
                << finalize,
                document{}
                    << "$set" << open_document
                        << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << bsoncxx::types::b_date{last_time_matches_ran}
                    << close_document
                    << "$push" << open_document
                        << user_account_keys::OTHER_USERS_BLOCKED << open_document
                            << "$each" << blocked_accounts.view()
                        << close_document
                        << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS << open_document
                            << "$each" << previously_matched_accounts.view()
                        << close_document
                    << close_document
                << finalize);

    }

    std::string completed_string = std::string("Thread index_number ").append(std::to_string(thread_index)).append(" completed.\n");
    std::cout << completed_string;
}