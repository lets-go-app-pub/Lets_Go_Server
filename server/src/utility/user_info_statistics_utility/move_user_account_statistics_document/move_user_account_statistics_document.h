//
// Created by jeremiah on 11/8/21.
//

#pragma once

#include <mongocxx/collection.hpp>

/** The function is expected to be called from inside of a transaction.
 *
 *  It will move the document for a given user from ACCOUNTS_DATABASE_NAME; USER_ACCOUNT_STATISTICS_COLLECTION_NAME to
 *  INFO_FOR_STATISTICS_DATABASE_NAME; USER_ACCOUNT_STATISTICS_DOCUMENTS_COMPLETED_COLLECTION_NAME.
 *
 *  Then if create_new_statistics_document is set to true, it will create a new document inside ACCOUNTS_DATABASE_NAME;
 *  USER_ACCOUNT_STATISTICS_COLLECTION_NAME.
 * **/
bool moveUserAccountStatisticsDocument(
        mongocxx::client& mongo_cpp_client,
        const bsoncxx::oid& user_account_oid,
        mongocxx::collection& user_accounts_statistics_collection,
        const std::chrono::milliseconds& currentTimestamp,
        bool create_new_statistics_document,
        mongocxx::client_session* session = nullptr
        );