//
// Created by jeremiah on 3/20/21.
//

#include <AccountCategoryEnum.grpc.pb.h>
#include "algorithm_pipeline_helper_functions.h"

#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "algorithm_pipeline_field_names.h"
#include "matching_algorithm.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool generateProjectCalculateFinalPointIntermediatesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
) {

    bsoncxx::array::view previouslyMatchedAccounts;
    auto previouslyMatchedAccountsElement = userAccountValues.user_account_doc_view[user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS];
    if (previouslyMatchedAccountsElement
        && previouslyMatchedAccountsElement.type() == bsoncxx::type::k_array) { //if element exists and is type array
        previouslyMatchedAccounts = previouslyMatchedAccountsElement.get_array().value;
    } else { //if element does not exist or is not type array
        logElementError(__LINE__, __FILE__, previouslyMatchedAccountsElement,
                        userAccountValues.user_account_doc_view, bsoncxx::type::k_array,
                        user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    stages->project(document{}
        << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << 1
        << user_account_keys::ACCOUNT_TYPE << 1
        << user_account_keys::LOCATION << 1
        << algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FALL_OFF_VAR << open_document
            << "$let" << open_document
                << "vars" << open_document
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHED_ID_VAR << open_document
                        << "$reduce" << open_document
                            << "input" << previouslyMatchedAccounts
                            << "initialValue" << false
                            << "in" << open_document
                                << "$cond" << open_document
                                    << "if" << open_document
                                        << "$eq" << open_array
                                            << "$$this." + user_account_keys::previously_matched_accounts::OID
                                            << "$_id"
                                        << close_array
                                    << close_document
                                    << "then" << "$$this"
                                    << "else" << "$$value"
                                << close_document
                            << close_document
                        << close_document
                    << close_document
                << close_document
                << "in" << open_document
                    << "$cond" << open_document
                        << "if" << open_document
                            << "$ne" << open_array
                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHED_ID_VAR
                                << false
                            << close_array
                        << close_document
                        << "then" << open_document
                            << "$let" << open_document
                                << "vars" << open_document
                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_NUMERATOR_VAR << open_document
                                        << "$subtract" << open_array
                                            << bsoncxx::types::b_int64{userAccountValues.current_timestamp.count()}
                                            << open_document
                                                << "$toLong" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHED_ID_VAR + "." + user_account_keys::previously_matched_accounts::TIMESTAMP
                                            << close_document

                                        << close_array
                                    << close_document
                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_DENOMINATOR_VAR << open_document
                                        << "$multiply" << open_array
                                            << bsoncxx::types::b_int64{matching_algorithm::PREVIOUSLY_MATCHED_FALLOFF_TIME.count()}
                                            << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCHED_ID_VAR + "." + user_account_keys::previously_matched_accounts::NUMBER_TIMES_MATCHED
                                        << close_array
                                    << close_document
                                << close_document
                                << "in" << open_document
                                    << "$cond" << open_document
                                        << "if" << open_document
                                            << "$and" << open_array
                                                << open_document
                                                    << "$gt" << open_array
                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_DENOMINATOR_VAR
                                                        << 0
                                                    << close_array
                                                << close_document

                                                << open_document
                                                    << "$lt" << open_array
                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_NUMERATOR_VAR
                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_DENOMINATOR_VAR
                                                    << close_array
                                                << close_document

                                            << close_array
                                        << close_document
                                        << "then" << open_document
                                            << "$multiply" << open_array
                                                << -1
                                                << matching_algorithm::PREVIOUSLY_MATCHED_WEIGHT
                                                << open_document
                                                    << "$subtract" << open_array
                                                        << 1
                                                        << open_document
                                                            << "$divide" << open_array
                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_NUMERATOR_VAR
                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_DENOMINATOR_VAR
                                                            << close_array
                                                        << close_document

                                                    << close_array
                                                << close_document

                                            << close_array
                                        << close_document
                                        << "else" << bsoncxx::types::b_double{ 0 }
                                    << close_document
                                << close_document
                            << close_document
                        << close_document
                        << "else" << bsoncxx::types::b_double{ 0 }
                    << close_document
                << close_document
            << close_document
        << close_document<< algorithm_pipeline_field_names::MONGO_PIPELINE_INACTIVITY_POINTS_TO_SUBTRACT_KEY << open_document
            << "$cond" << open_document
                << "if" << open_document
                    << "$eq" << open_array
                        << open_document
                            << "$toLong" << '$' + user_account_keys::LAST_TIME_FIND_MATCHES_RAN
                        << close_document

                        << bsoncxx::types::b_int64{general_values::EVENT_DEFAULT_LAST_TIME_FIND_MATCHES_RAN}
                    << close_array
                << close_document
                << "then" << bsoncxx::types::b_double{ 0 }
                << "else" << open_document
                    << "$multiply" << open_array
                        << open_document
                            << "$subtract" << open_array
                                << bsoncxx::types::b_int64{userAccountValues.current_timestamp.count()}
                                << open_document
                                    << "$toLong" << '$' + user_account_keys::LAST_TIME_FIND_MATCHES_RAN
                                << close_document

                            << close_array
                        << close_document

                        << matching_algorithm::INACTIVE_ACCOUNT_WEIGHT
                        << -1
                    << close_array
                << close_document
            << close_document
        << close_document<< algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_STATS_VAR << open_document
            << "$reduce" << open_document
                << "input" << '$' + user_account_keys::CATEGORIES
                << "initialValue" << open_document
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << bsoncxx::types::b_int64{userAccountValues.end_of_time_frame_timestamp.count()}
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY << 0
                    << user_account_keys::accounts_list::ACTIVITY_STATISTICS << open_array

                    << close_array
                << close_document
                << "in" << open_document
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << open_document
                        << "$cond" << open_document
                            << "if" << open_document
                                << "$lt" << open_array
                                    << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                    << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                << close_array
                            << close_document
                            << "then" << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                            << "else" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                        << close_document
                    << close_document
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY << open_document
                        << "$add" << open_array
                            << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY
                            << open_document
                                << "$let" << open_document
                                    << "vars" << open_document
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_TIME_WEIGHT_VAR << open_document
                                            << "$cond" << open_document
                                                << "if" << open_document
                                                    << "$eq" << open_array
                                                        << "$$this." + user_account_keys::categories::TYPE
                                                        << AccountCategoryType::CATEGORY_TYPE
                                                    << close_array
                                                << close_document
                                                << "then" << matching_algorithm::OVERLAPPING_CATEGORY_TIMES_WEIGHT
                                                << "else" << matching_algorithm::OVERLAPPING_ACTIVITY_TIMES_WEIGHT
                                            << close_document
                                        << close_document
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_SHORT_OVERLAP_TIME_WEIGHT_VAR << open_document
                                            << "$cond" << open_document
                                                << "if" << open_document
                                                    << "$eq" << open_array
                                                        << "$$this." + user_account_keys::categories::TYPE
                                                        << AccountCategoryType::CATEGORY_TYPE
                                                    << close_array
                                                << close_document
                                                << "then" << matching_algorithm::SHORT_TIMEFRAME_CATEGORY_OVERLAP_WEIGHT
                                                << "else" << matching_algorithm::SHORT_TIMEFRAME_ACTIVITY_OVERLAP_WEIGHT
                                            << close_document
                                        << close_document
                                    << close_document
                                    << "in" << open_document
                                        << "$cond" << open_document
                                            << "if" << open_document
                                                << "$and" << open_array
                                                    << open_document
                                                        << "$gt" << open_array
                                                            << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                                                            << 0
                                                        << close_array
                                                    << close_document

                                                    << open_document
                                                        << "$gt" << open_array
                                                            << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR
                                                            << 0
                                                        << close_array
                                                    << close_document

                                                    << open_document
                                                        << "$gt" << open_array
                                                            << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
                                                            << 0
                                                        << close_array
                                                    << close_document

                                                << close_array
                                            << close_document
                                            << "then" << open_document
                                                << "$let" << open_document
                                                    << "vars" << open_document
                                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_RATIO_VAR << open_document
                                                            << "$cond" << open_document
                                                                << "if" << open_document
                                                                    << "$gt" << open_array
                                                                        << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR
                                                                        << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
                                                                    << close_array
                                                                << close_document
                                                                << "then" << open_document
                                                                    << "$divide" << open_array
                                                                        << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                                                                        << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR
                                                                    << close_array
                                                                << close_document
                                                                << "else" << open_document
                                                                    << "$divide" << open_array
                                                                        << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                                                                        << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
                                                                    << close_array
                                                                << close_document
                                                            << close_document
                                                        << close_document
                                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_SHORT_OVERLAP_POINTS_VAR << open_document
                                                            << "$multiply" << open_array
                                                                << open_document
                                                                    << "$subtract" << open_array
                                                                        << 1
                                                                        << open_document
                                                                            << "$divide" << open_array
                                                                                << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                                                                                << bsoncxx::types::b_int64{userAccountValues.end_of_time_frame_timestamp.count() - userAccountValues.earliest_time_frame_start_timestamp.count()}
                                                                            << close_array
                                                                        << close_document

                                                                    << close_array
                                                                << close_document

                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_SHORT_OVERLAP_TIME_WEIGHT_VAR
                                                            << close_array
                                                        << close_document
                                                    << close_document
                                                    << "in" << open_document
                                                        << "$multiply" << open_array
                                                            << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_RATIO_VAR
                                                            << open_document
                                                                << "$add" << open_array
                                                                    << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_SHORT_OVERLAP_POINTS_VAR
                                                                    << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_TIME_WEIGHT_VAR
                                                                << close_array
                                                            << close_document

                                                        << close_array
                                                    << close_document
                                                << close_document
                                            << close_document
                                            << "else" << 0
                                        << close_document
                                    << close_document
                                << close_document
                            << close_document

                            << open_document
                                << "$let" << open_document
                                    << "vars" << open_document
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIME_WEIGHT_VAR << open_document
                                            << "$cond" << open_document
                                                << "if" << open_document
                                                    << "$eq" << open_array
                                                        << "$$this." + user_account_keys::categories::TYPE
                                                        << AccountCategoryType::CATEGORY_TYPE
                                                    << close_array
                                                << close_document
                                                << "then" << matching_algorithm::BETWEEN_CATEGORY_TIMES_WEIGHT
                                                << "else" << matching_algorithm::BETWEEN_ACTIVITY_TIMES_WEIGHT
                                            << close_document
                                        << close_document
                                    << close_document
                                    << "in" << open_document
                                        << "$reduce" << open_document
                                            << "input" << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                                            << "initialValue" << 0
                                            << "in" << open_document
                                                << "$add" << open_array
                                                    << "$$value"
                                                    << open_document
                                                        << "$multiply" << open_array
                                                            << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIME_WEIGHT_VAR
                                                            << open_document
                                                                << "$subtract" << open_array
                                                                    << 1
                                                                    << open_document
                                                                        << "$divide" << open_array
                                                                            << "$$this"
                                                                            << bsoncxx::types::b_int64{matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count()}
                                                                        << close_array
                                                                    << close_document

                                                                << close_array
                                                            << close_document

                                                        << close_array
                                                    << close_document

                                                << close_array
                                            << close_document
                                        << close_document
                                    << close_document
                                << close_document
                            << close_document

                            << open_document
                                << "$cond" << open_document
                                    << "if" << open_document
                                        << "$eq" << open_array
                                            << "$$this." + user_account_keys::categories::TYPE
                                            << AccountCategoryType::CATEGORY_TYPE
                                        << close_array
                                    << close_document
                                    << "then" << matching_algorithm::CATEGORIES_MATCH_WEIGHT
                                    << "else" << matching_algorithm::ACTIVITY_MATCH_WEIGHT
                                << close_document
                            << close_document

                        << close_array
                    << close_document
                    << user_account_keys::accounts_list::ACTIVITY_STATISTICS << open_document
                        << "$concatArrays" << open_array
                            << "$$value." + user_account_keys::accounts_list::ACTIVITY_STATISTICS
                            << open_array
                                << open_document
                                    << user_account_keys::categories::INDEX_VALUE << "$$this." + user_account_keys::categories::INDEX_VALUE
                                    << user_account_keys::categories::TYPE << "$$this." + user_account_keys::categories::TYPE
                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR
                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                                << close_document

                            << close_array

                        << close_array
                    << close_document
                << close_document
            << close_document
        << close_document
    << finalize);

    return true;
}