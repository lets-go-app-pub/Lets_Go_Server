#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include <utility_general_functions.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>
#include <store_mongoDB_error_and_exception.h>

#include "login_support_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"

//proto file is LoginSupportFunctions.proto

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void deleteAccountImplementation(
        const loginsupport::LoginSupportRequest* request,
        loginsupport::LoginSupportResponse* response
);

void deleteAccount(
        const loginsupport::LoginSupportRequest* request,
        loginsupport::LoginSupportResponse* response
) {

    handleFunctionOperationException(
            [&] {
                deleteAccountImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            },
            __LINE__, __FILE__, request);
}

void deleteAccountImplementation(
        const loginsupport::LoginSupportRequest* request,
        loginsupport::LoginSupportResponse* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    {
        ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS) {
            response->set_return_status(basic_info_return_status);
            return;
        }
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    response->set_timestamp(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid user_account_oid{user_account_oid_str};

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account;

    {
        bsoncxx::document::value merge_document = document{} << finalize;

        bsoncxx::document::value projection_document = document{}
                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                << user_account_keys::STATUS << 1
                << finalize;

        bsoncxx::document::value login_document = getLoginDocument<false>(
                login_token_str,
                installation_id,
                merge_document,
                current_timestamp
        );

        if (!runInitialLoginOperation(
                find_user_account,
                user_accounts_collection,
                user_account_oid,
                login_document,
                projection_document)
        ) {
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }
    }

    const ReturnStatus return_status = checkForValidLoginToken(find_user_account, user_account_oid_str);

    if (return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(return_status);
    } else {

        mongocxx::client_session::with_transaction_cb transaction_callback = [&](
                mongocxx::client_session* callback_session) {

            //delete the passed account
            if (serverInternalDeleteAccount(
                    mongo_cpp_client,
                    accounts_db,
                    user_accounts_collection,
                    callback_session,
                    user_account_oid,
                    current_timestamp)
                    ) {
                response->set_timestamp(current_timestamp.count());
                response->set_return_status(ReturnStatus::SUCCESS);
            } else {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }

        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
        // more 'generic' errors. Can look here for more info
        // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
        try {
            session.with_transaction(transaction_callback);
        } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
            std::cout << "Exception calling deleteAccountImplementation() transaction.\n" << e.what() << '\n';
#endif
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "user_OID", user_account_oid
            );

            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

    }

}
