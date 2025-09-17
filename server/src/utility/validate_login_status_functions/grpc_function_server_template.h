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
#include "get_login_document.h"

//Calls a login using the provided information and merges mergeDocument into the user account if successful.
//Will call setSuccess on successful login, and setReturnStatus on failed login.
//If projectionDocument is left nullptr it will be ignored.
//If save_statistics() is set to anything besides nullptr it will be run if successful.
//setSuccess has a parameter for a bsoncxx::document::view. This is a pointer to an object that expires
// after then function ends. It can !!ONLY BE OPERATED ON INSIDE THE LAMBDA!!. If a reference to the view is saved
// it will be invalid when the function completes.
template <bool allowed_to_run_with_not_enough_info>
void grpcValidateLoginFunctionTemplate(
        const std::string& user_account_oid_str,
        const std::string& login_token_str,
        const std::string& installation_id,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::document::view& merge_document,
        const std::function<void(const ReturnStatus&)>& set_return_status,
        const std::function<void(const bsoncxx::document::view& /*user_account_doc*/)>& set_success,
        std::shared_ptr<document> projection_document = nullptr,
        const std::function<void(mongocxx::client& /*mongoCppClient*/, mongocxx::database& /*accountsDB*/)>& save_statistics = nullptr
) {

    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;
    using bsoncxx::builder::stream::open_array;
    using bsoncxx::builder::stream::open_document;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::pipeline update_pipeline;

    const bsoncxx::document::value login_document = getLoginDocument<allowed_to_run_with_not_enough_info>(
            login_token_str,
            installation_id,
            merge_document,
            current_timestamp
    );

    if(projection_document == nullptr) {
        projection_document = std::make_shared<document>();
    }

    //specifying id to be extracted so error messages will show it even though it is implicit
    //NOTE: Do not finalize this document, it will not be properly stored and no projection will occur.
    (*projection_document)
            << "_id" << 1
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1;

    update_pipeline.replace_root(login_document.view());

    mongocxx::stdx::optional<bsoncxx::document::value> find_and_update_user_account;

    if (!runInitialLoginOperation(
            find_and_update_user_account,
            user_accounts_collection,
            bsoncxx::oid{user_account_oid_str},
            login_document,
            *projection_document)
    ) {
        set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    const ReturnStatus return_status = checkForValidLoginToken(
            find_and_update_user_account,
            user_account_oid_str
    );

    if (return_status == ReturnStatus::SUCCESS) {
        const bsoncxx::document::view user_account_doc_view = *find_and_update_user_account;

        if(save_statistics) {
            save_statistics(mongo_cpp_client, accounts_db);
        }

        set_success(user_account_doc_view);
    } else {
        set_return_status(return_status);
    }

}