//
// Created by jeremiah on 3/20/21.
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

void generateProjectCategoriesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
) {

    auto userCategoriesMongoDBArray = bsoncxx::builder::basic::array{};

    for (const ActivityStruct& i : userAccountValues.user_categories) {
        userCategoriesMongoDBArray.append(i.activityIndex);
    }

    stages->project(
        document{}
            << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << 1
            << user_account_keys::ACCOUNT_TYPE << 1
            << user_account_keys::LOCATION << 1
            << user_account_keys::CATEGORIES << open_document
                << "$cond" << open_document
                    << "if" << open_document
                        << "$and" << open_array
                            << bsoncxx::types::b_bool{userAccountValues.algorithm_search_options == AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY}
                            << open_document
                                << "$eq" << open_array
                                    << open_document
                                        << "$size" << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHING_ACTIVITIES_FIELD
                                    << close_document
                                    << 0
                                << close_array
                            << close_document
                        << close_array
                    << close_document
                    << "then" << open_document
                        << "$filter" << open_document
                            << "input" << '$' + user_account_keys::CATEGORIES
                            << "cond" << open_document
                                << "$and" << open_array
                                    << open_document
                                        << "$eq" << open_array
                                            << "$$this." + user_account_keys::categories::TYPE
                                            << AccountCategoryType::CATEGORY_TYPE
                                        << close_array
                                    << close_document

                                    << open_document
                                        << "$in" << open_array
                                            << "$$this." + user_account_keys::categories::INDEX_VALUE
                                            << userCategoriesMongoDBArray
                                        << close_array
                                    << close_document

                                << close_array
                            << close_document
                        << close_document
                    << close_document
                    << "else" << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHING_ACTIVITIES_FIELD
                << close_document
            << close_document
        << finalize
    );
}