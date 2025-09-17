//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include <optional>

#include <mongocxx/collection.hpp>

#include <FindMatches.grpc.pb.h>

#include "find_matches_helper_objects.h"
#include "database_names.h"
#include "collection_names.h"
#include "utility_testing_functions.h"
#include "store_mongoDB_error_and_exception.h"

#ifdef LG_TESTING
    inline bool algorithm_ran_only_match_with_events = false;
#endif

//will find the verified account document and the associated matching account document
//will update the verified document values to remove obsolete array elements
//will update the matching document values to remove obsolete elements and update age if necessary
//saves following values to userAccountValues
//findVerifiedAccountView, drawFromListValue, age, matchAccountOID, findMatchDocView
//gendersToMatchMongoDBArray userGenderElement, minAgeRange, maxAgeRange
bool extractAndSaveUserInfo(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::function<void(
                const ReturnStatus&,
                const findmatches::FindMatchesCapMessage::SuccessTypes&
        )>& send_error_message_to_client,
        bsoncxx::stdx::optional<bsoncxx::document::value>& find_user_account_value
);

//attempts to extract an element from either 'algorithm matches' list or 'other users said yes' list
//if a valid element is found it will be 'popped' from the respective array and added to the 'extracted' list
//the function will also save the valid values to the gRPC response passed to it
ValidateArrayElementReturnEnum validateArrayElement(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        FindMatchesArrayNameEnum array_to_pop_from
);

//this function runs after the algorithm and organizes and saves all of the matches to a document
bool extractAndSaveMatchingResults(
        UserAccountValues& user_account_values,
        std::optional<mongocxx::cursor>& matching_accounts_cursor
);

//extracts and stores various information from the matching document to be used in the algorithm
AlgorithmReturnValues organizeUserInfoForAlgorithm(
        UserAccountValues& user_account_values,
        mongocxx::client_session* session,
        mongocxx::collection& user_accounts_collection
);

//runs checks to see if algorithm is still on cool down for this account before running it
//NOTE: the bool here will only return false if an error occurred, not if the algorithm is on cool down
AlgorithmReturnValues beginAlgorithmCheckCoolDown(
        UserAccountValues& user_account_values,
        mongocxx::client_session* session,
        mongocxx::collection& user_accounts_collection
);

//this function will create and run the aggregation pipeline
//it stores the resulting cursor in matchingAccounts
//if an error occurs it returns false if not it returns true
bool runMatchingAlgorithm(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        std::optional<mongocxx::cursor>& matching_accounts,
        bool only_match_with_events
);

//This function will call runMatchingAlgorithm() and save the results.
//user_account_values is expected to be fully set up for this function to run properly.
AlgorithmReturnValues runMatchingAlgorithmAndReturnResults(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        bool only_match_with_events
);

//extract values from matchInfo struct and save it to matchResponseMessage generating
// the gRPC message to send back to the client
//returns 'true' if successful 'false' if an error occurred
bool extractMatchAccountInfoForWriteToClient(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        const MatchInfoStruct& match_info,
        const std::chrono::milliseconds& swipes_time_before_reset,
        const std::chrono::milliseconds& current_timestamp,
        findmatches::SingleMatchMessage* match_response_message,
        std::vector<bsoncxx::document::value>& insert_statistics_documents,
        bsoncxx::builder::basic::array& update_statistics_documents_query
);

bool buildQueryDocForAggregationPipeline(
        UserAccountValues& userAccountValues,
        mongocxx::collection& user_accounts_collection,
        bsoncxx::builder::stream::document& query_doc,
        bool only_match_with_events
);

/** -------------------------- **/
/** -- INCLUDED FOR TESTING -- **/
/** -------------------------- **/
//used for handling error cases, if an invalid value is returned this will be called
bool clearOidArrayElement(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::string& arrayKey
);

bool clearStringArrayElement(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::string& arrayOfOID,
        const std::string& oidKey
);

//finds and stores all accounts that this account should never match with to a single array inside userAccountValues
bool storeRestrictedAccounts(
        UserAccountValues& user_account_values,
        mongocxx::client_session* string_key,
        mongocxx::collection& user_accounts_collection
);

//builds the categories array from the passed document
//NOTE: this function is fairly similar to requestCategoriesHelper() however the way it handles
// timestamps and storing them is different and so it is a separate function
bool buildCategoriesArrayForCurrentUser(UserAccountValues& user_account_values);

//saves the statistics for algorithm running, as well as individual matches sent to client
void saveFindMatchesStatistics(
        UserAccountValues& user_account_values,
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& stats_db,
        mongocxx::database& accounts_db,
        const std::vector<bsoncxx::document::value>& statistics_documents_to_insert,
        const bsoncxx::array::view& update_statistics_documents_query
);

//builds document for projecting out all fields that are not 'useful' for statistics
bsoncxx::document::value projectUserAccountFieldsOutForStatistics();

//extracts and saves the passed field to the restricted array for the algorithm
template <bool field_is_string_type>
bool saveRestrictedOIDToArray(
        UserAccountValues& user_account_values,
        const std::string& array_key,
        const std::string& array_oid_key,
        const bsoncxx::document::view& document_to_extract_from,
        bool field_must_exist
) {

    //NOTE: typesToBeRemoved is only used in a single case.
    bool successful = true;

    std::vector<bsoncxx::oid> temp_oids;
    auto extracted_array_ele = document_to_extract_from[array_key];
    if (extracted_array_ele
        && extracted_array_ele.type() == bsoncxx::type::k_array) { //if element exists and is type array

        bsoncxx::builder::stream::document builder;

        //iterate through has been extracted accounts and get oid
        bsoncxx::array::view extracted_array = extracted_array_ele.get_array().value;
        for (const auto& matched : extracted_array) {
            if (matched.type() == bsoncxx::type::k_document) { //if is type document
                const bsoncxx::document::view matched_document_view = matched.get_document().value;
                const bsoncxx::type field_type = field_is_string_type ? bsoncxx::type::k_utf8 : bsoncxx::type::k_oid;

                auto object_id_element = matched_document_view[array_oid_key];
                if (object_id_element && object_id_element.type() == field_type) { //if element exists and is type oID
                    if (field_is_string_type) {
                        temp_oids.emplace_back(object_id_element.get_string().value.to_string());
                    } else {
                        temp_oids.emplace_back(object_id_element.get_oid().value);
                    }
                } else if(field_must_exist) { //if element should exist does not exist or is not type oID

                    successful = false;

                    logElementError(
                            __LINE__, __FILE__, object_id_element,
                            document_to_extract_from, field_type, array_oid_key,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );
                }

            } else { //if matchedOID is NOT type document
                const std::string error_string = "has been extracted array element came back wrong type '";

                const std::string element_type = convertBsonTypeToString(matched.type());
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>{}, error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "element_type", element_type,
                        "document", document_to_extract_from,
                        "oID_Used", user_account_values.user_account_oid
                );
                return false;
            }
        }
    }
    else { //if element does not exist or is not type array
        logElementError(__LINE__, __FILE__, extracted_array_ele,
                        document_to_extract_from, bsoncxx::type::k_array, array_key,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    for (const auto& o : temp_oids) {
        user_account_values.restricted_accounts.insert(o);
    }

    return successful;
}