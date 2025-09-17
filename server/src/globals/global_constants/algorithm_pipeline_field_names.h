//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>
#include "user_account_keys.h"

namespace algorithm_pipeline_field_names {
    //these are used as intermediates in the mongoDB algorithm
    /** NOTE: DO NOT CHANGE THESE NAMES, THE PIPELINES ARE GENERATED FROM A DIFFERENT PROGRAM **/
    inline const std::string MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER = "uSr"; //boolean; true if user account, not set if matching time frame
    inline const std::string MONGO_PIPELINE_MATCHING_ACTIVITIES_FIELD = "mAf"; //array of documents; holds a temporary key with CATEGORIES style documents of matching activites for the match
    inline const std::string MONGO_PIPELINE_DISTANCE_KEY = "dI"; //type double; returned by the geoNear search as the field for distance user is from match
    inline const std::string MONGO_PIPELINE_AGE_RANGE_MIN_KEY = user_account_keys::AGE_RANGE + '.' + user_account_keys::age_range::MIN;
    inline const std::string MONGO_PIPELINE_AGE_RANGE_MAX_KEY = user_account_keys::AGE_RANGE + '.' + user_account_keys::age_range::MAX;
    inline const std::string MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY = "cArr"; //type array; a temporary name with $map when the input is CATEGORIES
    inline const std::string MONGO_PIPELINE_TIMEFRAME_KEY = user_account_keys::CATEGORIES + '.' + user_account_keys::categories::TIMEFRAMES;
    inline const std::string MONGO_PIPELINE_TIMEFRAME_DATA = "tFd" ; //document with 4 fields, 1 of which is an array of TIMEFRAMES type documents another is MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH (the others are parameters for a $reduce function)
    inline const std::string MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH = "tFm" ; //type int64; (could be type int, but derived from std::chrono::milliseconds.count())
    inline const std::string MONGO_PIPELINE_PREVIOUS_TIME_VAR = "pTi"; //type int64; stored TIME from a $reduce function
    inline const std::string MONGO_PIPELINE_PREVIOUS_TIME_IS_ABOVE_NOW_VAR = "pAn" ; //type boolean; used with MONGO_PIPELINE_PREVIOUS_TIME_VAR
    inline const std::string MONGO_PIPELINE_TIME_IS_ABOVE_NOW_VAR = "tAn" ; //type boolean; used as a variable inside generateAddFieldsInitializeAndCleanTimeFramesPipelineStage()
    inline const std::string MONGO_PIPELINE_TIME_FRAMES_ACTIVITIES_ARRAY_VAR = "tFa";
    inline const std::string MONGO_PIPELINE_TIME_FRAMES_CATEGORIES_ARRAY_VAR = "tFc";
    inline const std::string MONGO_PIPELINE_TIME_FRAMES_ARRAY_VAR = "tFl"; //array of documents representing time frames
    inline const std::string MONGO_PIPELINE_TIME_STATS_VAR = "tSt"; //document of values; used as a variable, one of the variables is the time_frames array to be passed on in the pipeline
    inline const std::string MONGO_PIPELINE_USER_INDEX_VAR = "uSi"; //a number of some kind, does not need to be more than an unsigned int, but mongoDB sometimes makes them doubles
    inline const std::string MONGO_PIPELINE_MATCH_INDEX_VAR = "mAi"; //a number of some kind, does not need to be more than an unsigned int, but mongoDB sometimes makes them doubles
    inline const std::string MONGO_PIPELINE_USER_ARRAY_VAL_VAR = "uAv"; //document; a variable representing a document of type TIMEFRAMES OR just containing the TIME element
    inline const std::string MONGO_PIPELINE_MATCH_ARRAY_VAL_VAR = "mAV"; //document; a variable representing a document of type TIMEFRAMES OR just containing the TIME element
    inline const std::string MONGO_PIPELINE_ARRAY_ELEMENT_MATCH_VAR = "aEm"; //boolean; variable used as part of algorithm, not passed on
    inline const std::string MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR = "tOp"; //int64; total overlap time of the matching arrays
    inline const std::string MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR = "bTa"; //array of int64 values; each value is a timestamp (in millis) of close timeframes
    inline const std::string MONGO_PIPELINE_NESTED_VALUE_VAR = "nTv"; //number; mongoDB creates and interprets the type of this (probably a double)
    inline const std::string MONGO_PIPELINE_OVERLAP_START_TIME_VAR = "oSt"; //int64; variable used in a $reduce
    inline const std::string MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR = "bSt"; //int64; variable used in a $reduce
    inline const std::string MONGO_PIPELINE_PREVIOUS_TYPE_USER_VAR = "pTu"; //boolean; true if current user timeframe, false if match timeframe
    inline const std::string MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR = "cNv"; //number; mongoDB creates and interprets the type of this (probably a double an unsigned int would be sufficient though)

    inline const std::string MONGO_PIPELINE_BETWEEN_TIME_VALUE_VAR = "bTv";
    inline const std::string MONGO_PIPELINE_EXPIRATION_OVERLAP_CHECK_VAR = "eOc"; //boolean; variable in $let statement
    inline const std::string MONGO_PIPELINE_EXPIRATION_BETWEEN_CHECK_VAR = "eBc";

    inline const std::string MONGO_PIPELINE_BETWEEN_CHECK_VAR = "bCV"; //boolean; variable in $let statement
    inline const std::string MONGO_PIPELINE_BETWEEN_CHECK_SECOND_VAR = "bCs"; //boolean; variable in $let statement

    inline const std::string MONGO_PIPELINE_MATCH_STATISTICS_VAR = "mSt"; //document; contains the statistics for the match
    //const std::string MONGO_PIPELINE_ACTIVITY_STATISTICS_VAR = "aSt"; //document; contains statistics for calculating points part of the pipeline
    inline const std::string MONGO_PIPELINE_TOTAL_USER_TIME_VAR = "tUt"; //type int64; timestamp for total user time in timeframe for the specific activity

    inline const std::string MONGO_PIPELINE_TIME_MATCH_RAN_VAR = "tMR"; //int64; the currentTime value of the time that the match ran
    inline const std::string MONGO_PIPELINE_EARLIEST_START_TIME_VAR = "eST"; //int64; the earliest possible time value that the match ran
    inline const std::string MONGO_PIPELINE_MAX_POSSIBLE_TIME_VAR = "mPt"; //int64; the latest possible time value that the match ran

    inline const std::string MONGO_PIPELINE_FINAL_POINTS_VAR = "fPt"; //double; final points value of the match

    inline const std::string MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR = "mEt"; //int64; timestamp for the expiration time of this match based on timestamps
    inline const std::string MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR = "mSo"; //number; mongoDB creates and interprets the type of this (probably a double an unsigned int would be sufficient though)

    inline const std::string MONGO_PIPELINE_OVERLAP_RATIO_VAR = "oVr"; //number; most likely a double, it is the ratio of totalOverlapTime/totalTime
    inline const std::string MONGO_PIPELINE_SHORT_OVERLAP_TIME_WEIGHT_VAR = "sOv"; //number; most likely an int32, it should be either SHORT_TIMEFRAME_ACTIVITY_OVERLAP_WEIGHT or SHORT_TIMEFRAME_CATEGORY_OVERLAP_WEIGHT
    inline const std::string MONGO_PIPELINE_SHORT_OVERLAP_POINTS_VAR = "sOp"; //number; most likely a double, it is a point value added to overlapping times

    inline const std::string MONGO_PIPELINE_MATCHED_ID_VAR = "mID"; //double (or document I think?), an intermediate variable for calculations
    inline const std::string MONGO_PIPELINE_NUMERATOR_VAR = "nUm"; //double, numerator intermediate for MONGO_PIPELINE_TIME_FALL_OFF_VAR
    inline const std::string MONGO_PIPELINE_DENOMINATOR_VAR = "dEn"; //double, denominator intermediate for MONGO_PIPELINE_TIME_FALL_OFF_VAR
    inline const std::string MONGO_PIPELINE_TIME_FALL_OFF_VAR = "tFo"; //double, one of the final point calculations

    inline const std::string MONGO_PIPELINE_OVERLAP_TIME_WEIGHT_VAR = "oTw"; //int32; either OVERLAPPING_ACTIVITY_TIMES_WEIGHT or OVERLAPPING_CATEGORY_TIMES_WEIGHT
    inline const std::string MONGO_PIPELINE_BETWEEN_TIME_WEIGHT_VAR = "bTw"; //int32; either BETWEEN_ACTIVITY_TIMES_WEIGHT or BETWEEN_CATEGORY_TIMES_WEIGHT
    inline const std::string MONGO_PIPELINE_INACTIVITY_POINTS_TO_SUBTRACT_KEY = "iPs"; //double (a negative number); one of the point values calculated before the final points for the match are tabulated
    inline const std::string MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY = "cAo"; //double; one of the point values calculated before the final points for the match are tabulated

}