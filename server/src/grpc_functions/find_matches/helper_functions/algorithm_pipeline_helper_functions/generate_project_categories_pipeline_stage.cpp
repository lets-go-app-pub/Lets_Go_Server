//
// Created by jeremiah on 8/8/21.
//

#include <bsoncxx/builder/basic/array.hpp>
#include <AccountCategoryEnum.grpc.pb.h>

#include "algorithm_pipeline_helper_functions.h"

#include "user_account_keys.h"
#include "algorithm_pipeline_field_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void generateProjectActivitiesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
) {
    auto userActivitiesMongoDBArray = bsoncxx::builder::basic::array{};

    for (const ActivityStruct& i : userAccountValues.user_activities) {
        userActivitiesMongoDBArray.append(i.activityIndex);
    }

    stages->project(document{}
        << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << 1
        << user_account_keys::ACCOUNT_TYPE << 1
        << user_account_keys::LOCATION << 1
        << user_account_keys::CATEGORIES << 1
        << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHING_ACTIVITIES_FIELD << open_document
            << "$filter" << open_document
                << "input" << '$' + user_account_keys::CATEGORIES
                << "cond" << open_document
                    << "$and" << open_array
                        << open_document
                            << "$eq" << open_array
                                << "$$this." + user_account_keys::categories::TYPE
                                << AccountCategoryType::ACTIVITY_TYPE
                            << close_array
                        << close_document

                        << open_document
                            << "$in" << open_array
                                << "$$this." + user_account_keys::categories::INDEX_VALUE
                                << userActivitiesMongoDBArray
                            << close_array
                        << close_document

                    << close_array
                << close_document
            << close_document
        << close_document
    << finalize);
}