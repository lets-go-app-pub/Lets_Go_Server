//
// Created by jeremiah on 3/4/23.
//

#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility_testing_functions.h>
#include <user_account_keys.h>
#include <AccountCategoryEnum.grpc.pb.h>
#include <matching_algorithm.h>

#include "find_matches_helper_functions.h"
#include "algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

AlgorithmReturnValues runMatchingAlgorithmAndReturnResults(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        bool only_match_with_events
) {

    //used to store the algorithm matches
    std::optional<mongocxx::cursor> matching_accounts;

    timespec aggregation_start_ts, aggregation_stop_ts;

    clock_gettime(CLOCK_REALTIME, &aggregation_start_ts);

    //This try block must be wrapped around this entire block, the pipeline is not actually run
    // until the if() statement is run. Possibly because the document itself is so large, or
    // an optimization to not run the aggregation command until needed.
    try {

        if (!runMatchingAlgorithm(
                user_account_values,
                user_accounts_collection,
                matching_accounts,
                only_match_with_events
        )
                ) {
            return {};
        }

        //if cursor exists and is not empty
        if (matching_accounts && (*matching_accounts).begin() != (*matching_accounts).end()) {

            //NOTE: This is the 'aggregation run' because until the cursor is accessed above, there is
            // no guarantee that the database operation has been run.
            clock_gettime(CLOCK_REALTIME, &aggregation_stop_ts);

            user_account_values.algorithm_run_time_ns = std::chrono::nanoseconds{
                    aggregation_stop_ts.tv_nsec - aggregation_start_ts.tv_nsec
            };

#ifdef _DEBUG
            std::cout << "'Algorithm Time' CLOCK_REALTIME: " << durationAsNanoSeconds(aggregation_stop_ts,
                                                                                      aggregation_start_ts) << '\n';
#endif

            if (!extractAndSaveMatchingResults(
                    user_account_values,
                    matching_accounts)
            ) { //if an error occurred in the function
                return {};
            }

            return AlgorithmReturnValues(successfully_extracted);
        } else {

            clock_gettime(CLOCK_REALTIME, &aggregation_stop_ts);

#ifdef _DEBUG
            std::cout << "'Aggregation Run' CLOCK_REALTIME: " << durationAsNanoSeconds(aggregation_stop_ts,
                                                                                       aggregation_start_ts) << '\n';
#endif

            if (matching_accounts) {
                std::cout << "matching_accounts was empty\n";
            } else {
                std::cout << "matching_accounts did not exist\n";
            }

            return {
                    no_matches_found,
                    matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND
            };
        }
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );

        return {};
    }
}