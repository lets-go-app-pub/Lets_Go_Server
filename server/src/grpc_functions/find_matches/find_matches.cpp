//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>

#include "find_matches.h"
#include "utility_general_functions.h"
#include "find_matches_helper_objects.h"
#include "helper_functions/find_matches_helper_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "matching_algorithm.h"
#include "general_values.h"
#include "FindMatches.pb.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

/** Documentation can be found inside [grpc_functions/find_matches/_documentation.md]. **/

void findMatchesImplementation(
        const findmatches::FindMatchesRequest* request,
        grpc::ServerWriterInterface<findmatches::FindMatchesResponse>* response
);

inline void addFunctionUnlockDocument(bsoncxx::builder::stream::document& update_doc) {
    update_doc
        << "$set" << open_document
            << user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK << bsoncxx::types::b_date{std::chrono::milliseconds {-1L}}
        << close_document;
}

inline void addCooldownToResponseMessage(
        findmatches::FindMatchesCapMessage* response_message,
        const long cooldown
) {
    response_message->set_return_status(ReturnStatus::SUCCESS);
    response_message->set_success_type(
            findmatches::FindMatchesCapMessage_SuccessTypes_MATCH_ALGORITHM_ON_COOL_DOWN
    );
    response_message->set_cool_down_on_match_algorithm(cooldown);
}

inline void addNoMatchesFoundToResponseMessage(
        findmatches::FindMatchesCapMessage* response_message,
        const long cooldown
) {
    response_message->set_return_status(ReturnStatus::SUCCESS);
    response_message->set_success_type(
            findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND
    );
    response_message->set_cool_down_on_match_algorithm(cooldown);
}

inline void setLgErrorToResponseMessage(
        findmatches::FindMatchesCapMessage* response_message
) {
    response_message->set_return_status(ReturnStatus::LG_ERROR);
    response_message->set_success_type(findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN);
}

void findMatches(
        const findmatches::FindMatchesRequest* request,
        grpc::ServerWriterInterface<findmatches::FindMatchesResponse>* response
) {
    handleFunctionOperationException(
            [&] {
                findMatchesImplementation(request, response);
            },
            [&] {
                findmatches::FindMatchesResponse response_message;
                response_message.mutable_find_matches_cap()->set_return_status(ReturnStatus::DATABASE_DOWN);
                response_message.mutable_find_matches_cap()->set_success_type(findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN);
                response->Write(response_message);
            },
            [&] {
                findmatches::FindMatchesResponse response_message;
                setLgErrorToResponseMessage(response_message.mutable_find_matches_cap());
                response->Write(response_message);
            }, __LINE__, __FILE__, request);
}

void findMatchesImplementation(
        const findmatches::FindMatchesRequest* request,
        grpc::ServerWriterInterface<findmatches::FindMatchesResponse>* response
) {

    //NOTE: The calls to mongoDB are sys calls, and so timing thread and cpu
    // won't give good results.
    timespec
            function_start_ts{},
            function_stop_ts{},
            transaction_start_ts{},
            transaction_stop_ts{};

    clock_gettime(CLOCK_REALTIME, &function_start_ts);

    size_t number_responses_to_send;

    if (request->number_messages() < 1) {
        findmatches::FindMatchesResponse success_message;
        success_message.mutable_find_matches_cap()->set_return_status(ReturnStatus::SUCCESS);
        success_message.mutable_find_matches_cap()->set_success_type(findmatches::FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED);
        success_message.mutable_find_matches_cap()->set_timestamp(getCurrentTimestamp().count());
        response->Write(success_message);
        return;
    }
    else if (request->number_messages() > matching_algorithm::MAXIMUM_NUMBER_RESPONSE_MESSAGES) {
        number_responses_to_send = matching_algorithm::MAXIMUM_NUMBER_RESPONSE_MESSAGES;
    }
    else {
        number_responses_to_send = request->number_messages();
    }

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    auto send_error_message_to_client = [&response](const ReturnStatus& return_status) {
        findmatches::FindMatchesResponse error_message;
        error_message.mutable_find_matches_cap()->set_return_status(return_status);
        error_message.mutable_find_matches_cap()->set_success_type(findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN);
        response->Write(error_message);
    };

    {

        ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS) {
            send_error_message_to_client(basic_info_return_status);
            return;
        }

        if (isInvalidLocation(request->client_longitude(), request->client_latitude())) {
            send_error_message_to_client(ReturnStatus::INVALID_PARAMETER_PASSED);
            return;
        }

    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    //Location coordinates are checked above to be valid.
    UserAccountValues user_account_values{
            bsoncxx::oid{user_account_oid_str},
            request->client_longitude(),
            request->client_latitude(),
            current_timestamp,
            current_timestamp + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED,
            current_timestamp + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES,
    };

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    {
        mongocxx::stdx::optional<bsoncxx::document::value> find_and_update_user_account;

        bsoncxx::builder::stream::document merge_document;
        //set lock if able (see FIND_MATCHES_TIMESTAMP_LOCK for details)
        merge_document
                << user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK << bsoncxx::types::b_date{
                user_account_values.current_timestamp + matching_algorithm::TIME_TO_LOCK_FIND_MATCHES_TO_PREVENT_DOUBLE_RUNS
                };

        std::shared_ptr<std::vector<bsoncxx::document::value>> append_to_and_statement_doc = std::make_shared<std::vector<bsoncxx::document::value>>();

        //make sure not already locked (see FIND_MATCHES_TIMESTAMP_LOCK for details)
        bsoncxx::document::value matching_function_not_locked = document{}
                        << "$lte" << open_array
                                << "$" + user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK
                                << bsoncxx::types::b_date{user_account_values.current_timestamp}
                        << close_array
                << finalize;

        append_to_and_statement_doc->emplace_back(matching_function_not_locked.view());

        //NOTE: User info is extracted inside the transaction for consistency.
        bsoncxx::document::value projection_document = document{}
                        << "_id" << 1
                        << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                << finalize;

        bsoncxx::document::value login_document = getLoginDocument<false>(
                login_token_str,
                installation_id,
                merge_document,
                user_account_values.current_timestamp,
                append_to_and_statement_doc
        );

        //NOTE: Because FIND_MATCHES_TIMESTAMP_LOCK must be locked OUTSIDE the transaction (or it will not run until
        // the transaction is complete) the login is also run outside the transaction
        if (!runInitialLoginOperation(
                find_and_update_user_account,
                user_accounts_collection,
                user_account_values.user_account_oid,
                login_document,
                projection_document)
                ) {
            findmatches::FindMatchesResponse response_message;
            setLgErrorToResponseMessage(response_message.mutable_find_matches_cap());
            response->Write(response_message);
            return;
        }

        const ReturnStatus login_return_status = checkForValidLoginToken(
                find_and_update_user_account,
                user_account_values.user_account_oid.to_string()
        );

        if (login_return_status == ReturnStatus::UNKNOWN) {
#ifndef _RELEASE
            std::cout << "findMatches() still running, returning on cool down.\n";
#endif
            findmatches::FindMatchesResponse response_message;
            //Returning the entire cool down, if it was reached an error occurred anyway, so a few extra seconds
            // of waiting may be helpful.
            addCooldownToResponseMessage(
                    response_message.mutable_find_matches_cap(),
                    matching_algorithm::TIME_TO_LOCK_FIND_MATCHES_TO_PREVENT_DOUBLE_RUNS.count()
            );
            response_message.mutable_find_matches_cap()->set_timestamp(user_account_values.current_timestamp.count());

            response->Write(response_message);
            return;
        } else if (login_return_status != ReturnStatus::SUCCESS) {
            send_error_message_to_client(login_return_status);
            return;
        }

    }

    //If less than number_responses_to_send number of matches are sent back to the client. This value
    // should be set to something like cooldown or error in order to let the client know why.
    findmatches::FindMatchesResponse find_matches_response_for_cap_message;
    findmatches::FindMatchesCapMessage* cap_response_message = find_matches_response_for_cap_message.mutable_find_matches_cap();
    cap_response_message->set_return_status(ReturnStatus::SUCCESS);
    cap_response_message->set_success_type(findmatches::FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED);
    cap_response_message->set_timestamp(current_timestamp.count());

    auto set_response_as_error_message_to_client =
            [&cap_response_message](
                    const ReturnStatus& return_status,
                    const findmatches::FindMatchesCapMessage::SuccessTypes& success_type
            ) {
                cap_response_message->set_return_status(return_status);
                cap_response_message->set_success_type(success_type);
            };

    //This is just used to store the user account. It must stay alive until user_account_values is
    // no longer used because it holds the value for UserAccountValues::user_account_doc_view.
    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account_doc;
    bool first_run = true;

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* session) {

#ifndef LG_TESTING
        if(!first_run)
#endif
        {
            // If the transaction retries, then the members inside user_account_values are not reset. This block will clear
            // anything the transaction generates.
            user_account_values.clearNonConstValues();
            find_user_account_doc = bsoncxx::stdx::optional<bsoncxx::document::value>();
            cap_response_message->Clear();
            cap_response_message->set_return_status(ReturnStatus::SUCCESS);
            cap_response_message->set_success_type(findmatches::FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED);
            cap_response_message->set_timestamp(current_timestamp.count());
        }
#ifndef LG_TESTING
        else
#endif
        {
            first_run = false;
        }

        //Extract user info from user account and update information inside the user account for the algorithm.
        if(!extractAndSaveUserInfo(
                user_account_values,
                user_accounts_collection,
                session,
                set_response_as_error_message_to_client,
                find_user_account_doc)
        ) { //a failure occurred
            //NOTE: Any errors are handled and the response is set inside the function (Don't set LG_ERROR here or anything)

            bsoncxx::builder::stream::document update_document;
            addFunctionUnlockDocument(update_document);

            try {
                //'unlock' the matching algorithm for the user
                user_accounts_collection.update_one(
                    *session,
                    document{}
                        << "_id" << user_account_values.user_account_oid
                    << finalize,
                    update_document.view()
                );
            }
            catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "match_OID_used", user_account_values.user_account_oid,
                        "timestamp", getDateTimeStringFromTimestamp(user_account_values.current_timestamp)
                );

                //clear saved matches so that it will not send any back
                setLgErrorToResponseMessage(cap_response_message);
            }

            //NOTE: Do not abort here, want the 'unlock' to run.
            return;
        }

        ValidateArrayElementReturnEnum return_val;
        int number_tries_remaining = matching_algorithm::ARRAY_ELEMENTS_EXTRACTED_BEFORE_GIVE_UP;
        bool continue_loop = true; //this will stop the loop when it is set to false

        bool other_users_said_yes_empty = false; //this will be set to true if the 'other users matches' list is empty
        bool algorithm_matches_empty = false; //this will only be set to true if the algorithm returns no matches when the list is empty
        bool searched_for_only_events = false; //this will only be set to true if the algorithm returns no events when both lists are empty

        //This will be set changed to a different value if the algorithm runs. This keeps track of if the algorithm has already run.
        FinalAlgorithmResults algorithm_results = FinalAlgorithmResults::algorithm_did_not_run;

        //stops when
        // 1) loop is canceled internally
        // 2) too many requests attempted
        // 3) number of requested matches has been extracted
        // 4) user does not have enough swipes remaining to continue drawing matches
        while (continue_loop
               && number_tries_remaining > 0
               && user_account_values.saved_matches.size() < number_responses_to_send
               && user_account_values.number_swipes_remaining > 0
        ) {

            if (other_users_said_yes_empty && algorithm_matches_empty) {

                //If no valid matches found, do a loose search for any events in the area.
                if(user_account_values.saved_matches.empty()
                    && !searched_for_only_events
                    && algorithm_results == FinalAlgorithmResults::set_no_matches_found) {

                    searched_for_only_events = true;
                    algorithm_results = algorithm_successful;

                    //run algorithm
                    //This function can be called directly because in order to get here, the algorithm had to
                    // do the setup below when beginAlgorithmCheckCoolDown() was called.
                    const AlgorithmReturnValues algorithm_return_values = runMatchingAlgorithmAndReturnResults(
                            user_account_values,
                            user_accounts_collection,
                            true
                    );

                    switch (algorithm_return_values.returnMessage) {
                        case error: { //if algorithm failed
                            setLgErrorToResponseMessage(cap_response_message);
                            continue_loop = false;
                            break;
                        }
                        case no_matches_found: {
                            addNoMatchesFoundToResponseMessage(
                                    cap_response_message,
                                    algorithm_return_values.coolDownOnMatchAlgorithm.count()
                            );

                            //no matches were found by the algorithm twice
                            continue_loop = false;
                            algorithm_results = FinalAlgorithmResults::set_no_matches_found;
                            break;
                        }
                        case match_algorithm_returned_empty_recently:
                        case match_algorithm_ran_recently: {

                            const std::string error_string = "An algorithm on cooldown type message was returned when"
                                                             " running runMatchingAlgorithmAndReturnResults(). This"
                                                             " should not be possible.";

                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    std::optional<std::string>(), error_string,
                                    "number_tries_remaining", std::to_string(number_tries_remaining),
                                    "number_swipes_remaining", std::to_string(user_account_values.number_swipes_remaining),
                                    "user_oid", user_account_values.user_account_oid,
                                    "algorithm_return_values.returnMessage", algorithm_return_values.returnMessage
                            );

                            addCooldownToResponseMessage(
                                    cap_response_message,
                                    algorithm_return_values.coolDownOnMatchAlgorithm.count()
                            );

                            //this can mean
                            // the match ran recently OR
                            // the match algorithm ran recently, returned an empty match and had no changes to the search criteria by the user

                            continue_loop = false;

                            //if match_algorithm_returned_empty_recently, save time algorithm comes off cool down
                            algorithm_results = FinalAlgorithmResults::algorithm_cool_down_found;

                            break;
                        }
                        case successfully_extracted: {
                            //Allow the function to extract the matches that were found, but not to run. This means set
                            // algorithm_matches_empty = false AND do NOT set continue_loop = false.
                            algorithm_matches_empty = false;
                            break;
                        }
                    }
                } else {

                    //If cap response has not been set. Send an error and set it.
                    if(cap_response_message->return_status() == ReturnStatus::SUCCESS
                       && cap_response_message->success_type() == findmatches::FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED) {
                        const std::string error_string = "other_users_said_yes_empty && algorithm_matches_empty was reached. However, the"
                                                         " cap_response_message has not been set which should never be the case.";

                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "number_tries_remaining", std::to_string(number_tries_remaining),
                                "number_swipes_remaining", std::to_string(user_account_values.number_swipes_remaining),
                                "user_oid", user_account_values.user_account_oid,
                                "user_account_doc", user_account_values.user_account_doc_view,
                                "saved_matches.size()", std::to_string(user_account_values.saved_matches.size())
                        );

                        addNoMatchesFoundToResponseMessage(
                                cap_response_message,
                                matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND.count()
                        );

                        // NOTE: Ok to continue here.
                    }

                    continue_loop = false;
                }
            }
            //draw from this list if
            // 'other users said yes' list is not empty AND 'algorithm matches' list is empty
            //OR
            // 'other users said yes' list is not empty AND draw_from_list_value hit the max number
            else if (!other_users_said_yes_empty
                     && (algorithm_matches_empty
                         || user_account_values.draw_from_list_value >= matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER)
                    ) { //attempt to draw from 'other user said yes' list

                return_val = validateArrayElement(
                        user_account_values,
                        user_accounts_collection,
                        session,
                        FindMatchesArrayNameEnum::other_users_matched_list
                );

                if (return_val == ValidateArrayElementReturnEnum::empty_list) {

                    //if list is empty no longer draw from 'other users matched' list
                    other_users_said_yes_empty = true;
                    number_tries_remaining++;
                } else if (return_val == ValidateArrayElementReturnEnum::success) {
                    user_account_values.draw_from_list_value = 0;
                    user_account_values.number_swipes_remaining--;
                }
                //else {} //other cases are handled below

            }
            /*//NOTE: this 'else' statement will essentially run the following check, however
            // the below logic combined with the logic of the 'else if' statement above are always true
            if (!algorithm_matches_empty
                && (other_users_said_yes_empty
                    || user_account_values.draw_from_list_value < NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER)
            )*/
            else { //attempt to draw from 'algorithm matches' list

                return_val = validateArrayElement(
                        user_account_values,
                        user_accounts_collection,
                        session,
                        FindMatchesArrayNameEnum::algorithm_matched_list
                );

                if (return_val == ValidateArrayElementReturnEnum::empty_list) {

                    if (algorithm_results != FinalAlgorithmResults::algorithm_did_not_run) {
                        //This will allow it to check the 'other user said yes' list before exiting
                        // while only running the algorithm once per call.
                        algorithm_matches_empty = true;

                        addNoMatchesFoundToResponseMessage(
                                cap_response_message,
                                matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND.count()
                        );
                    }
                    else {

                        algorithm_results = algorithm_successful;

                        //run algorithm
                        const AlgorithmReturnValues algorithm_return_values = beginAlgorithmCheckCoolDown(
                            user_account_values,
                            session,
                            user_accounts_collection
                        );

                        switch (algorithm_return_values.returnMessage) {
                            case error: { //if algorithm failed
                                setLgErrorToResponseMessage(cap_response_message);
                                continue_loop = false;
                                break;
                            }
                            case no_matches_found: {
                                addNoMatchesFoundToResponseMessage(
                                        cap_response_message,
                                        algorithm_return_values.coolDownOnMatchAlgorithm.count()
                                );

                                //no matches were found by the algorithm
                                //this will allow it to check the 'other user said yes' list before exiting
                                //An important note is that if this point is reached where algorithm_matches_empty == true
                                // && algorithm_results == set_no_matches_found then the algorithm will attempt to run
                                // again in a lot of cases.
                                algorithm_matches_empty = true;
                                algorithm_results = FinalAlgorithmResults::set_no_matches_found;
                                break;
                            }
                            case match_algorithm_returned_empty_recently:
                            case match_algorithm_ran_recently: {
                                addCooldownToResponseMessage(
                                        cap_response_message,
                                        algorithm_return_values.coolDownOnMatchAlgorithm.count()
                                );

                                //this can mean
                                // the match ran recently OR
                                // the match algorithm ran recently, returned an empty match and had no changes to the search criteria by the user

                                //this will allow it to check the 'other user said yes' list before exiting
                                algorithm_matches_empty = true;

                                //if match_algorithm_returned_empty_recently, save time algorithm comes off cool down
                                algorithm_results = FinalAlgorithmResults::algorithm_cool_down_found;

                                break;
                            }
                            case successfully_extracted:
                                break;
                        }
                    }

                    number_tries_remaining++;
                }
                else if (return_val == ValidateArrayElementReturnEnum::success) {

                    //increment drawFromListValue if the extraction was successful and not at max value
                    if (user_account_values.draw_from_list_value <
                        matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER) {
                        user_account_values.draw_from_list_value++;
                    }

                    user_account_values.number_swipes_remaining--;
                }
                //else {} //other cases are handled below

            }

            if (return_val == ValidateArrayElementReturnEnum::unhandleable_error) {
                setLgErrorToResponseMessage(cap_response_message);
                continue_loop = false;
                //NOTE: Do not abort here, want the 'unlock' to run.
            }
            //No need to end the loop in other cases, the element that caused the error is removed.
//            else if (return_val == ValidateArrayElementReturnEnum::extraction_error) {} //continue with the loop
//            else if (return_val == ValidateArrayElementReturnEnum::no_longer_valid) {} //continue with the loop
//            else {} //other cases are handled above

            number_tries_remaining--;
        }

        //If the number of requested matches was not reached AND the cap message was not set.
        if(user_account_values.saved_matches.size() < number_responses_to_send
            && cap_response_message->return_status() == ReturnStatus::SUCCESS
            && cap_response_message->success_type() == findmatches::FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED
        ) {
            if(user_account_values.number_swipes_remaining <= 0) { //if swipes hitting zero cancelled the loop
                cap_response_message->set_return_status(ReturnStatus::SUCCESS);
                cap_response_message->set_success_type(findmatches::FindMatchesCapMessage_SuccessTypes_NO_SWIPES_REMAINING);
            }
            else { //if loop was canceled in some other way

                //The only ways the loop can be canceled, and it gets to this point are when continue_loop is false
                // or number_tries_remaining hit zero. When continue_loop is false the cap message should already be
                // set, and it should never reach this point. When number_tries_remaining ran out, it is valid for
                // it to hit this point. However, if number_tries_remaining still has attempts remaining, it should
                // not hit this point.
                if(number_tries_remaining > 0 || !continue_loop) {
                    const std::string error_string = "other_users_said_yes_empty && algorithm_matches_empty was reached. However, the"
                                                     " cap_response_message has not been set which should never be the case.";

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "number_tries_remaining", std::to_string(number_tries_remaining),
                            "number_swipes_remaining", std::to_string(user_account_values.number_swipes_remaining),
                            "user_oid", user_account_values.user_account_oid,
                            "user_account_doc", user_account_values.user_account_doc_view,
                            "continue_loop", continue_loop?"true":"false"
                    );
                }

                addCooldownToResponseMessage(
                        cap_response_message,
                        matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND.count()
                );
            }
        }

        // save any relevant info to document
        bsoncxx::builder::basic::array algorithms_matched_list;
        bsoncxx::builder::basic::array other_users_matched_list;
        bsoncxx::builder::basic::array concat_with_has_been_extracted_list;

        for (const bsoncxx::document::view_or_value& ele : user_account_values.algorithm_matched_account_list) {
            algorithms_matched_list.append(
                    ele.view()
            );
        }

        for (const bsoncxx::document::view& ele : user_account_values.other_users_swiped_yes_list) {
            other_users_matched_list.append(
                    ele
            );
        }

        for (const MatchInfoStruct& ele : user_account_values.saved_matches) {
            concat_with_has_been_extracted_list.append(
                    ele.matching_element_doc
            );
        }

        bsoncxx::builder::stream::document update_document;
        update_document

                //set 'account list to draw from' integer to updated value
                //set 'swipes remaining' integer to updated value
                //set 'algorithm matched' list to updated value
                //set 'other users matched' list to updated value
                //'unlock' the find_matches algorithm
                << "$set" << open_document
                    << user_account_keys::INT_FOR_MATCH_LIST_TO_DRAW_FROM << bsoncxx::types::b_int32{user_account_values.draw_from_list_value}
                    << user_account_keys::NUMBER_SWIPES_REMAINING << bsoncxx::types::b_int32{user_account_values.number_swipes_remaining}
                    << user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST << algorithms_matched_list
                    << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << other_users_matched_list
                << close_document

                //concatenate matches to 'has been extracted list'
                << "$push" << open_document
                    << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST << open_document
                        << "$each" << concat_with_has_been_extracted_list
                    << close_document
                << close_document

                //increment the 'total numbers of matches drawn' integer
                << "$inc" << open_document
                    << user_account_keys::TOTAL_NUMBER_MATCHES_DRAWN << (int) user_account_values.saved_matches.size()
                << close_document;

        addFunctionUnlockDocument(update_document);

        switch (algorithm_results) {
            //algorithm did not run
            case algorithm_did_not_run:
            //If algorithm was on cool down, do NOT save anything, it did not run and do not want to overwrite
            // the empty match value.
            case algorithm_cool_down_found:
                break;
            case algorithm_successful: //algorithm successfully ran
                update_document
                    << "$set" << open_document
                        << user_account_keys::LAST_TIME_MATCH_ALGORITHM_RAN << bsoncxx::types::b_date{user_account_values.current_timestamp}
                        << user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
                    << close_document;
                break;
            case set_no_matches_found: //algorithm ran and returned noMatchesFound
                update_document
                    << "$set" << open_document
                        << user_account_keys::LAST_TIME_MATCH_ALGORITHM_RAN << bsoncxx::types::b_date{user_account_values.current_timestamp}
                        << user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED << bsoncxx::types::b_date{user_account_values.current_timestamp}
                    << close_document;
                break;
        }

        try {
            user_accounts_collection.update_one(
                    *session,
                    document{}
                            << "_id" << user_account_values.user_account_oid
                    << finalize,
                    update_document.view()
            );
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), e.what(),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "match_account_doc", user_account_values.user_account_doc_view,
                    "match_OID_used", user_account_values.user_account_oid,
                    "timestamp", getDateTimeStringFromTimestamp(user_account_values.current_timestamp)
            );

            //clear saved matches so that it will not send any back
            setLgErrorToResponseMessage(cap_response_message);
            user_account_values.saved_matches.clear();
            //Can abort here, nothing worked either way.
            session->abort_transaction();
            return;
        }

    }; //end of mongoDB transaction

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {

        mongocxx::client_session session = mongo_cpp_client.start_session();

        //these are transaction options
        mongocxx::options::transaction trans_opts{};
        //NOTE: The deadline time on the client for findMatches() is gRPC_Find_Matches_Deadline_Time
        // (40 seconds at the time of writing this).
        //NOTE: 60 seconds is also the default max commit time.
        trans_opts.max_commit_time_ms(matching_algorithm::TIME_TO_LOCK_FIND_MATCHES_TO_PREVENT_DOUBLE_RUNS);

        clock_gettime(CLOCK_REALTIME, &transaction_start_ts);
        session.with_transaction(transactionCallback, trans_opts);
    }
    catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
        std::cout << "Exception calling findMatchesImplementation() transaction.\n" << e.what() << '\n';
#endif
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_account_OID", user_account_oid_str
        );

        //clear saved matches so that it will not send any back
        setLgErrorToResponseMessage(cap_response_message);
        user_account_values.saved_matches.clear();

        //continue here to send error to client
    }
    catch (const mongocxx::operation_exception& e) {

        if(e.code().value() == 50) { //operation exceeded time limit (this means the max_time was exceeded)
            const std::string error_string = "Operation time limit was exceeded (most likely) on algorithm run.";
            const std::optional<std::string> exception_string = e.what();
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    exception_string,error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "user_oid", user_account_values.user_account_oid,
                    "user_account_doc", user_account_values.user_account_doc_view,
                    "errorResponseMessage", cap_response_message->DebugString()
            );

            user_account_values.saved_matches.clear();

            //Returning a cool down here so the user's device doesn't spam the system.
            addCooldownToResponseMessage(
                    cap_response_message,
                    matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND.count()
            );
        }
        else {
            if (e.raw_server_error()) { //raw_server_error exists
                throw mongocxx::operation_exception(e.code(),
                                                    bsoncxx::document::value(e.raw_server_error().value()),
                                                    e.what());
            } else { //raw_server_error does not exist
                throw mongocxx::operation_exception(e.code(),
                                                    document{} << finalize,
                                                    e.what());
            }
        }
    }

#ifndef _RELEASE
    clock_gettime(CLOCK_REALTIME, &transaction_stop_ts);

    std::cout << "Total Transaction CLOCK_REALTIME: " << durationAsNanoSeconds(transaction_stop_ts, transaction_start_ts) << '\n';
#endif

    //Milliseconds before the swipes are reset.
    const std::chrono::milliseconds swipes_time_before_reset =
            matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED -
             std::chrono::milliseconds{user_account_values.current_timestamp} % matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED;

    std::vector<bsoncxx::document::value> statistics_documents_to_insert;
    bsoncxx::builder::basic::array statistics_document_oids_to_update;

    size_t num_written_back = 0;
    for (const auto& match : user_account_values.saved_matches) {
        findmatches::FindMatchesResponse match_response_message;

        bool result = extractMatchAccountInfoForWriteToClient(
                mongo_cpp_client,
                accounts_db,
                user_accounts_collection,
                match,
                swipes_time_before_reset,
                user_account_values.current_timestamp,
                match_response_message.mutable_single_match(),
                statistics_documents_to_insert,
                statistics_document_oids_to_update
        );

        if (result) { //if extracted info successfully
            response->Write(match_response_message);
            num_written_back++;
        }

        //if an error occurred here, it was already stored
    }

    if(num_written_back < number_responses_to_send
        && cap_response_message->return_status() == ReturnStatus::SUCCESS
        && cap_response_message->success_type() == findmatches::FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED
    ) {
        const std::string error_string = "Not enough saved matches were returned AND the response_message_when_not_enough_matches "
                                         "was not set. It should always return something to the user.";

        storeMongoDBErrorAndException(
              __LINE__, __FILE__,
              std::optional<std::string>(), error_string,
              "number_responses_to_send", std::to_string(number_responses_to_send),
              "num_written_back", std::to_string(num_written_back),
              "saved_matches.size()", std::to_string(user_account_values.saved_matches.size()),
              "user_oid", user_account_values.user_account_oid,
              "user_account_doc", user_account_values.user_account_doc_view
        );

        //Returning a cool down here so the user's device doesn't spam the system.
        addCooldownToResponseMessage(
            cap_response_message,
            matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND.count()
        );
    }

    cap_response_message->set_swipes_time_before_reset(swipes_time_before_reset.count());

    //Write final message to client, this message is always expected to go back.
    response->Write(find_matches_response_for_cap_message);

    mongocxx::database stats_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];

    //save statistics
    saveFindMatchesStatistics(
            user_account_values,
            mongo_cpp_client,
            stats_db,
            accounts_db,
            statistics_documents_to_insert,
            statistics_document_oids_to_update
    );

#ifndef _RELEASE
    clock_gettime(CLOCK_REALTIME, &function_stop_ts);

    std::cout << "'Total Function' CLOCK_REALTIME: " << durationAsNanoSeconds(function_stop_ts, function_start_ts) << '\n';
#endif
}


