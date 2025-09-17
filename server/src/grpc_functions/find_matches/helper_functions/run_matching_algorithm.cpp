//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility_testing_functions.h>
#include <user_account_keys.h>
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

/**
 * Documentation can be found inside [grpc_functions/find_matches/_documentation.md].
 * There is a javascript file inside the C++ project 'GenerateAlgorithmAndStuff' of the algorithm.
**/

bool runMatchingAlgorithm(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        std::optional<mongocxx::cursor>& matching_accounts,
        bool only_match_with_events
) {

#ifdef LG_TESTING
    if(only_match_with_events) {
        algorithm_ran_only_match_with_events = true;
    }
#endif

    bsoncxx::builder::stream::document query_doc;

    bool build_aggregation_successful = buildQueryDocForAggregationPipeline(
            user_account_values,
            user_accounts_collection,
            query_doc,
            only_match_with_events
    );

    mongocxx::pipeline pipeline;

    pipeline.match(query_doc.view());

    generateProjectOnlyUsefulFieldsPipelineStage(&pipeline);

    generateProjectActivitiesPipelineStage(&pipeline, user_account_values);

    generateProjectCategoriesPipelineStage(&pipeline, user_account_values);

    generateAddFieldsInitializeAndCleanTimeFramesPipelineStage(&pipeline, user_account_values);

    generateAddFieldsMergeTimeFramesPipelineStage(&pipeline, user_account_values);

    generateAddFieldsTimeFrameTimeStatisticsPipelineStage(&pipeline, user_account_values);

    if(!generateProjectCalculateFinalPointIntermediatesPipelineStage(&pipeline, user_account_values)) //if an error occurred
        return false;

    generateAddFieldsFinalPoints(&pipeline);

    generateSortByTotalPointsPipelineStage(&pipeline);

    generateLimitPipelineStage(&pipeline);

    generateProjectCleanUpPipelineStage(&pipeline, user_account_values);

    if(!build_aggregation_successful) {
        return false;
    }

    mongocxx::options::aggregate aggregation_options;

    //Pipeline stages have a limit of 100 megabytes of RAM. If a stage exceeds this limit, MongoDB will produce an error. To allow for the
    // handling of large datasets, use the allowDiskUse option to enable aggregation pipeline stages to write data to temporary files.
    //The aggregation_options should not be used unless necessary, so it doesn't hurt to have it on. Otherwise, could randomly get errors.
    aggregation_options.allow_disk_use(true);

    //During testing the database must have very fast reads after the writes. There is always the chance that
    // propagation to the secondaries has not yet occurred. So while this should be a benefit for production,
    // it can make tests flaky.
#ifndef LG_TESTING
    //max staleness of data set to 15 minutes
    static std::chrono::minutes MAX_STALENESS{15L};

    //prefer a secondary to try to spread the resources towards servers other than the primary server
    //NOTE: http://mongocxx.org/api/current/classmongocxx_1_1options_1_1aggregate.html#a26201c7fa4bc7bf910d530679fe087f6
    // lists read_preference as deprecated, however this doesn't seem to be the case when used in this manner

    mongocxx::read_preference read_pref;
    read_pref.max_staleness(MAX_STALENESS);

    read_pref.mode(mongocxx::read_preference::read_mode::k_secondary_preferred);

    aggregation_options.read_preference(read_pref);
#endif

    aggregation_options.max_time(matching_algorithm::TIMEOUT_FOR_ALGORITHM_AGGREGATION);

    // Force query use algorithm index named ALGORITHM_INDEX_NAME.
    // NOTE: This is a reasonable alternative to an index filter for a query shape. MongoDB docs say to
    // 'use index filters sparingly' and it is not needed here, so using a hint instead.
    aggregation_options.hint(mongocxx::hint(user_account_collection_index::ALGORITHM_INDEX_NAME));

    try {

        //NOTE: Purposefully not running this in the session. There are two reasons for that. First this allows the
        // secondaries to be used to run the algorithm. Second it does not 'need' to be part of the transaction and
        // (although I don't know this for certain) running it in the transaction may slow it down.
        matching_accounts = user_accounts_collection.aggregate(
                //*session,
                pipeline,
                aggregation_options
                );

        return true;
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional <std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );

        return false;
    }
}
