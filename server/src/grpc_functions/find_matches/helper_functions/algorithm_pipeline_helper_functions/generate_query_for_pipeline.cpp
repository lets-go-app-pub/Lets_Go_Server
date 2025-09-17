//
// Created by jeremiah on 3/20/21.
//

#include <bsoncxx/builder/basic/array.hpp>
#include <build_validate_match_document.h>
#include <utility_general_functions.h>
#include <user_account_keys.h>

#include "algorithm_pipeline_helper_functions.h"
#include "general_values.h"
#include "server_parameter_restrictions.h"


//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void generateInitialQueryForPipeline(
        UserAccountValues& user_account_values,
        bsoncxx::builder::stream::document& query_doc,
        bool only_match_with_events
) {

    //generate document to filter for valid matches
    generatePipelineMatchDocument(
            query_doc,
            user_account_values.age,
            user_account_values.min_age_range,
            user_account_values.max_age_range,
            user_account_values.gender,
            user_account_values.genders_to_match_with_bson_array,
            user_account_values.user_matches_with_everyone,
            user_account_values.user_account_oid,
            user_account_values.current_timestamp,
            only_match_with_events
    );

    bsoncxx::builder::basic::array restricted_accounts_mongo_array{};
    bool array_has_element_inside = false;
    for (const bsoncxx::oid& o : user_account_values.restricted_accounts) {
        restricted_accounts_mongo_array.append(document{} << "_id" << o << finalize);
        array_has_element_inside = true;
    }

    //$nor cannot be empty (it should always have the current_user_oid inside it)
    if(array_has_element_inside) {
        //append restricted accounts
        //$nor is faster than $nin
        query_doc
                << "$nor" << restricted_accounts_mongo_array;
    }

    double max_distance;

    if(only_match_with_events) {
        max_distance = server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE;
    } else {
        max_distance = user_account_values.max_distance;
    }

    max_distance = max_distance / 3963.2; //convert degrees to miles (from mongoDB site)

    //A custom function is also built (see after initial query in aggregation pipeline) which will calculate the distance for each
    // document individually.  However, it does not use the index and so it MUST look at each document individually. Therefore,
    // $geoWithin is used ($geoNear is slower even though the aggregation stage will output the distance) to shorten the list
    // before the documents are iterated through.
    query_doc
        << user_account_keys::LOCATION << open_document
            << "$geoWithin" << open_document
                << "$centerSphere" << open_array
                    << open_array
                        << user_account_values.longitude
                        << user_account_values.latitude
                    << close_array
                    << max_distance
                << close_array
            << close_document
        << close_document;

    //If only matching events, the MAX_DISTANCE of the event does not matter.
    if(!only_match_with_events) {

        //NOTE: This is incapable of taking advantage of an index when done this way (the optimizations_for_later text file
        // have a NOTE to look into geospatial indexes). However, the compound index for the algorithm will still be used.
        query_doc
                << "$expr" << open_document
                    << "$gte" << open_array
                        << '$' + user_account_keys::MAX_DISTANCE
                        << generateDistanceFromLocations(user_account_values.longitude, user_account_values.latitude)
                    << close_array
                << close_document;
    }

}