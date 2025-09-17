//
// Created by jeremiah on 3/19/21.
//

#include <optional>

#include "user_match_options_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "check_for_no_elements_in_array.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool handleUserExtractedAccount(
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::collection& user_account_collection,
        mongocxx::client_session* session,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::oid& match_account_oid,
        const ResponseType& response_type,
        std::optional<bsoncxx::document::value>& user_account_doc,
        bsoncxx::document::view& user_account_doc_view,
        std::optional<bsoncxx::document::view>& user_extracted_array_doc,
        std::optional<bsoncxx::oid>& statistics_oid
) {

    if(session == nullptr) {
        const std::string error_string = "handleUserExtractedAccount() was called when session was set to nullptr.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional <std::string>(), error_string,
                "userAccountOID", user_account_oid,
                "matchAccountOID", match_account_oid
        );

        return false;
    }

    mongocxx::pipeline pipe;
    bsoncxx::builder::stream::document project_reduced_arrays_builder;

    pipe.match(
        document{}
            << "_id" << user_account_oid
        << finalize
    );

    addFilterForProjectionStage(
            project_reduced_arrays_builder,
            match_account_oid,
            user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
            user_account_keys::accounts_list::OID
    );

    addFilterForProjectionStage(
            project_reduced_arrays_builder,
            match_account_oid,
            user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST,
            user_account_keys::accounts_list::OID
    );

    addFilterForProjectionStage(
            project_reduced_arrays_builder,
            match_account_oid,
            user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST,
            user_account_keys::accounts_list::OID
    );

    addFilterForProjectionStage(
            project_reduced_arrays_builder,
            match_account_oid,
            user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS,
            user_account_keys::previously_matched_accounts::OID
    );

    if (response_type == ResponseType::USER_MATCH_OPTION_YES) { //if generating yes response

        //Project the fields needed for verifying the other account is still a match. And for chat room info.
        project_reduced_arrays_builder
                << user_account_keys::AGE << 1
                << user_account_keys::GENDER << 1
                << user_account_keys::AGE_RANGE << 1
                << user_account_keys::GENDERS_RANGE << 1
                << user_account_keys::FIRST_NAME << 1
                << user_account_keys::PICTURES << 1;
    }

    std::optional<mongocxx::cursor> find_user_account_cursor;
    pipe.project(project_reduced_arrays_builder.view());
    try {
            //if match was successful
            find_user_account_cursor = user_account_collection.aggregate(
                    *session,
                    pipe
            );
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), e.what(),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "oid_used", user_account_oid
        );
        return false;
    }

    int num_extracted_elements = 0;
    int num_documents = 0;

    if(!find_user_account_cursor) {
        const std::string error_string = "Cursor was not found when attempting aggregation inside handleUserExtractAccount().";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "oid_used", user_account_oid
        );

        return false;
    }

    //There should only be 1 document in the result because it was queried by _id.
    for (const auto& doc : *find_user_account_cursor) {
        user_account_doc = bsoncxx::document::value(doc);
        user_account_doc_view = user_account_doc->view();
        num_documents++;
    }

    if (num_documents != 1) { //if matching account documents was 0 or greater than 1
        const std::string error_string = "Matching document count was not 1.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "oid_used", user_account_oid,
                "numDocuments", std::to_string(num_documents)
        );

        return false;
    }

    //error checking
    const int num_algorithm_match_elements = checkForNoElementsInArray<true>(
            &user_account_doc_view,
            user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST
    );

    const int num_other_users_said_elements = checkForNoElementsInArray<true>(
            &user_account_doc_view,
            user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST
    );

    const int num_previously_matched_elements = checkForNoElementsInArray<false>(
            &user_account_doc_view,
            user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS
    );

    //check the 'extracted' list
    auto extracted_array_element = user_account_doc_view[user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST];
    if (extracted_array_element
        && extracted_array_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
        auto extracted_array = extracted_array_element.get_array().value;

        for (auto ele : extracted_array) {

            if (num_extracted_elements == 0) { //the first element in the array
                //extract document
                if (ele && ele.type() == bsoncxx::type::k_document) { //if element exists and is type document
                    user_extracted_array_doc = ele.get_document().value;
                } else { //if element does not exist or is not type document
                    logElementError(
                        __LINE__, __FILE__,
                        extracted_array_element, user_account_doc_view,
                        bsoncxx::type::k_document, user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );
                    num_extracted_elements = -1;
                    break;
                }
            }
            num_extracted_elements++;
        }
    } else { //if element does not exist or is not type array
        logElementError(
            __LINE__, __FILE__,
            extracted_array_element, user_account_doc_view,
            bsoncxx::type::k_array, user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        return false;
    }

    bsoncxx::builder::stream::document update_query_document;
    bsoncxx::builder::stream::document update_document;

    update_query_document
            << "_id" << user_account_oid;

    if (
        (num_algorithm_match_elements > 1 && num_other_users_said_elements > 1)
        || (num_algorithm_match_elements > 1 && num_extracted_elements > 1)
        || (num_other_users_said_elements > 1 && num_extracted_elements > 1)
            ) { //if this element exists in more than one place, store error

        const std::string error_string = "An oid exists in multiple places across the matching account lists in.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "oid_used", match_account_oid,
                "projected_document", user_account_doc_view
        );

        //NOTE: Should continue here, the function will clean up arrays if necessary.
    }

    if (num_extracted_elements != 0) { //if elements exist in 'extracted' list or an error occurred

        //remove all elements for this id from 'extracted' list
        update_document
            << "$pull" << open_document
                << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST << open_document
                    << user_account_keys::accounts_list::OID << match_account_oid
                << close_document
            << close_document;

        if (num_extracted_elements > 0 && user_extracted_array_doc) { //if there is at least one element in the 'extracted' list
            if (num_extracted_elements > 1) {
                //store error, the elements will be removed either way
                const std::string error_string = "Too many elements in array " + user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST + '\n';
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "matching_document", user_account_doc_view
                );

                //NOTE: Want to continue here to allow elements to be cleaned up.
            }

            //extract OID for statistics
            auto stats_doc_oid_element = (*user_extracted_array_doc)[user_account_keys::accounts_list::SAVED_STATISTICS_OID];
            if (stats_doc_oid_element
                && stats_doc_oid_element.type() == bsoncxx::type::k_oid) { //if element exists and is type oid
                //store the oid for the statistics document, it will be updated after the transaction
                statistics_oid = stats_doc_oid_element.get_oid().value;
            } else if (!stats_doc_oid_element) { //if element does not exist
                //If element does not exist the error should have already been stored back when this was put into the
                // collection in matchAccounts.cpp.
            } else { //if element exists but is not type oid
                logElementError(
                    __LINE__, __FILE__,
                    stats_doc_oid_element, *user_extracted_array_doc,
                    bsoncxx::type::k_oid, user_account_keys::accounts_list::SAVED_STATISTICS_OID,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );
                return false;
            }
        }

    } else { //if there are no elements for this id in the 'extracted' list
        //This means that the extracted element was naturally filtered out, most likely because its expiration time was
        // reached, no need to do anything here.
    }

    if (num_algorithm_match_elements != 0) {

        //NOTE: The errors for this are already sent.
        update_document
            << "$pull" << open_document
                << user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST << open_document
                    << user_account_keys::accounts_list::OID << match_account_oid
                << close_document
            << close_document;
    }

    if (num_other_users_said_elements != 0) {

        //NOTE: The errors for this are already sent.
        update_document
            << "$pull" << open_document
                << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << open_document
                    << user_account_keys::accounts_list::OID << match_account_oid
                << close_document
            << close_document;
    }

    //NOTE: Still want to update the match 'previously matched' list here because otherwise
    // the user can see the same person immediately.
    if (num_previously_matched_elements == 0) { //if no element for 'previously matched' list

        //Upsert one to 'previously matched' list.
        update_document
            << "$push" << open_document
                << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS << open_document
                    << user_account_keys::previously_matched_accounts::OID << bsoncxx::types::b_oid{match_account_oid}
                    << user_account_keys::previously_matched_accounts::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
                    << user_account_keys::previously_matched_accounts::NUMBER_TIMES_MATCHED << bsoncxx::types::b_int32{1}
                << close_document
            << close_document;
    } else if (num_previously_matched_elements == 1) { //if one element for 'previously matched' list

        //NOTE: Needs to be added to query so the .$ operator will work.
        update_query_document
            << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS + "." + user_account_keys::previously_matched_accounts::OID << match_account_oid;

        //Increment number times matched.
        update_document
            << "$inc" << open_document
                << std::string(user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS).append(".$.").append(user_account_keys::previously_matched_accounts::NUMBER_TIMES_MATCHED)
                << bsoncxx::types::b_int32{1}
            << close_document
            << "$set" << open_document
                << std::string(user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS).append(".$.").append(user_account_keys::previously_matched_accounts::TIMESTAMP)
                << bsoncxx::types::b_date{current_timestamp}
            << close_document;

    } else { //if more than 1 element for 'previously matched' list or invalid type was returned from function

        //store error and remove all elements, it shouldn't break anything except the user can match with them again
        //if value was -1 error was already stored
        if (num_previously_matched_elements != -1) {
            const std::string error_string = "Too many elements in array " + user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS + "'.";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "matching_document", user_account_doc_view
            );
        }

        update_document
            << "$pull" << open_document
                << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS << open_document
                    << user_account_keys::previously_matched_accounts::OID << match_account_oid
                << close_document
            << close_document;
    }

    bsoncxx::stdx::optional<mongocxx::result::update> update_user_account_doc;
    try {
        update_user_account_doc = user_account_collection.update_one(
                *session,
                update_query_document.view(),
                update_document.view()
        );
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(),std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "query_doc", update_query_document.view(),
                "update_doc", update_document.view()
        );
        return false;
    }

    if (!update_user_account_doc || update_user_account_doc->modified_count() != 1) { //if update failed

        //NOTE: It is possible to not update anything here. For example, something else could access
        // the previously matched accounts and remove it. Leading to update_query_document not finding
        // anything. However, by nature this function should update the previously matched account
        // and so it should always update it.

        const std::string error_string = "Update matching collection failed.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "query_doc", update_query_document.view(),
                "update_doc", update_document.view()
        );

        return false;
    }

    return true;
}