//
// Created by jeremiah on 9/15/22.
//

#pragma once

#include <chrono>

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/pipeline.hpp>

#include "user_account_keys.h"
#include "UserSubscriptionStatus.grpc.pb.h"

#include <optional>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include <StatusEnum.grpc.pb.h>
#include <UserAccountStatusEnum.grpc.pb.h>
#include <utility_general_functions.h>

//generates document for logic using aggregation pipeline
// document expected to be placed inside a replace_root type of aggregation pipeline
// will save a value to LOGGED_IN_RETURN_MESSAGE based on ReturnStatus
// which is expected to be projected out and checked; if appendToAndStatementDoc is
// set and a value returns false then LOGGED_IN_RETURN_MESSAGE will be set to ReturnStatus::UNKNOWN
template <bool allowed_to_run_with_not_enough_info, bool requires_subscription = false>
bsoncxx::document::value getLoginDocument(
        const std::string& login_token_str,
        const std::string& installation_id,
        const bsoncxx::document::view& merge_document,
        const std::chrono::milliseconds& current_timestamp,
        const std::shared_ptr<std::vector<bsoncxx::document::value>>& append_to_and_statement_doc = nullptr
        ) {

    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;
    using bsoncxx::builder::stream::open_array;
    using bsoncxx::builder::stream::open_document;

    std::vector<std::pair<std::string,bsoncxx::document::value&>> appended_to_variable_names;
    if(append_to_and_statement_doc != nullptr) {
        for(size_t i = 0; i < (*append_to_and_statement_doc).size(); i++) {

            std::string variable_name = "append_";

            int digits = (int)i;

            while(digits > 0) {
                variable_name += (char)('a' + (digits % 26));
                digits /= 26;
            }

            appended_to_variable_names.emplace_back(
                    variable_name,
                    (*append_to_and_statement_doc)[i]
            );
        }
    }

    const bsoncxx::types::b_date current_date_mongo = bsoncxx::types::b_date{current_timestamp};

    auto aggregation_var_reference = [](const std::string& var)->std::string {
        return "$$" + var;
    };

    auto calculate_not_doc = [&aggregation_var_reference](const std::string& var)->bsoncxx::document::value {
        return document{}
                << "$not" << open_array
                    << aggregation_var_reference(var)
                << close_array
            << finalize;
    };

    std::optional<bsoncxx::document::value> account_status_check;
    bsoncxx::builder::stream::document boolean_variables;

    static const std::string ACCOUNT_ACCESSIBLE = "aiA";

    if(allowed_to_run_with_not_enough_info) { //the calling function is allowed to run with STATUS_REQUIRES_MORE_INFO set
        boolean_variables
            << ACCOUNT_ACCESSIBLE << open_document
                << "$or" << open_array
                    << open_document
                        << "$eq" << open_array
                            << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_ACTIVE
                        << close_array
                    << close_document
                    << open_document
                        << "$eq" << open_array
                            << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_REQUIRES_MORE_INFO
                        << close_array
                    << close_document
                << close_array
            << close_document;
    } else { //the calling function is allowed to run with STATUS_REQUIRES_MORE_INFO set
        boolean_variables
            << ACCOUNT_ACCESSIBLE << open_document
                << "$eq" << open_array
                    << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_ACTIVE
                << close_array
            << close_document;
    }

    static const std::string LOGIN_TOKEN_MATCHES = "ltM";

    //login token matches
    boolean_variables
        << LOGIN_TOKEN_MATCHES << open_document
            << "$eq" << open_array
                << "$" + user_account_keys::LOGGED_IN_TOKEN << login_token_str
            << close_array
        << close_document;

    static const std::string LOGIN_TOKEN_TIME_OK = "lltO";

    //login token has not expired
    boolean_variables
        << LOGIN_TOKEN_TIME_OK << open_document
            << "$lt" << open_array
                << current_date_mongo << "$" + user_account_keys::LOGGED_IN_TOKEN_EXPIRATION
            << close_array
        << close_document;

    static const std::string INSTALL_ID_MATCHES = "iiM";

    //install id matches
    boolean_variables
            << INSTALL_ID_MATCHES << open_document
                << "$eq" << open_array
                    << "$" + user_account_keys::LOGGED_IN_INSTALLATION_ID << installation_id
                << close_array
            << close_document;

    static const std::string SUBSCRIPTION_REQUIRED = "ssR";

    if(requires_subscription) {
        //subscription required
        boolean_variables
                << SUBSCRIPTION_REQUIRED << open_document
                    << "$gt" << open_array
                        << "$" + user_account_keys::SUBSCRIPTION_STATUS << UserSubscriptionStatus::NO_SUBSCRIPTION
                    << close_array
                << close_document;
    }

    bsoncxx::builder::basic::array cases_builder;

    bsoncxx::builder::stream::array success_condition_builder{};

    success_condition_builder
            << aggregation_var_reference(ACCOUNT_ACCESSIBLE)
            << aggregation_var_reference(LOGIN_TOKEN_MATCHES)
            << aggregation_var_reference(LOGIN_TOKEN_TIME_OK)
            << aggregation_var_reference(INSTALL_ID_MATCHES);

    if(requires_subscription) {
        success_condition_builder
            << aggregation_var_reference(SUBSCRIPTION_REQUIRED);
    }

    for(const auto& [name, doc] : appended_to_variable_names) {
        success_condition_builder
            << aggregation_var_reference(name);
    }

    //user was properly logged in, and matched other conditions as well
    cases_builder.append(
        document{}
            << "case" << open_document
                << "$and" << success_condition_builder.view()
            << close_document
            << "then" << open_document

                << "$mergeObjects" << open_array
                    << merge_document
                    << open_document
                        << user_account_keys::LOGGED_IN_RETURN_MESSAGE << ReturnStatus::SUCCESS
                    << close_document
                << close_array

            << close_document
        << finalize
    );

    auto append_failure_case = [&cases_builder](
            const bsoncxx::document::view& caseDoc, const ReturnStatus& value) {
        cases_builder.append(
            document{}
                << "case" << caseDoc
                << "then" << open_document
                    << user_account_keys::LOGGED_IN_RETURN_MESSAGE << value
                << close_document
            << finalize
        );
    };

    /***********************************************/
    /** NOTE: Order of switch cases matters here! **/
    /***********************************************/

    //account banned
    append_failure_case(
        document{}
            << "$eq" << open_array
                << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_BANNED
            << close_array
        << finalize,
        ReturnStatus::ACCOUNT_BANNED
    );

    //account suspended
    append_failure_case(
        document{}
            << "$eq" << open_array
                << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_SUSPENDED
            << close_array
        << finalize,
        ReturnStatus::ACCOUNT_SUSPENDED
    );

    //installation ids do not match
    append_failure_case(
        calculate_not_doc(INSTALL_ID_MATCHES),
        ReturnStatus::LOGGED_IN_ELSEWHERE
    );

    if(requires_subscription) {
        //subscription required
        append_failure_case(
                document{}
                    << "$eq" << open_array
                        << "$" + user_account_keys::SUBSCRIPTION_STATUS << UserSubscriptionStatus::NO_SUBSCRIPTION
                    << close_array
                << finalize,
                ReturnStatus::SUBSCRIPTION_REQUIRED
        );
    }

    //expired token
    append_failure_case(
        document{}
            << "$and" << open_array
                << "$$" + LOGIN_TOKEN_MATCHES
                << calculate_not_doc(LOGIN_TOKEN_TIME_OK)
            << close_array
        << finalize,
        ReturnStatus::LOGIN_TOKEN_EXPIRED
    );

    if(!allowed_to_run_with_not_enough_info) {
        //not enough info collected
        append_failure_case(
                document{}
                    << "$eq" << open_array
                        << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_REQUIRES_MORE_INFO
                    << close_array
                << finalize,
                ReturnStatus::NOT_ENOUGH_INFO
        );
    }

    //invalid login token passed
    append_failure_case(
        calculate_not_doc(LOGIN_TOKEN_MATCHES),
        ReturnStatus::LOGIN_TOKEN_DID_NOT_MATCH
    );

    //append any additional conditions
    for(const auto& [name, doc] : appended_to_variable_names) {
        boolean_variables
            << name << doc;

        append_failure_case(
            calculate_not_doc(name),
            ReturnStatus::UNKNOWN
        );
    }

    return document{}
        << "newRoot" << open_document
            << "$let" << open_document
                << "vars" << open_document

                    //generate document to merge below; $$mergeDoc is expected to be a document type
                    // with a field for LOGGED_IN_RETURN_MESSAGE
                    << "mergeDoc" << open_document

                        << "$let" << open_document
                            << "vars" << boolean_variables.view()
                            << "in" << open_document
                                << "$switch" << open_document
                                    << "branches" << cases_builder

                                    //NOTE: reaching here means that there was an error with logic
                                    << "default" << open_document
                                        << user_account_keys::LOGGED_IN_RETURN_MESSAGE << ReturnStatus::LG_ERROR
                                    << close_document

                                << close_document
                            << close_document
                        << close_document

                    << close_document

                << close_document
                << "in" << open_document

                    << "$mergeObjects" << open_array
                        << "$$ROOT"
                        << "$$mergeDoc"
                    << close_array

                << close_document
            << close_document
        << close_document
    << finalize;

}