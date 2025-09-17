//
// Created by jeremiah on 3/20/21.
//

#include <vector>
#include <thread>
#include <server_initialization_functions.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <user_account_keys.h>
#include <UserAccountStatusEnum.grpc.pb.h>
#include <RequestMessages.grpc.pb.h>

#include "generate_multiple_accounts_multi_thread.h"
#include "generate_multiple_random_accounts.h"
#include "generate_indexing_for_accounts.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

/** NOTE: Min age is not taken into account for activities and so it is possible that
 * a user generates all activities that they are too young for causing verification function
 * to fail.**/
void multiThreadInsertAccounts(int numberAccountsToGenerate, int numberOfThreads, bool generateIndexingAccountValues) {

    unique_phone_numbers.clear();

    setupMongoDBIndexing();

    std::vector<std::thread> threads;

    int numberAccountsPerThread = numberAccountsToGenerate / numberOfThreads;
    int leftOverAccounts = numberAccountsToGenerate % numberOfThreads;

    google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage> grpcCategoriesArray;
    google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage> grpcActivitiesArray;

    for (int i = 0; i < numberOfThreads; i++) {

        int numberAccountsToInsert = numberAccountsPerThread;

        if (leftOverAccounts > 0) {
            numberAccountsToInsert++;
            leftOverAccounts--;
        }

        threads.emplace_back(
            insertRandomAccounts, numberAccountsToInsert, i, true
        );
    }

    for (auto& t : threads) {
        t.join();
    }

    threads.clear();

    if(generateIndexingAccountValues) {

        std::vector<std::string> account_oids;

        {
            mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
            mongocxx::client& mongoCppClient = *mongocxx_pool_entry;
            mongocxx::collection accounts_collection = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME][collection_names::USER_ACCOUNTS_COLLECTION_NAME];

            mongocxx::options::find opts;
            opts.projection(
                    document{}
                        << "_id" << 1
                    << finalize
                    );

            auto cursor = accounts_collection.find(
                document{}
                    << user_account_keys::STATUS << UserAccountStatus::STATUS_ACTIVE
                << finalize,
                opts);

            for(auto& doc : cursor) {
                account_oids.emplace_back(doc["_id"].get_oid().value.to_string());
            }
        }

        numberAccountsPerThread = (int)account_oids.size() / numberOfThreads;
        leftOverAccounts = (int)account_oids.size() % numberOfThreads;
        int counter = 0;
        for (size_t i = 0; i < account_oids.size(); i+=numberAccountsPerThread) {

            int final_index_increment = 0;

            if (leftOverAccounts > 0) {
                final_index_increment++;
                leftOverAccounts--;
            }

            int final_index = (int)i + numberAccountsPerThread + final_index_increment - 1;

            if(final_index>(int)account_oids.size()-1) {
                final_index=(int)account_oids.size()-1;
            }

            threads.emplace_back(
                    generateIndexingForAccounts,
                    account_oids,
                    i,
                    final_index,
                    counter++
                );

            i += final_index_increment;
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    unique_phone_numbers.clear();
}