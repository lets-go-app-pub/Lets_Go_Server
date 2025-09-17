//
// Created by jeremiah on 2/7/22.
//

#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <AccountCategoryEnum.grpc.pb.h>
#include <user_account_keys.h>
#include <utility_testing_functions.h>
#include <specific_match_queries/specific_match_queries.h>

#include "algorithm_pipeline_helper_functions.h"
#include "store_mongoDB_error_and_exception.h"

#include "database_names.h"
#include "collection_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

template <AccountCategoryType category_type_to_match>
void appendMatchActivitiesOrCategoriesToDocument(
        bsoncxx::builder::stream::document& query_doc,
        UserAccountValues& user_account_values
) {

    bsoncxx::builder::basic::array user_activities_or_categories_array{};

    if (category_type_to_match == AccountCategoryType::ACTIVITY_TYPE) {
        for (const ActivityStruct& activity : user_account_values.user_activities) {
            user_activities_or_categories_array.append(activity.activityIndex);
        }
    } else { //MATCH_CATEGORIES_AND_ACTIVITIES
        for (const ActivityStruct& category : user_account_values.user_categories) {
            user_activities_or_categories_array.append(category.activityIndex);
        }
    }

    atLeastOneCategoryMatches(
            query_doc,
            user_activities_or_categories_array,
            category_type_to_match
    );
}

bool generateCategoriesActivitiesMatching(
        bsoncxx::builder::stream::document& query_doc,
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection
        ) {

    timespec check_for_activities_start_ts, check_for_activities_stop_ts;
    clock_gettime(CLOCK_REALTIME, &check_for_activities_start_ts);

    //This will force the algorithm to only search for categories if there are no matching activities. It can cut
    // down significantly on the number of matches in favor of more relevant ones.
    if(user_account_values.algorithm_search_options == AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY) { //only matching activities
            appendMatchActivitiesOrCategoriesToDocument<AccountCategoryType::ACTIVITY_TYPE>(
                    query_doc,
                    user_account_values
            );
    } else { //match by activity and category
        bsoncxx::builder::stream::document query_only_activities;
        appendMatchActivitiesOrCategoriesToDocument<AccountCategoryType::ACTIVITY_TYPE>(
                query_only_activities,
                user_account_values
        );

        bsoncxx::builder::stream::document count_docs_query;
        count_docs_query
            << bsoncxx::builder::concatenate(query_doc.view())
            << bsoncxx::builder::concatenate(query_only_activities.view());

        long num_matched;
        try {

            mongocxx::options::count opts;
            opts.limit(1);
            opts.hint(mongocxx::hint(user_account_collection_index::ALGORITHM_INDEX_NAME));

            //NOTE: No need to include this in the transaction. It is just for estimation purposes.
            num_matched = user_accounts_collection.count_documents(count_docs_query.view(), opts);
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME);

            return false;
        }

        if(num_matched == 1) { //an activity matched, only match with activities
            query_doc << bsoncxx::builder::concatenate(query_only_activities.view());
            user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;
        } else { //no activities found, match with categories
            appendMatchActivitiesOrCategoriesToDocument<AccountCategoryType::CATEGORY_TYPE>(
                    query_doc,
                    user_account_values
            );
            //If the user can only match by categories, guarantee that the target is also willing to match by category.
            //NOTE: This value is not part of the index.
            query_doc
                << user_account_keys::SEARCH_BY_OPTIONS << AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;
        }

        clock_gettime(CLOCK_REALTIME, &check_for_activities_stop_ts);
        user_account_values.check_for_activities_run_time_ns = std::chrono::nanoseconds{check_for_activities_stop_ts.tv_nsec - check_for_activities_start_ts.tv_nsec};

    }

    return true;
}


