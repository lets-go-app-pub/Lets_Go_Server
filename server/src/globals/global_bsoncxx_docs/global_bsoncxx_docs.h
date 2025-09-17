//
// Created by jeremiah on 3/29/21.
//
#pragma once

#include <chrono>

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/pipeline.hpp>

#include <AccountLoginTypeEnum.grpc.pb.h>
#include <SMSVerification.grpc.pb.h>
#include <AccountState.grpc.pb.h>
#include "user_account_keys.h"

#include <optional>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include <StatusEnum.grpc.pb.h>
#include <AccountLoginTypeEnum.grpc.pb.h>
#include <SMSVerification.grpc.pb.h>
#include <AccountState.grpc.pb.h>
#include <TypeOfChatMessage.grpc.pb.h>
#include <AccountCategoryEnum.grpc.pb.h>
#include <UserAccountStatusEnum.grpc.pb.h>
#include <stored_type_enum.h>
#include <icons_info_keys.h>
#include <utility_general_functions.h>

#include "chat_room_shared_keys.h"
#include "activities_info_keys.h"
#include "user_account_keys.h"
#include "match_algorithm_results_keys.h"
#include "fresh_errors_keys.h"
#include "individual_match_statistics_keys.h"
#include "outstanding_reports_keys.h"
#include "server_parameter_restrictions.h"
#include "report_values.h"
#include "general_values.h"
#include "chat_room_header_keys.h"
#include "chat_room_message_keys.h"

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

/** DO NOT AUTO FORMAT FILE OR ASSOCIATED .cpp FILE **/

enum LoginFunctionResultValuesEnum {
    LOGIN_ERROR_UNKNOWN_VALUE, //error occurred

    LOGIN_BANNED,
    LOGIN_SUSPENDED,
    LOGIN_ACCOUNT_ID_DOES_NOT_EXIST, //account ID is not stored
    LOGIN_INSTALLATION_DOES_NOT_EXIST, //installation ID never used on account before
    LOGIN_VERIFICATION_TIME_EXPIRED,
    /** USER OK TO LOG IN **/
    //LOGIN_LOGIN_TOKEN_DOES_NOT_MATCH,
    LOGIN_INSTALLATION_ID_DOES_NOT_MATCH, //installation ID does not match stored installation ID
    LOGIN_LOGIN_TOKEN_EXPIRED,
    /** USER ALREADY LOGGED IN **/
    LOGIN_USER_LOGGED_IN
};

//generates document used for checking if sms is on or off cool down and resetting it if off cd
bsoncxx::document::value getResetSmsIfOffCoolDown(
        const bsoncxx::types::b_date& currentDateMongo,
        const std::string& coolDownOnSmsKey,
        const std::string& timeSmsCanBeSentAgainKey);

//generates document for logic using aggregation pipeline
// document expected to be placed inside a replace_root type of aggregation pipeline
// will save a value to LOGGED_IN_RETURN_MESSAGE based on LoginFunctionResultValuesEnum
// which is expected to be projected out and checked
bsoncxx::document::value getLoginFunctionDocument(
        const std::string& account_id,
        const std::string& installation_id,
        const std::chrono::milliseconds& current_timestamp);

//returns a document that can be used to calculate timestamp from a mongoDB aggregation pipeline
//use as AGE << result.view()
bsoncxx::document::value buildDocumentToCalculateAge(const std::chrono::milliseconds& currentTimestamp);

//build a check for making sure passed age range is valid, used inside set_age_range
//this function expects all parameters to be < server_parameter_restrictions::LOWEST_ALLOWED_AGE and
// > server_parameter_restrictions::HIGHEST_ALLOWED_AGE
bsoncxx::document::value buildAgeRangeChecker(int min_age, int max_age);

//build a check for making sure passed age range is valid, used inside set_age_range
//this function expects all parameters to be < server_parameter_restrictions::LOWEST_ALLOWED_AGE and
// > server_parameter_restrictions::HIGHEST_ALLOWED_AGE
void buildAgeRangeChecker(int user_age, int& min_age, int& max_age);

bsoncxx::builder::stream::document buildSmsVerificationFindAndUpdateOneDoc(const std::string& installationId,
                                                                           const std::chrono::milliseconds& currentTimestamp,
                                                                           const AccountLoginType& pendingDocAccountType,
                                                                           const std::string& accountId,
                                                                           const sms_verification::SMSVerificationRequest* request);

//builds an array of a single category with no timeframes (represents ANYTIME)
bsoncxx::array::value buildDefaultCategoriesArray();

//build a document that will update CATEGORIES by inserting a default category if none exists and
// removing expired time frames
bsoncxx::document::value getCategoriesDocWithChecks(const std::chrono::milliseconds& earliestStartTime);

//if an element with the same name already exists, will update the document to the passed document
// if no element exists, will add the new element to the end of the array
bsoncxx::document::value buildSetCategoryOrActivityDoc(
        const bsoncxx::document::view& activity_or_category_doc_view,
        const std::string& ARRAY_FIELD_KEY_TO_UPDATE,
        const bsoncxx::array::view& equality_doc
);

//passed an array of timestamps from the client, the index represents the icon index in the server
//will find all icons that either
//1) do not exist inside list
//2) the timestamp is expired
bsoncxx::document::value buildFindOutdatedIconIndexes(
        const google::protobuf::RepeatedField<google::protobuf::int64>& iconTimestamps
);


//Used by set_categories to project the specified activities from database
//This will return an array that is the same size as index_numbers_array_view.
//The element will be a document containing activities_info_keys::activities::CATEGORY_INDEX if successfully extracted.
//The element will be null under the following conditions.
// 1) If the user is not inside the age range of the activity.
// 2) If the user is not inside the age range of the category.
// 3) The activity index inside index_numbers_array_view does not exist.
//each element of index_numbers_array_view is expected to be an integer.
bsoncxx::document::value buildProjectActivitiesByIndex(
        const std::string& PROJECTED_ARRAY_NAME,
        const bsoncxx::array::view& index_numbers_array_view,
        int user_age
);

//used by move_extracted_element_to_other_account.cpp to update the accounts that were
// matches to both be a part of the chat room as well as both being set as a 'match' to
// each other
bsoncxx::document::value buildUpdateAccountsToMatchMade(
        const std::string& chat_room_id,
        const bsoncxx::types::b_date& last_time_viewed_date_object,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::oid& match_account_oid
);

//builds a document that is sent into getLoginDocument as another condition to check on login
// the condition is that the user is required inside the chat room
bsoncxx::document::value buildUserRequiredInChatRoomCondition(const std::string& chatRoomId);

//builds a document to leave the chat room for a user
// it will set the account state of a user inside the header to not in chat room
// update other relevant values for the user
// and update a new admin if necessary & possible
bsoncxx::document::value buildUserLeftChatRoomHeader(
        const bsoncxx::oid& userAccountOID,
        int thumbnail_size,
        const std::string& thumbnail_reference_oid,
        const std::chrono::milliseconds& currentTimestamp
);

//builds a pipeline (similar to buildUserLeftChatRoomHeader())
// will return the accountOID passed as well as the accountOID that will be
// promoted to admin if this accountOID leaves
//Will contain 2 document with 2 keys.
// If the current user does not exist, null will be returned in place of a document.
// If the current user is not admin OR no account exists to be promoted to admin, null will be returned in place of a document.
mongocxx::pipeline buildUserLeftChatRoomExtractAccountsAggregation(
        const bsoncxx::oid& user_account_oid
);

//builds a document to run on a chat room collection query, it will search for a chat room
//that the user is inside or admin of
bsoncxx::document::value matchChatRoomUserInside(const bsoncxx::oid& userAccountOID);

//build a document to run on a chat room collection query, will search for if the document is
//a message type which is able to be deleted
bsoncxx::document::value buildMessageIsDeleteTypeDocument();

//build a document to update the chat room last active time for a user, it will use
//the $max function to update it (so it will keep the larger value)
bsoncxx::document::value buildUpdateChatRoomHeaderLastActiveTimeForUser(
        const bsoncxx::oid& userAccountOID,
        const std::chrono::milliseconds& currentTimestamp
);

//builds the replaceRoot document used in the query aggregation pipeline when DELETE_FOR_ALL_USERS is used,
//this is used to project the necessary info out
bsoncxx::document::value
buildClientMessageToServerDeleteForEveryoneProjectionDocument(const bsoncxx::oid& userAccountOID);

//builds the replaceRoot document used in the update aggregation pipeline when DELETE_FOR_ALL_USERS is used,
//this will update the header and the message which is going to be deleted
bsoncxx::document::value buildClientMessageToServerDeleteForEveryoneUpdateDocument(
        const bsoncxx::oid& userAccountOID,
        const std::chrono::milliseconds& currentTimestamp
);

//builds the replaceRoot document used in the update aggregation pipeline when DELETE_FOR_SINGLE_USER is used,
//this will update the message which is going to be deleted
bsoncxx::document::value buildClientMessageToServerDeleteSingleUserUpdateDocument(
        const std::string& userAccountOID
);

//builds the replaceRoot document used in the update aggregation pipeline when delete for a single user
//or an edit message is called
bsoncxx::document::value buildClientMessageToServerUpdateModifiedMessage(
        const bsoncxx::oid& userAccountOID,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::document::view& updateMessageDocument
);

//builds the document used inside a match aggregation pipeline inside an $expr block,
//the expression will return true if account is accessible false if deleted
bsoncxx::document::value buildCheckIfAccountDeletedDocument(const std::string& currentUserAccountOIDStr);

//builds the document used inside ChatStreamContainerObject::runReadFunction() when full message info is requested from the server
// this is meant to be used with a replace_root aggregation pipeline stage, it will leave the 'normal' message documents
// alone however it will convert the header to consist of 3 elements, the _id, the header created time, and a bool that
// is true if the user existed inside the chat room and false otherwise
bsoncxx::document::value buildCheckIfUserIsInHeaderAndKeepMessagesTheSame(
        const bsoncxx::oid& userAccountOID,
        const std::string& MESSAGE_TYPE_VALID_KEY,
        const std::string& SORTING_BY_INDEX_KEY,
        const std::string& ARRAY_ID_KEY,
        const std::string& ARRAY_INDEX_KEY,
        const bsoncxx::array::view& user_oid_to_index
);

//builds the document used inside saveNewReportToCollection() function
// this document is meant to be used with an upsert command so that if the document
// does not exist a new one will be inserted if it DOES exist the existing document
// will be updated
bsoncxx::document::value buildUpdateReportLogDocument(
        const bsoncxx::oid& reported_user_account_oid,
        const bsoncxx::oid& reporting_user_account_oid,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::document::view& report_log_element
);

//Builds the document used inside SearchErrors $group stage for the pipeline.
// This document will group by the origin_version_file_line.  It will also
// make sure that any mandatory fields that are used are carried through this
// stage. For device name and api number it will store them twice so that the
// $group stage following can count number of occurrences of each value.
bsoncxx::document::value buildGroupSearchedErrorsDocument(
        const std::string& document_count_key
);

//Builds the document used inside SearchErrors $project stage for the pipeline.
// This document will project out all mandatory fields.  It will also put
// the api number and device name into an array of documents showing the number
// of times each value occurs. This is meant to be used after a fairly specific
// $group stage (look at buildProjectSearchedErrorsDocument()).
bsoncxx::document::value buildProjectSearchedErrorsDocument(
        const std::string& document_count_key,
        const std::string& name_of_field_key,
        const std::string& number_of_times_repeated_key
);

//Builds the document used inside ExtractErrors $project stage for the pipeline.
// This document should receive an array of the sizes of all matching documents and
// will project out a document containing four numbers. Total extracted size, total size,
// number of extracted documents, and total documents.
bsoncxx::document::value buildProjectExtractedErrorDocumentStatistics(
        const std::string& PROJECTED_EXTRACT_INTERMEDIATE_KEY,
        const std::string& PROJECTED_TOTAL_BYTES,
        const std::string& PROJECTED_EXTRACTED_BYTES,
        const std::string& PROJECTED_TOTAL_COUNT_KEY,
        const std::string& PROJECTED_EXTRACTED_COUNT_KEY,
        long MAX_NUMBER_OF_BYTES
);

//Builds the document used inside runMatchingActivityStatistics() $add_fields stage for
// the pipeline.
//This pipeline will generate documents for the MATCH_ALGORITHM_RESULTS_COLLECTION_NAME collection from the
// INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME collection. It will do this by grouping the matches together
// by day occurred, type (activity or category), and index value of the activity or category.
// It will also then count the number of specific swipe types (it follows StoredType
// enum) and store them inside the document.
mongocxx::pipeline buildMatchingActivityStatisticsPipeline(
        const std::string& SWIPE_PIPELINE_TYPE,
        const long& currentTimestampDay,
        const long& finalDayAlreadyGenerated
);

//will build a pipeline for inserting the passed message document to the server
mongocxx::pipeline buildPipelineForInsertingDocument(
        const bsoncxx::document::view& message_to_insert
);

//Build a pipeline for setting an event to 'canceled' state. If it is a user created event, more steps need to be taken.
mongocxx::pipeline buildPipelineForCancelingEvent();
