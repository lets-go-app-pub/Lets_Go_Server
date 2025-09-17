//
// Created by jeremiah on 3/20/21.
//

#include <bsoncxx/builder/basic/array.hpp>
#include <AccountCategoryEnum.grpc.pb.h>

#include "algorithm_pipeline_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "algorithm_pipeline_field_names.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//returns an empty array if an error occurred
bsoncxx::builder::basic::array saveTimeframesToMongoDBArray(
        const UserAccountValues& userAccountValues,
        const bsoncxx::builder::basic::array& branchDefaultValue,
        std::vector<ActivityStruct>& activitiesOrCategories
);

void generateAddFieldsMergeTimeFramesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
) {

    bsoncxx::builder::basic::array branchDefaultValue;
    branchDefaultValue.append(
        document{}
            << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{userAccountValues.earliest_time_frame_start_timestamp.count() + 1}
            << user_account_keys::categories::timeframes::START_STOP_VALUE << bsoncxx::types::b_int32{1}
            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
        << finalize
    );

    branchDefaultValue.append(
        document{}
            << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{userAccountValues.end_of_time_frame_timestamp.count()}
            << user_account_keys::categories::timeframes::START_STOP_VALUE << bsoncxx::types::b_int32{-1}
            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
        << finalize
    );

    auto activityTimeFramesCasesArray = saveTimeframesToMongoDBArray(
            userAccountValues,
            branchDefaultValue,
            userAccountValues.user_activities
    );

    bsoncxx::builder::stream::document activities_and_categories_switches = generateConditionalSwitchesForCategories(
            userAccountValues.algorithm_search_options,
            activityTimeFramesCasesArray.view(),
            [&branchDefaultValue](const bsoncxx::array::view& activities_categories_doc)->bsoncxx::builder::stream::document {
                bsoncxx::builder::stream::document return_doc;
                return_doc
                    << "$switch" << open_document
                        << "branches" << activities_categories_doc
                        << "default" << branchDefaultValue
                    << close_document;
                return return_doc;
            },
            [&userAccountValues, &branchDefaultValue]()->bsoncxx::builder::basic::array {
                auto categoriesTimeFramesArray = saveTimeframesToMongoDBArray(
                        userAccountValues,
                        branchDefaultValue,
                        userAccountValues.user_categories
                );

                return categoriesTimeFramesArray;
            }
    );

    stages->add_fields(document{}
        << user_account_keys::CATEGORIES << open_document
            << "$map" << open_document
                << "input" << '$' + user_account_keys::CATEGORIES
                << "as" << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY
                << "in" << open_document
                    << user_account_keys::categories::TYPE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TYPE
                    << user_account_keys::categories::INDEX_VALUE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::INDEX_VALUE
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
                    << user_account_keys::categories::TIMEFRAMES << open_document
                        << "$let" << open_document
                            << "vars" << open_document
                                << algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FRAMES_ARRAY_VAR << activities_and_categories_switches
                            << close_document
                            << "in" << open_document
                                << "$let" << open_document
                                    << "vars" << open_document
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_STATS_VAR << open_document
                                            << "$reduce" << open_document
                                                << "input" << open_document
                                                    << "$range" << open_array
                                                        << 0
                                                        << open_document
                                                            << "$add" << open_array
                                                                << open_document
                                                                    << "$size" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TIMEFRAMES
                                                                << close_document

                                                                << open_document
                                                                    << "$size" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FRAMES_ARRAY_VAR
                                                                << close_document

                                                            << close_array
                                                        << close_document

                                                    << close_array
                                                << close_document
                                                << "initialValue" << open_document
                                                    << user_account_keys::categories::TIMEFRAMES << open_array

                                                    << close_array
                                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_USER_INDEX_VAR << 0
                                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_INDEX_VAR << 0
                                                << close_document
                                                << "in" << open_document
                                                    << "$let" << open_document
                                                        << "vars" << open_document
                                                            << algorithm_pipeline_field_names::MONGO_PIPELINE_USER_ARRAY_VAL_VAR << open_document
                                                                << "$cond" << open_document
                                                                    << "if" << open_document
                                                                        << "$lt" << open_array
                                                                            << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_USER_INDEX_VAR
                                                                            << open_document
                                                                                << "$size" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FRAMES_ARRAY_VAR
                                                                            << close_document

                                                                        << close_array
                                                                    << close_document
                                                                    << "then" << open_document
                                                                        << "$arrayElemAt" << open_array
                                                                            << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FRAMES_ARRAY_VAR
                                                                            << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_USER_INDEX_VAR
                                                                        << close_array
                                                                    << close_document
                                                                    << "else" << open_document
                                                                        << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{general_values::NUMBER_BIGGER_THAN_UNIX_TIMESTAMP_MS}
                                                                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 0
                                                                    << close_document
                                                                << close_document
                                                            << close_document
                                                            << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_ARRAY_VAL_VAR << open_document
                                                                << "$cond" << open_document
                                                                    << "if" << open_document
                                                                        << "$lt" << open_array
                                                                            << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_INDEX_VAR
                                                                            << open_document
                                                                                << "$size" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TIMEFRAMES
                                                                            << close_document

                                                                        << close_array
                                                                    << close_document
                                                                    << "then" << open_document
                                                                        << "$arrayElemAt" << open_array
                                                                            << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TIMEFRAMES
                                                                            << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_INDEX_VAR
                                                                        << close_array
                                                                    << close_document
                                                                    << "else" << open_document
                                                                        << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{general_values::NUMBER_BIGGER_THAN_UNIX_TIMESTAMP_MS}
                                                                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 0
                                                                    << close_document
                                                                << close_document
                                                            << close_document

                                                        << close_document
                                                        << "in" << open_document
                                                            << "$let" << open_document
                                                                << "vars" << open_document
                                                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_ARRAY_ELEMENT_MATCH_VAR << open_document
                                                                        << "$cond" << open_document
                                                                            << "if" << open_document
                                                                                << "$or" << open_array
                                                                                    << open_document
                                                                                        << "$lt" << open_array
                                                                                            << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_USER_ARRAY_VAL_VAR + '.' + user_account_keys::categories::timeframes::TIME
                                                                                            << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_ARRAY_VAL_VAR + '.' + user_account_keys::categories::timeframes::TIME
                                                                                        << close_array
                                                                                    << close_document

                                                                                    << open_document
                                                                                        << "$and" << open_array
                                                                                            << open_document
                                                                                                << "$eq" << open_array
                                                                                                    << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_USER_ARRAY_VAL_VAR + '.' + user_account_keys::categories::timeframes::TIME
                                                                                                    << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_ARRAY_VAL_VAR + '.' + user_account_keys::categories::timeframes::TIME
                                                                                                << close_array
                                                                                            << close_document

                                                                                            << open_document
                                                                                                << "$eq" << open_array
                                                                                                    << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_USER_ARRAY_VAL_VAR + '.' + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                                                    << 1
                                                                                                << close_array
                                                                                            << close_document

                                                                                        << close_array
                                                                                    << close_document

                                                                                << close_array
                                                                            << close_document
                                                                            << "then" << false
                                                                            << "else" << true
                                                                        << close_document
                                                                    << close_document
                                                                << close_document
                                                                << "in" << open_document
                                                                    << user_account_keys::categories::TIMEFRAMES << open_document
                                                                        << "$cond" << open_document
                                                                            << "if" << open_document
                                                                                << "$eq" << open_array
                                                                                    << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_ARRAY_ELEMENT_MATCH_VAR
                                                                                    << false
                                                                                << close_array
                                                                            << close_document
                                                                            << "then" << open_document
                                                                                << "$concatArrays" << open_array
                                                                                    << "$$value." + user_account_keys::categories::TIMEFRAMES
                                                                                    << open_array
                                                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_USER_ARRAY_VAL_VAR
                                                                                    << close_array

                                                                                << close_array
                                                                            << close_document
                                                                            << "else" << open_document
                                                                                << "$concatArrays" << open_array
                                                                                    << "$$value." + user_account_keys::categories::TIMEFRAMES
                                                                                    << open_array
                                                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_ARRAY_VAL_VAR
                                                                                    << close_array

                                                                                << close_array
                                                                            << close_document
                                                                        << close_document
                                                                    << close_document
                                                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_USER_INDEX_VAR << open_document
                                                                        << "$cond" << open_document
                                                                            << "if" << open_document
                                                                                << "$eq" << open_array
                                                                                    << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_ARRAY_ELEMENT_MATCH_VAR
                                                                                    << false
                                                                                << close_array
                                                                            << close_document
                                                                            << "then" << open_document
                                                                                << "$add" << open_array
                                                                                    << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_USER_INDEX_VAR
                                                                                    << 1
                                                                                << close_array
                                                                            << close_document
                                                                            << "else" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_USER_INDEX_VAR
                                                                        << close_document
                                                                    << close_document
                                                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_INDEX_VAR << open_document
                                                                        << "$cond" << open_document
                                                                            << "if" << open_document
                                                                                << "$eq" << open_array
                                                                                    << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_ARRAY_ELEMENT_MATCH_VAR
                                                                                    << true
                                                                                << close_array
                                                                            << close_document
                                                                            << "then" << open_document
                                                                                << "$add" << open_array
                                                                                    << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_INDEX_VAR
                                                                                    << 1
                                                                                << close_array
                                                                            << close_document
                                                                            << "else" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_INDEX_VAR
                                                                        << close_document
                                                                    << close_document
                                                                << close_document
                                                            << close_document
                                                        << close_document
                                                    << close_document
                                                << close_document
                                            << close_document
                                        << close_document
                                    << close_document
                                    << "in" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_STATS_VAR + '.' + user_account_keys::categories::TIMEFRAMES
                                << close_document
                            << close_document
                        << close_document
                    << close_document
                << close_document
            << close_document
        << close_document
    << finalize);

}

void createAnAnytimeArray(
        bsoncxx::builder::basic::array& activityTimeFrames,
        const bsoncxx::builder::basic::array& branchDefaultValue,
        ActivityStruct& activityOrCategory,
        const std::chrono::milliseconds& earliestTimeFrameStartTimestamp,
        const std::chrono::milliseconds& endOfTimeFrameTimestamp
        ) {
    //create an anytime array for this time frame
    activityTimeFrames.clear();

    for(const auto& ele : branchDefaultValue.view()) {
        activityTimeFrames.append(ele.get_document());
    }

    activityOrCategory.totalTime =
            endOfTimeFrameTimestamp - (earliestTimeFrameStartTimestamp + std::chrono::milliseconds{1});

}

bsoncxx::builder::basic::array saveTimeframesToMongoDBArray(
        const UserAccountValues& userAccountValues,
        const bsoncxx::builder::basic::array& branchDefaultValue,
        std::vector<ActivityStruct>& activitiesOrCategories
) {
    auto timeFrameCasesArray = bsoncxx::builder::basic::array{};
    const std::string categoryActivityKey = std::string("$$").append(algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY).append(".").append(user_account_keys::categories::INDEX_VALUE);

    //builds the array for the switch statement for the aggregation pipeline
    for (ActivityStruct& activityOrCategory : activitiesOrCategories) {

        auto activityTimeFrames = bsoncxx::builder::basic::array{};

        for (unsigned int j = 0; j < activityOrCategory.timeFrames.size(); j += 2) {

            if (activityOrCategory.timeFrames[j].startStopValue != 1
                || j + 1 == activityOrCategory.timeFrames.size()
                || activityOrCategory.timeFrames[j + 1].startStopValue != -1
                ) {

                std::string errorString = "Start stop values inside time frame array are not lined up or size is not even.";

                std::string timeFrameValues;

                for (unsigned int k = 0; k < activityOrCategory.timeFrames.size(); k++) {
                    timeFrameValues += "Index: " + std::to_string(k);
                    timeFrameValues += " StartStopValue: " + std::to_string(activityOrCategory.timeFrames[k].startStopValue);
                    timeFrameValues += " Time: " + getDateTimeStringFromTimestamp(userAccountValues.current_timestamp);
                    timeFrameValues += '\n';
                }

                std::optional<std::string> exceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__, exceptionString, errorString,
                        "time_frames", timeFrameValues,
                        "document", userAccountValues.user_account_doc_view,
                        "j", std::to_string(j),
                        "activityOrCategory.timeFrames.size()", std::to_string(activityOrCategory.timeFrames.size())
                );

                createAnAnytimeArray(
                        activityTimeFrames,
                        branchDefaultValue,
                        activityOrCategory,
                        userAccountValues.earliest_time_frame_start_timestamp,
                        userAccountValues.end_of_time_frame_timestamp
                );

                break;
            }

            bsoncxx::builder::stream::document switchCaseDocument;
            activityTimeFrames.append(
                    document{}
                            << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{activityOrCategory.timeFrames[j].time.count()}
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << bsoncxx::types::b_int32{activityOrCategory.timeFrames[j].startStopValue}
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << finalize
            );

            activityTimeFrames.append(
                    document{}
                            << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{activityOrCategory.timeFrames[j + 1].time.count()}
                            << user_account_keys::categories::timeframes::START_STOP_VALUE << bsoncxx::types::b_int32{activityOrCategory.timeFrames[j + 1].startStopValue}
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER << bsoncxx::types::b_bool{true}
                    << finalize
            );
        }

        if (activityOrCategory.totalTime.count() == 0) {
            std::string errorString = "Total time ended up as 0 for user activities '";
            std::string timeFrameValues;

            for (unsigned int k = 0; k < activityOrCategory.timeFrames.size(); k++) {
                timeFrameValues += "Index: " + std::to_string(k);
                timeFrameValues += " StartStopValue: ";
                timeFrameValues += std::to_string(activityOrCategory.timeFrames[k].startStopValue);
                timeFrameValues += " Time: " + getDateTimeStringFromTimestamp(userAccountValues.current_timestamp);
                timeFrameValues += '\n';
            }

            std::optional<std::string> exceptionString;
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    exceptionString, errorString,
                    "time_frames", timeFrameValues,
                    "document", userAccountValues.user_account_doc_view
            );

            createAnAnytimeArray(
                    activityTimeFrames,
                    branchDefaultValue,
                    activityOrCategory,
                    userAccountValues.earliest_time_frame_start_timestamp,
                    userAccountValues.end_of_time_frame_timestamp
            );

            //NOTE: OK to continue here
        }

        auto activityDocument =
            document{}
                << "case" << open_document
                    << "$eq" << open_array
                        << bsoncxx::types::b_int32{activityOrCategory.activityIndex}
                        << categoryActivityKey
                    << close_array
                << close_document
                << "then" << activityTimeFrames
            << finalize;

        timeFrameCasesArray.append(activityDocument);

    }

    if(timeFrameCasesArray.view().empty()) {

        const std::string error_string = "No timeframes found in user timeframes array.";
        std::optional<std::string> dummy_exception_string;
        storeMongoDBErrorAndException(__LINE__, __FILE__, dummy_exception_string, error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                      "document", userAccountValues.user_account_doc_view);

        //upsert dummy case that should NEVER be true (the default branch will be selected, however branches cannot be
        // submitted as empty)
        timeFrameCasesArray.append(
            document{}
                << "case" << open_document
                    << "$lt" << open_array
                        << categoryActivityKey
                        << 0
                    << close_array
                << close_document
                << "then" << branchDefaultValue
            << finalize
        );
    }

    return timeFrameCasesArray;
}

