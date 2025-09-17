//
// Created by jeremiah on 8/31/22.
//

#include "algorithm_pipeline_helper_functions.h"
#include "AccountCategoryEnum.pb.h"
#include "algorithm_pipeline_field_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bsoncxx::builder::stream::document generateConditionalSwitchesForCategories(
        AlgorithmSearchOptions algorithm_search_options,
        const bsoncxx::array::view& activities_total_time_cases_array,
        const std::function<bsoncxx::builder::stream::document(const bsoncxx::array::view&)>& generate_switch_doc,
        const std::function<bsoncxx::builder::basic::array()>& build_categories_cases
        ) {

    //Activity matches must be included regardless. It is true that the query will only search for matching activities
    // if a single matching activity is found. However, it is possible that between NO matching activities found and
    // the aggregation operation itself a matching activity could be inserted.
    if(algorithm_search_options == AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY) {
        return generate_switch_doc(activities_total_time_cases_array);
    } else {

        bsoncxx::builder::stream::document activities_and_categories_switches;

        auto categories_total_time_cases_array = build_categories_cases();

        activities_and_categories_switches
            << "$cond" << open_document
                << "if" << open_document
                    << "$eq" << open_array
                        << std::string("$$").append(algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY).append(".").append(user_account_keys::categories::TYPE)
                        << AccountCategoryType::CATEGORY_TYPE
                    << close_array
                << close_document
                << "then" << generate_switch_doc(categories_total_time_cases_array.view())
                << "else" << generate_switch_doc(activities_total_time_cases_array)
            << close_document;

        return activities_and_categories_switches;
    }

}