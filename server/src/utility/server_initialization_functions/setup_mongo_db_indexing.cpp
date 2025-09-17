//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <connection_pool_global_variable.h>
#include <ErrorOriginEnum.grpc.pb.h>
#include <UserAccountStatusEnum.grpc.pb.h>
#include <specific_match_queries/specific_match_queries.h>

#include "server_initialization_functions.h"

#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "user_account_keys.h"
#include "pending_account_keys.h"
#include "info_stored_after_deletion_keys.h"
#include "email_verification_keys.h"
#include "account_recovery_keys.h"
#include "match_algorithm_results_keys.h"
#include "fresh_errors_keys.h"
#include "handled_errors_list_keys.h"
#include "individual_match_statistics_keys.h"
#include "user_account_statistics_documents_completed_keys.h"
#include "activity_suggestion_feedback_keys.h"
#include "other_suggestion_feedback_keys.h"
#include "bug_report_feedback_keys.h"
#include "outstanding_reports_keys.h"
#include "server_parameter_restrictions.h"
#include "pre_login_checkers_keys.h"
#include "UserAccountType.grpc.pb.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setupMongoDBIndexing() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database errorsDB = mongoCppClient[database_names::ERRORS_DATABASE_NAME];
    mongocxx::database statisticsInfoDB = mongoCppClient[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
    mongocxx::database feedbackDB = mongoCppClient[database_names::FEEDBACK_DATABASE_NAME];
    mongocxx::database reportsDB = mongoCppClient[database_names::REPORTS_DATABASE_NAME];
    mongocxx::database deletedDB = mongoCppClient[database_names::DELETED_DATABASE_NAME];

    mongocxx::collection adminAccountsCollection = accountsDB[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection userAccountsCollection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection pendingCollection = accountsDB[collection_names::PENDING_ACCOUNT_COLLECTION_NAME];
    mongocxx::collection userPicturesCollection = accountsDB[collection_names::USER_PICTURES_COLLECTION_NAME]; //no indexing, accessed by OID
    mongocxx::collection pre_login_checkers_collection = accountsDB[collection_names::PRE_LOGIN_CHECKERS_COLLECTION_NAME];
    mongocxx::collection infoStoredAfterDeletionCollection = accountsDB[collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME]; //indexed by phone number

    mongocxx::collection freshErrorsCollection = errorsDB[collection_names::FRESH_ERRORS_COLLECTION_NAME];
    mongocxx::collection handledErrorsCollection = errorsDB[collection_names::HANDLED_ERRORS_COLLECTION_NAME];
    mongocxx::collection handledErrorsListCollection = errorsDB[collection_names::HANDLED_ERRORS_LIST_COLLECTION_NAME];

    mongocxx::collection emailVerificationCollection = accountsDB[collection_names::EMAIL_VERIFICATION_COLLECTION_NAME];
    mongocxx::collection accountRecoveryCollection = accountsDB[collection_names::ACCOUNT_RECOVERY_COLLECTION_NAME];

    mongocxx::collection match_results_collection = statisticsInfoDB[collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME];
    mongocxx::collection individual_match_statistics_collection = statisticsInfoDB[collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME];
    mongocxx::collection user_account_statistics_documents_completed_collection = statisticsInfoDB[collection_names::USER_ACCOUNT_STATISTICS_DOCUMENTS_COMPLETED_COLLECTION_NAME];

    mongocxx::collection activity_feedback_collection = feedbackDB[collection_names::ACTIVITY_SUGGESTION_FEEDBACK_COLLECTION_NAME];
    mongocxx::collection bug_report_feedback_collection = feedbackDB[collection_names::BUG_REPORT_FEEDBACK_COLLECTION_NAME];
    mongocxx::collection other_suggestion_feedback_collection = feedbackDB[collection_names::OTHER_SUGGESTION_FEEDBACK_COLLECTION_NAME];

    mongocxx::collection outstanding_reports_collection = reportsDB[collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME];

    mongocxx::collection deleted_accounts_collection = deletedDB[collection_names::DELETED_DELETED_ACCOUNTS_COLLECTION_NAME];

    mongocxx::options::index uniqueIndex{};
    uniqueIndex.unique(true);
    //setting up the index in the background so servers can be started without blocking the collections to check
    // and see if the index is set up
    uniqueIndex.background(true);

    std::cout << "Started setting up database indexing.\n";

    //Simple indexing rules
    //1 First, add those fields against which Equality queries are run.
    //2 The next fields to be indexed should reflect the Sort order of the query.
    //3 The last fields represent the Range of data to be accessed.

    adminAccountsCollection.create_index(
            document{}
                    << admin_account_key::NAME << 1
                    << admin_account_key::PASSWORD << 1
            << finalize,
            uniqueIndex
    );

    userAccountsCollection.create_index(
            document{}
                    << user_account_keys::PHONE_NUMBER << 1 //phone number
            << finalize,
            uniqueIndex
    );

    userAccountsCollection.create_index(
            document{}
                    << user_account_keys::ACCOUNT_ID_LIST << 1 //array of account Ids
            << finalize
    );

    mongocxx::options::index algorithm_index_options{};

    bsoncxx::builder::stream::document algorithm_partial_filter_doc;
    matchingIsActivatedOnMatchAccount(algorithm_partial_filter_doc);

    //Exclude for the algorithm index elements that will always evaluate to the same thing.
    //NOTE: This MUST be included in the algorithm query even though it is excluded from the index. If
    // it is not included inside the query then it will not use the compound index.
    algorithm_index_options.partial_filter_expression(algorithm_partial_filter_doc.view());
    algorithm_index_options.name(user_account_collection_index::ALGORITHM_INDEX_NAME);

    //setting up the index in the background so servers can be started without blocking the collections to check
    // and see if the index is set up
    algorithm_index_options.background(true);

    //ALGORITHM INDEX
    //This index should only ever be used by the algorithm. Everything else has an _id field or a phone number, even
    // documents that require calling universalMatchConditions() function to generate a document.
    //NOTE: LOCATION is at the top because of selectivity. CATEGORIES are at the bottom because they take up the least
    // room in the index that way.  The other fields are arbitrary, although GENDER is 2nd because it is an equality
    // check and not a range check.
    //NOTE: If the index is getting too large (so large it can't comfortably fit into RAM). Then the CATEGORIES fields can
    // be removed from the index. They take up over half of the space requires for the array. And if there are only ~80 values
    // for activities, they are not NEEDED for selectivity. However, it will reduce performance.
    //NOTE: Order, there are 'schools of though' on compound index ordering.
    // 1) Most selective first;
    // 2) ESR (listed below);
    //  -First, add those fields against which Equality queries are run.
    //  -The next fields to be indexed should reflect the Sort order of the query.
    //  -The last fields represent the Range of data to be accessed.
    //NOTE: When the algorithm is run and searches for events only, it will not put CATEGORIES in the query. So keeping
    // it last in the compound index is important.
    userAccountsCollection.create_index(
            document{}
                    << user_account_keys::LOCATION << "2dsphere" //probably be most selective with large data sets
                    << user_account_keys::GENDER << 1 // fairly selective; equality comparison (male and female can cut it in half, other will probably be rare)
                    << user_account_keys::AGE << 1 // fairly selective
                    << user_account_keys::AGE_RANGE + '.' + user_account_keys::age_range::MAX << 1 // fairly selective; works with age_range::MIN in a sense
                    << user_account_keys::AGE_RANGE + '.' + user_account_keys::age_range::MIN << 1 // fairly selective; works with age_range::MAX in a sense
                    << user_account_keys::EVENT_EXPIRATION_TIME << 1 // fairly selective; if it is set to event_expiration_time_values::USER_ACCOUNT it will be accepted as a user, if it is set to event_expiration_time_values::EVENT_CANCELED it is a canceled event.
                    // << user_account_keys::GENDERS_RANGE << 1 // fairly selective; is array field so mutually exclusive w/ CATEGORIES
                    << user_account_keys::CATEGORIES + '.' + user_account_keys::categories::INDEX_VALUE << 1 //sort of selective; the array being last seems to take up less memory; works with categories::TYPE in a sense
                    << user_account_keys::CATEGORIES + '.' + user_account_keys::categories::TYPE << 1 //sort of selective; works with categories::INDEX_VALUE in a sense
                    // << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS + '.' + user_account_keys::previously_matched_accounts::OID << 1 //creates a very large index AND is array field so mutually exclusive w/ CATEGORIES
                    // << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS + '.' + user_account_keys::previously_matched_accounts::TIMESTAMP << 1 //creates a very large index AND is array field so mutually exclusive w/ CATEGORIES
                    // << user_account_keys::OTHER_USERS_BLOCKED + '.' + user_account_keys::other_users_blocked::OID_STRING << 1 //creates a very large index AND is array field so mutually exclusive w/ CATEGORIES
            << finalize,
            algorithm_index_options
    );

    mongocxx::options::index events_index_options{};

    bsoncxx::builder::stream::document events_partial_filter_doc;
    events_partial_filter_doc
        << user_account_keys::ACCOUNT_TYPE << open_document
            << "$gte" << UserAccountType::ADMIN_GENERATED_EVENT_TYPE
        << close_document;

    //$gte ADMIN_GENERATED_EVENT_TYPE should only be events. The CREATED_BY field will only work for events, so this
    // should work out well.
    //NOTE: Will get a segfault if trying to create this index in place.
    events_index_options.partial_filter_expression(events_partial_filter_doc.view());
    events_index_options.background(true);

    //This index is user for searching events.
    userAccountsCollection.create_index(
        document{}
            << user_account_keys::ACCOUNT_TYPE << 1
            << user_account_keys::TIME_CREATED << 1
            << user_account_keys::EVENT_EXPIRATION_TIME << 1
            << user_account_keys::event_values::CREATED_BY << 1 //This can be excluded from the query, so it must go last.
        << finalize,
        events_index_options
    );

    //NOTE: See login_function/_documentation.md for more info on why indexing is done this way for the pending account
    // collection.

    pendingCollection.create_index(
            document{}
                   << pending_account_keys::INDEXING << 1
            << finalize,
            uniqueIndex
    );

    pendingCollection.create_index(
            document{}
                    << pending_account_keys::ID << 1
            << finalize,
            uniqueIndex
    );

    pendingCollection.create_index(
            document{}
                    << pending_account_keys::PHONE_NUMBER << 1
            << finalize,
            uniqueIndex
    );

    mongocxx::options::index pendingExpirationIndex{};
    pendingExpirationIndex.background(true);
    pendingExpirationIndex.expire_after(std::chrono::duration_cast<std::chrono::seconds>(
            general_values::TIME_UNTIL_PENDING_ACCOUNT_REMOVED)); //make document be deleted after TIME_UNTIL_PENDING_ACCOUNT_REMOVED

    pendingCollection.create_index(
            document{}
                   << pending_account_keys::TIME_VERIFICATION_CODE_WAS_SENT << 1 //time the verification code was sent
            << finalize,
            pendingExpirationIndex
    );

    infoStoredAfterDeletionCollection.create_index(
            document{}
                    << info_stored_after_deletion_keys::PHONE_NUMBER << 1 //phone number
            << finalize,
            uniqueIndex
    );

    emailVerificationCollection.create_index(
            document{}
                    << email_verification_keys::VERIFICATION_CODE << 1 //verification code
            << finalize,
            uniqueIndex
    );

    mongocxx::options::index emailVerificationExpirationIndex{};
    emailVerificationExpirationIndex.background(true);
    emailVerificationExpirationIndex.expire_after(general_values::TIME_UNTIL_EMAIL_VERIFICATION_ACCOUNT_REMOVED);

    emailVerificationCollection.create_index(
            document{}
                    << email_verification_keys::TIME_VERIFICATION_CODE_GENERATED << 1 //time the verification code was sent
            << finalize,
            emailVerificationExpirationIndex
    );

    accountRecoveryCollection.create_index(
            document{}
                    << account_recovery_keys::VERIFICATION_CODE << 1 //verification code
            << finalize,
            uniqueIndex
    );

    accountRecoveryCollection.create_index(
            document{}
                    << account_recovery_keys::PHONE_NUMBER << 1 //phone number
            << finalize,
            uniqueIndex
    );

    mongocxx::options::index accountRecoveryExpirationIndex{};
    accountRecoveryExpirationIndex.background(true);
    accountRecoveryExpirationIndex.expire_after(
            general_values::TIME_UNTIL_ACCOUNT_RECOVERY_REMOVED); //make document be deleted after TIME_UNTIL_EMAIL_VERIFICATION_EXPIRES

    accountRecoveryCollection.create_index(
            document{}
                    << account_recovery_keys::TIME_VERIFICATION_CODE_GENERATED << 1 //verification code
            << finalize,
           accountRecoveryExpirationIndex
   );

    match_results_collection.create_index(
            document{}
                    << match_algorithm_results_keys::GENERATED_FOR_DAY << 1
                    << match_algorithm_results_keys::ACCOUNT_TYPE << 1
                    << match_algorithm_results_keys::CATEGORIES_TYPE << 1
                    << match_algorithm_results_keys::CATEGORIES_VALUE << 1
            << finalize,
            uniqueIndex
    );

    individual_match_statistics_collection.create_index(
            document{}
                    << individual_match_statistics_keys::DAY_TIMESTAMP << 1
            << finalize
    );

    user_account_statistics_documents_completed_collection.create_index(
            document{}
                    << user_account_statistics_documents_completed_keys::PREVIOUS_ID << 1
            << finalize
    );

    activity_feedback_collection.create_index(
            document{}
                    << activity_suggestion_feedback_keys::TIMESTAMP_STORED << 1
            << finalize
    );

    bug_report_feedback_collection.create_index(
            document{}
                    << bug_report_feedback_keys::TIMESTAMP_STORED << 1
            << finalize
    );

    other_suggestion_feedback_collection.create_index(
            document{}
                    << other_suggestion_feedback_keys::TIMESTAMP_STORED << 1
            << finalize
    );

    outstanding_reports_collection.create_index(
            document{}
                    << outstanding_reports_keys::TIMESTAMP_LIMIT_REACHED << 1
            << finalize
    );

    bsoncxx::document::value errors_indexing_doc = document{}
                    << fresh_errors_keys::ERROR_ORIGIN << 1 //equality ALWAYS checked, but large 'chunks'
                    << fresh_errors_keys::VERSION_NUMBER << 1 //Range PART OF $group
                    << fresh_errors_keys::FILE_NAME << 1 //equality PART OF $group
                    << fresh_errors_keys::LINE_NUMBER << 1 //equality PART OF $group
                    << fresh_errors_keys::ERROR_URGENCY << 1 //equality ALWAYS checked, but large 'chunks'
                    << fresh_errors_keys::DEVICE_NAME << 1 //equality
                    << fresh_errors_keys::TIMESTAMP_STORED << 1 //Range
                    << fresh_errors_keys::API_NUMBER << 1 //Range
            << finalize;

    //Because the searchErrors function runs $sort first to be more efficient with the $group, the $sort
    // will need to be on version_number - file_name - line_number, so indexing them
    // to be the first three index values
    freshErrorsCollection.create_index(errors_indexing_doc.view());

    //index in reverse order because the $sort is in reverse order
    freshErrorsCollection.create_index(
            document{}
                    << fresh_errors_keys::TIMESTAMP_STORED << -1
            << finalize
    );

    handledErrorsCollection.create_index(errors_indexing_doc.view());

    handledErrorsListCollection.create_index(
            document{}
                << handled_errors_list_keys::ERROR_ORIGIN << 1
                << handled_errors_list_keys::VERSION_NUMBER << 1
                << handled_errors_list_keys::FILE_NAME << 1
                << handled_errors_list_keys::LINE_NUMBER << 1
            << finalize,
            uniqueIndex
    );

    //NOTE: $type and $exists are redundant, the $type will be missing if $exists == false
    //NOTE: I assume type checking needs to go first before checking specific properties of the type
    //NOTE: The validator will run every time an update runs on a document. This means if each array element is
    // checked each time it should be a bit expensive. Therefore, except categories (because it is
    // used in the matching algorithm), only the top level types are checked.
    //NOTE: May want to test performance without this validator at some point, it may be expensive (from NOTE above).
    accountsDB.run_command(
        document{}
            << "collMod" << collection_names::USER_ACCOUNTS_COLLECTION_NAME
            << "validator" << open_document
                << "$and" << open_array

                    << open_document
                        << user_account_keys::STATUS << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                       << user_account_keys::INACTIVE_MESSAGE << open_document
                            << "$type" << "string"
                        << close_document
                    << close_document

                    << open_document
                        << "$expr" << open_document
                            << "$or" << open_array

                                << open_document
                                    << "$eq" << open_array
                                        << open_document
                                           << "$type" << "$" + user_account_keys::INACTIVE_MESSAGE
                                        << close_document
                                       << "null"
                                    << close_array
                                << close_document

                                << open_document
                                    << "$eq" << open_array
                                        << open_document
                                           << "$type" << "$" + user_account_keys::INACTIVE_MESSAGE
                                        << close_document
                                       << "string"
                                    << close_array
                                << close_document

                            << close_array
                        << close_document
                    << close_document

                    << open_document
                       << user_account_keys::NUMBER_OF_TIMES_TIMED_OUT << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                       << user_account_keys::INSTALLATION_IDS << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LAST_VERIFIED_TIME << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::TIME_CREATED << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::ACCOUNT_ID_LIST << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::PHONE_NUMBER << open_document
                            << "$type" << "string"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::FIRST_NAME << open_document
                            << "$type" << "string"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::BIO << open_document
                            << "$type" << "string"
                        << close_document
                    << close_document
                    << open_document
                        << user_account_keys::CITY << open_document
                            << "$type" << "string"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::GENDER << open_document
                            << "$type" << "string"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::BIRTH_YEAR << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::BIRTH_MONTH << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::BIRTH_DAY_OF_MONTH << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::BIRTH_DAY_OF_YEAR << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::AGE << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::EMAIL_ADDRESS << open_document
                            << "$type" << "string"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION << open_document
                            << "$type" << "bool"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::PICTURES << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::SEARCH_BY_OPTIONS << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << "$expr" << open_document

                            //NOTE:  This is done with a nested condition because the $and statement does not
                            // short circuit.
                            << "$cond" << open_document

                                << "if" << open_document
                                    << "$eq" << open_array
                                        << open_document
                                            << "$type" << "$" + user_account_keys::CATEGORIES
                                        << close_document
                                        << "array"
                                    << close_array
                                << close_document

                                << "then" << open_document

                                    << "$and" << open_array
                                        << open_document
                                            << "$gt" << open_array
                                                << open_document
                                                    << "$size" << "$" + user_account_keys::CATEGORIES
                                                << close_document
                                                << 0
                                            << close_array
                                        << close_document

                                        << open_document
                                            << "$reduce" << open_document
                                                << "input" << "$" + user_account_keys::CATEGORIES
                                                << "initialValue" << true
                                                << "in" << open_document
                                                    << "$cond" << open_document

                                                        << "if" << open_document
                                                            << "$and" << open_array

                                                                << open_document
                                                                    << "$eq" << open_array
                                                                        << open_document
                                                                            << "$type" << "$$this." + user_account_keys::categories::TYPE
                                                                        << close_document
                                                                        << "int"
                                                                    << close_array
                                                                << close_document

                                                                << open_document
                                                                    << "$eq" << open_array
                                                                        << open_document
                                                                            << "$type" << "$$this." + user_account_keys::categories::INDEX_VALUE
                                                                        << close_document
                                                                        << "int"
                                                                    << close_array
                                                                << close_document

                                                                //NOTE: Using a condition here instead of $and because it does not support
                                                                // short-circuiting.
                                                                << open_document
                                                                    << "$cond" << open_document

                                                                    //make sure TIMEFRAMES is correct type
                                                                    << "if" << open_document
                                                                        << "$eq" << open_array
                                                                            << open_document
                                                                                << "$type" << "$$this." + user_account_keys::categories::TIMEFRAMES
                                                                            << close_document
                                                                            << "array"
                                                                        << close_array
                                                                    << close_document

                                                                    //make sure TIMEFRAMES contains correct types
                                                                    << "then" << open_document
                                                                        << "$reduce" << open_document
                                                                            << "input" << "$$this." + user_account_keys::categories::TIMEFRAMES
                                                                            << "initialValue" << true
                                                                            << "in" << open_document
                                                                                << "$cond" << open_document

                                                                                    << "if" << open_document

                                                                                        << "$and" << open_array

                                                                                            << open_document
                                                                                                << "$eq" << open_array
                                                                                                    << open_document
                                                                                                        << "$type" << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                                                    << close_document
                                                                                                << "long"
                                                                                                << close_array
                                                                                            << close_document

                                                                                            << open_document
                                                                                                << "$eq" << open_array
                                                                                                    << open_document
                                                                                                        << "$type" << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                                                    << close_document
                                                                                                    << "int"
                                                                                                << close_array
                                                                                            << close_document

                                                                                        << close_array

                                                                                    << close_document
                                                                                    << "then" << "$$value"
                                                                                    << "else" << false

                                                                                << close_document
                                                                            << close_document
                                                                        << close_document

                                                                    << close_document

                                                                    << "else" << false

                                                                    << close_document
                                                                << close_document

                                                            << close_array
                                                        << close_document
                                                        << "then" << "$$value"
                                                        << "else" << false

                                                    << close_document
                                                << close_document
                                            << close_document
                                        << close_document

                                    << close_array

                                << close_document

                                << "else" << false

                            << close_document

                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::MATCHING_ACTIVATED << open_document
                            << "$type" << "bool"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LOCATION << open_document
                            << "$type" << "object"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::AGE_RANGE << open_document
                            << "$type" << "object"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::GENDERS_RANGE << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::MAX_DISTANCE << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LAST_VERIFIED_TIME << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LAST_TIME_MATCH_ALGORITHM_RAN << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::INT_FOR_MATCH_LIST_TO_DRAW_FROM << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::TOTAL_NUMBER_MATCHES_DRAWN << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::TIME_SMS_CAN_BE_SENT_AGAIN << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_SWIPES_REMAINING << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::SWIPES_LAST_UPDATED_TIME << open_document
                            << "$type" << "long"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LOGGED_IN_TOKEN << open_document
                            << "$type" << "string"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LOGGED_IN_TOKEN_EXPIRATION << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LOGGED_IN_INSTALLATION_ID << open_document
                            << "$type" << "string"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::COOL_DOWN_ON_SMS_RETURN_MESSAGE << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::LOGGED_IN_RETURN_MESSAGE << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::BIRTHDAY_TIMESTAMP << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::GENDER_TIMESTAMP << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::FIRST_NAME_TIMESTAMP << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::BIO_TIMESTAMP << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::CITY_NAME_TIMESTAMP << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::EMAIL_TIMESTAMP << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::CATEGORIES_TIMESTAMP << open_document
                            << "$type" << "date"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::CHAT_ROOMS << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::OTHER_USERS_BLOCKED << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_SWIPED_YES << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_SWIPED_NO << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_SWIPED_BLOCK << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_SWIPED_REPORT << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_BLOCK << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_REPORT << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_SENT_ACTIVITY_SUGGESTION << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_SENT_BUG_REPORT << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_SENT_OTHER_SUGGESTION << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_REPORTED_BY_OTHERS_IN_CHAT_ROOM << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_BLOCKED_BY_OTHERS_IN_CHAT_ROOM << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_THIS_USER_REPORTED_FROM_CHAT_ROOM << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::NUMBER_TIMES_THIS_USER_BLOCKED_FROM_CHAT_ROOM << open_document
                            << "$type" << "int"
                        << close_document
                    << close_document

                    << open_document
                        << user_account_keys::DISCIPLINARY_RECORD << open_document
                            << "$type" << "array"
                        << close_document
                    << close_document

                << close_array
            << close_document
        << finalize);

    //NOTE: $type and $exists are redundant, the $type will be missing if $exists == false
    //NOTE: I assume type checking needs to go first before checking specific properties of the type
    //NOTE: Both the web server and the server itself can add to this collection, so putting a schema on it.
    errorsDB.run_command(
            document{}
                 << "collMod" << collection_names::FRESH_ERRORS_COLLECTION_NAME
                 << "validator" << open_document
                     << "$and" << open_array

                         //ERROR_ORIGIN
                         << open_document
                             << fresh_errors_keys::ERROR_ORIGIN << open_document
                                 << "$type" << "int"
                                 << "$gte" << ErrorOriginType_MIN
                                 << "$lte" << ErrorOriginType_MAX
                             << close_document
                         << close_document

                         //ERROR_URGENCY
                         << open_document
                            << fresh_errors_keys::ERROR_URGENCY << open_document
                                 << "$type" << "int"
                                 << "$gte" << ErrorUrgencyLevel_MIN
                                 << "$lte" << ErrorUrgencyLevel_MAX
                             << close_document
                         << close_document

                         //VERSION_NUMBER
                         << open_document
                             << fresh_errors_keys::VERSION_NUMBER << open_document
                                 << "$type" << "int"
                                 << "$gt" << 0
                             << close_document
                         << close_document

                         //FILE_NAME
                         << open_document
                             << "$expr" << open_document
                                 << "$and" << open_array
                                     << open_document
                                         << "$eq" << open_array
                                             << open_document
                                                << "$type" << "$" + fresh_errors_keys::FILE_NAME
                                             << close_document
                                            << "string"
                                         << close_array
                                     << close_document
                                     << open_document
                                         << "$lte" << open_array
                                             << open_document
                                                << "$strLenBytes" << "$" + fresh_errors_keys::FILE_NAME
                                             << close_document
                                             << server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES
                                         << close_array
                                     << close_document
                                 << close_array
                             << close_document
                         << close_document

                         //LINE_NUMBER
                         << open_document
                             << fresh_errors_keys::LINE_NUMBER << open_document
                                 << "$type" << "int"
                                 << "$gte" << 0
                             << close_document
                         << close_document

                         //STACK_TRACE
                         << open_document
                             << "$expr" << open_document
                                 << "$and" << open_array
                                     << open_document
                                         << "$eq" << open_array
                                             << open_document
                                                << "$type" << "$" + fresh_errors_keys::STACK_TRACE
                                             << close_document
                                            << "string"
                                         << close_array
                                     << close_document
                                     << open_document
                                         << "$lte" << open_array
                                             << open_document
                                                << "$strLenBytes" << "$" + fresh_errors_keys::STACK_TRACE
                                             << close_document
                                            << server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE
                                         << close_array
                                     << close_document
                                 << close_array
                             << close_document
                         << close_document

                         //TIMESTAMP_STORED
                         << open_document
                             << fresh_errors_keys::TIMESTAMP_STORED << open_document
                                << "$type" << "date"
                             << close_document
                         << close_document

                         //API_NUMBER
                         << open_document
                             << "$or" << open_array
                                 << open_document
                                     << fresh_errors_keys::API_NUMBER << open_document
                                         << "$type" << "int"
                                         << "$gt" << 0
                                     << close_document
                                 << close_document
                                 << open_document
                                     << fresh_errors_keys::API_NUMBER << open_document
                                        << "$exists" << false
                                     << close_document
                                 << close_document
                             << close_array
                         << close_document

                         //DEVICE_NAME
                         << open_document
                             << "$expr" << open_document
                                 << "$or" << open_array
                                     << open_document
                                         << "$eq" << open_array
                                             << open_document
                                                << "$type" << "$" + fresh_errors_keys::DEVICE_NAME
                                             << close_document
                                             << "missing"
                                         << close_array
                                     << close_document
                                     << open_document
                                         << "$and" << open_array
                                             << open_document
                                                 << "$eq" << open_array
                                                     << open_document
                                                        << "$type" << "$" + fresh_errors_keys::DEVICE_NAME
                                                     << close_document
                                                     << "string"
                                                 << close_array
                                             << close_document
                                             << open_document
                                                 << "$lte" << open_array
                                                     << open_document
                                                        << "$strLenBytes" << "$" + fresh_errors_keys::DEVICE_NAME
                                                     << close_document
                                                     << server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE
                                                 << close_array
                                             << close_document
                                         << close_array
                                     << close_document
                                 << close_array
                             << close_document
                         << close_document

                         //ERROR_MESSAGE
                         << open_document
                             << "$expr" << open_document
                                 << "$and" << open_array
                                     << open_document
                                         << "$eq" << open_array
                                             << open_document
                                                << "$type" << "$" + fresh_errors_keys::ERROR_MESSAGE
                                             << close_document
                                             << "string"
                                         << close_array
                                     << close_document
                                     << open_document
                                         << "$gte" << open_array
                                             << open_document
                                                << "$strLenBytes" << "$" + fresh_errors_keys::ERROR_MESSAGE
                                             << close_document
                                             << server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE
                                         << close_array
                                     << close_document
                                     << open_document
                                         << "$lte" << open_array
                                             << open_document
                                                << "$strLenBytes" << "$" + fresh_errors_keys::ERROR_MESSAGE
                                             << close_document
                                             << server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE
                                         << close_array
                                     << close_document
                                 << close_array
                             << close_document
                         << close_document

                     << close_array
                 << close_document
            << finalize);

    std::cout << "Finished setting up database indexing.\n";
}
