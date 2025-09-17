//
// Created by jeremiah on 3/31/21.
//
#pragma once

#include <string>
#include <functional>

#include <bsoncxx/document/value.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <StatusEnum.grpc.pb.h>
#include <global_bsoncxx_docs.h>

#include "store_mongoDB_error_and_exception.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "run_initial_login_operation.h"
#include "extract_data_from_bsoncxx.h"
#include "set_fields_helper_functions/set_fields_helper_functions.h"

//Meant to be used with parameters of the matching algorithm.
//Calls a login using the provided information. It will then project out the arrays OTHER_USERS_MATCHED_ACCOUNTS_LIST
// and ALGORITHM_MATCHED_ACCOUNTS_LIST and check if the accounts inside them are still valid matches. If they are
// they will be left inside the account, if not they will be changed.
//This will run as either an admin OR as a user.
//Returns false if an error occurs and runs set_return_status.
//Returns true if an error does NOT occur and runs set_success.
template <bool allowed_to_run_with_not_enough_info>
bool grpcValidateLoginFunctionWithMatchParametersTemplate(
        const std::string& user_account_oid_str,
        const std::string& login_token_str,
        const std::string& installation_id,
        const bool admin_info_used,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::document::view& match_parameters_to_query,
        const bsoncxx::document::view& fields_to_update_document,
        const std::function<void(const ReturnStatus& /*return_status*/, const std::string& /* error_message */)>& set_return_status,
        const std::function<void()>& set_success,
        const std::function<void(mongocxx::client& /*mongoCppClient*/, mongocxx::database& /*accountsDB*/)>& save_statistics
) {

    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;
    using bsoncxx::builder::stream::open_array;
    using bsoncxx::builder::stream::open_document;

    mongocxx::stdx::optional<bsoncxx::document::value> login_user_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    bsoncxx::builder::stream::document projection_document;

    {
        using namespace user_account_keys;

        //specifying id to be extracted so error messages will show it even though it is implicit
        //NOTE: Do not finalize this document, it will not be properly stored and no projection will occur.
        projection_document
                << "_id" << 1
                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                << std::string(OTHER_USERS_MATCHED_ACCOUNTS_LIST).append(".").append(accounts_list::OID) << 1
                << std::string(ALGORITHM_MATCHED_ACCOUNTS_LIST).append(".").append(accounts_list::OID) << 1;
    }

    if (!admin_info_used) { //user request

        bsoncxx::builder::stream::document merge_document;

        bsoncxx::document::value login_document = getLoginDocument<allowed_to_run_with_not_enough_info>(
                login_token_str,
                installation_id,
                merge_document,
                current_timestamp
        );

        if (!runInitialLoginOperation(
                login_user_account_doc,
                user_accounts_collection,
                bsoncxx::oid{user_account_oid_str},
                login_document,
                projection_document.view())
                ) {
            set_return_status(ReturnStatus::LG_ERROR, "");
            return false;
        }

        ReturnStatus return_status = checkForValidLoginToken(login_user_account_doc,
                                                             user_account_oid_str);

        if (return_status != ReturnStatus::SUCCESS) {
            set_return_status(return_status, "");
            return false;
        }

    }
    else { //admin request

        try {

            mongocxx::options::find opts;

            opts.projection(projection_document.view());

            login_user_account_doc = user_accounts_collection.find_one(
                    document{}
                            << "_id" << bsoncxx::oid{user_account_oid_str}
                    << finalize,
                    opts
            );

        } catch (const mongocxx::logic_error& e) {

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", user_account_oid_str,
                    "match_parameters_to_query", match_parameters_to_query,
                    "fields_to_update_document", fields_to_update_document
            );

            set_return_status(ReturnStatus::LG_ERROR, "Error exception thrown when setting user info.");
            return false;
        }

        if (!login_user_account_doc) {
            set_return_status(ReturnStatus::LG_ERROR, "Failed to find and update user info.\n'!login_user_account_doc' returned.");
            return false;
        }
    }

    mongocxx::stdx::optional<bsoncxx::document::value> final_user_account_doc;

    const bsoncxx::document::view login_user_account_view = login_user_account_doc->view();

    if(!removeInvalidElementsForUpdatedMatchingParameters(
            bsoncxx::oid{user_account_oid_str},
            login_user_account_view,
            user_accounts_collection,
            final_user_account_doc,
            match_parameters_to_query,
            fields_to_update_document)
            ) {
        //Error already stored
        set_return_status(ReturnStatus::LG_ERROR, "Error when running removeInvalidElementsForUpdatedMatchingParameters().");
        return false;
    }

    if(save_statistics) save_statistics(mongo_cpp_client, accounts_db);

    set_success();
    return true;
}