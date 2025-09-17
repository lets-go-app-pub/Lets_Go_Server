//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include <mongocxx/pipeline.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include "find_matches_helper_objects.h"

bsoncxx::builder::stream::document generateConditionalSwitchesForCategories(
        AlgorithmSearchOptions algorithm_search_options,
        const bsoncxx::array::view& activities_total_time_cases_array,
        const std::function<bsoncxx::builder::stream::document(const bsoncxx::array::view&)>& generate_switch_doc,
        const std::function<bsoncxx::builder::basic::array()>& build_categories_cases
);

bsoncxx::document::value generateDistanceFromLocations(
        const double& user_longitude,
        const double& user_latitude
);

//NOTE: builds query document excluding the activities and/or categories to match
void generateInitialQueryForPipeline(
        UserAccountValues& user_account_values,
        bsoncxx::builder::stream::document& query_doc,
        bool only_match_with_events
);

//NOTE: adds activities or categories array to match to the query document
bool generateCategoriesActivitiesMatching(
        bsoncxx::builder::stream::document& query_doc,
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection
);

//NOTE: refer to javascript file for pipeline details
void generateProjectOnlyUsefulFieldsPipelineStage(mongocxx::pipeline* stages);

//NOTE: refer to javascript file for pipeline details
void generateProjectActivitiesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
);

//NOTE: refer to javascript file for pipeline details
void generateProjectCategoriesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
);

//NOTE: refer to javascript file for pipeline details
void generateAddFieldsInitializeAndCleanTimeFramesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
);

//NOTE: refer to javascript file for pipeline details
void generateAddFieldsMergeTimeFramesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
);

//NOTE: refer to javascript file for pipeline details
void generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
);

//NOTE: refer to javascript file for pipeline details
bool generateProjectCalculateFinalPointIntermediatesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
);

//NOTE: refer to javascript file for pipeline details
void generateAddFieldsFinalPoints(mongocxx::pipeline* stages);

//NOTE: refer to javascript file for pipeline details
void generateProjectCleanUpPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
);

//NOTE: refer to javascript file for pipeline details
void generateSortByTotalPointsPipelineStage(mongocxx::pipeline* stages);

//NOTE: when limit is after sort in an aggregation pipeline mongodb will combine them for efficiency
//NOTE: refer to javascript file for pipeline details
void generateLimitPipelineStage(mongocxx::pipeline* stages);


