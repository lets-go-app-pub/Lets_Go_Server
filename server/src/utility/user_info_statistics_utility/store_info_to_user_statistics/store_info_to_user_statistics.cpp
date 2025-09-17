//
// Created by jeremiah on 11/7/21.
//

#include "store_info_to_user_statistics.h"
#include "database_names.h"
#include "collection_names.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <store_mongoDB_error_and_exception.h>
#include <move_user_account_statistics_document.h>
#include <session_to_run_functions.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void storeInfoToUserStatistics(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::document::view& push_update_doc,
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::client_session* session
) {

    mongocxx::collection user_account_statistics_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];
    const bsoncxx::document::value find_doc = document{}
                << "_id" << user_account_oid
            << finalize;
    const bsoncxx::document::value update_doc = document{}
                << "$push" << push_update_doc
            << finalize;

    mongocxx::stdx::optional<mongocxx::result::update> update_one_result;
    try {

        mongocxx::options::update opts;
        opts.upsert(true);

        //update the statistics doc
        //NOTE: This should not be run inside the transaction. This is because the 'document
        // too large' exception must be caught and any exceptions from the session must be
        // allowed to propagate to the transaction. It could also cause the entire operation
        // (e.g. the entire move_extracted_element_to_other_account transaction could be
        // completely aborted).
        update_one_result = user_account_statistics_collection.update_one(
                find_doc.view(),
                update_doc.view(),
                opts
        );

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            std::optional<std::string>(), std::string(e.what()),
            "database", database_names::ACCOUNTS_DATABASE_NAME,
            "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME
        );

        return;
    } catch (const mongocxx::operation_exception& e) {
        if (e.code().value() == 17419) { //document too large after update

            bool successful = false;

            if(session == nullptr) {

                //if this is not inside a session, create a new transaction
                mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* callback_session) {
                    successful = moveUserAccountStatisticsDocument(
                            mongo_cpp_client,
                            user_account_oid,
                            user_account_statistics_collection,
                            current_timestamp,
                            true,
                            callback_session
                            );
                };

                mongocxx::client_session transaction_session = mongo_cpp_client.start_session();

                //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
                // more 'generic' errors. Can look here for more info
                // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
                try {
                    transaction_session.with_transaction(transactionCallback);
                } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
                    std::cout << "Exception calling storeInfoToUserStatistics() transaction.\n" << e.what() << '\n';
#endif

                    storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "user_OID", user_account_oid,
                        "update_doc", push_update_doc
                    );

                    successful = false;
                }
            } else {
                successful = moveUserAccountStatisticsDocument(
                        mongo_cpp_client,
                        user_account_oid,
                        user_account_statistics_collection,
                        current_timestamp,
                        true,
                        session
                );
            }

            if(!successful) { //failed to move doc to statistics
                //error has already been stored
                return;
            }

            std::optional<std::string> update_exception_string;
            try {

                //Update the statistics doc.
                //NOTE: This does need to be run inside a transaction (when available). This is because if this is not part of the
                // transaction then the document will not be moved before this update (done inside moveUserAccountStatisticsDocument()).
                // This means that the operation will throw the same exception (e.code().value() == 17419) as above.
                update_one_result = update_one_optional_session(
                        session,
                        user_account_statistics_collection,
                        find_doc.view(),
                        update_doc.view()
                        );

            } catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME
                );

                return;
            }
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

    if (!update_one_result || update_one_result->modified_count() < 1) {

        const std::string error_string = "Updating a user statistics document failed. The document should now be upserted as a fallback. "
                                   "However, user documents are created on account creation and should exist for (and after) the "
                                   "lifetime of the account.";

        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            std::optional<std::string>(), error_string,
            "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
            "ObjectID_used", user_account_oid,
            "Optional value set", update_one_result ? "true" : "false",
            "Modified count",std::to_string(update_one_result ? update_one_result->modified_count() : -1),
            "Upserted count",std::to_string(update_one_result ? update_one_result->result().upserted_count() : -1)
        );
    }

}