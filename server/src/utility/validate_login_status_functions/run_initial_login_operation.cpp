//
// Created by jeremiah on 9/14/22.
//

#include <optional>

#include <mongocxx/exception/logic_error.hpp>

#include "run_initial_login_operation.h"
#include "database_names.h"
#include "collection_names.h"
#include "store_mongoDB_error_and_exception.h"
#include "session_to_run_functions.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool runInitialLoginOperation(
        bsoncxx::stdx::optional<bsoncxx::document::value>& findAndUpdateUserAccount,
        mongocxx::collection& userAccountsCollection,
        const bsoncxx::oid& userAccountOID,
        const bsoncxx::document::view& loginDocument,
        const bsoncxx::document::view& projectionDocument,
        mongocxx::client_session* session
) {

    mongocxx::pipeline updatePipeline;
    updatePipeline.replace_root(loginDocument);

    std::optional<std::string> exceptionString;

    mongocxx::options::find_one_and_update opts;
    opts.projection(projectionDocument);

    opts.return_document(mongocxx::options::return_document::k_after);

    try {

        //find and update user account document
        findAndUpdateUserAccount = find_one_and_update_optional_session(
                session,
                userAccountsCollection,
                document{}
                        << "_id" << userAccountOID
                << finalize,
                updatePipeline,
                opts
        );

    } catch (const mongocxx::logic_error& e) {

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                exceptionString, std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "ObjectID_used", userAccountOID,
                "Document_passed", updatePipeline.view_array()
        );

        return false;
    }

    return true;
}