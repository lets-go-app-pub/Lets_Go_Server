//
// Created by jeremiah on 5/30/22.
//

#include <database_names.h>
#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <collection_names.h>
#include <bsoncxx/builder/stream/document.hpp>
#include "send_messages_implementation.h"
#include "thread_pool_global_variable.h"
#include "chat_room_commands_helper_functions.h"

bool clearDatabaseAndGlobalsForTesting() {

#ifndef LG_TESTING
    std::cout << "LG_TESTING not enabled\n";
    return false;
#endif

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
    mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
    mongocxx::database feedback_db = mongo_cpp_client[database_names::FEEDBACK_DATABASE_NAME];
    mongocxx::database reports_db = mongo_cpp_client[database_names::REPORTS_DATABASE_NAME];
    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    std::cout << "Databases Finished\n";

    //mongoDB (ACCOUNTS_DATABASE_NAME) collections
    mongocxx::collection admin_accounts_collection = accounts_db[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_account_statistics_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];
    mongocxx::collection pending_account_collection = accounts_db[collection_names::PENDING_ACCOUNT_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];
    mongocxx::collection info_stored_after_deletion_collection = accounts_db[collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME];
    mongocxx::collection email_verification_collection = accounts_db[collection_names::EMAIL_VERIFICATION_COLLECTION_NAME];
    mongocxx::collection account_recovery_collection = accounts_db[collection_names::ACCOUNT_RECOVERY_COLLECTION_NAME];

    mongocxx::collection fresh_errors_collection = errors_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];
    mongocxx::collection handled_errors_collection = errors_db[collection_names::HANDLED_ERRORS_COLLECTION_NAME];
    mongocxx::collection handled_errors_list_collection = errors_db[collection_names::HANDLED_ERRORS_LIST_COLLECTION_NAME];

    mongocxx::collection deleted_deleted_accounts_collection = deleted_db[collection_names::DELETED_DELETED_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection deleted_user_pictures_collection = deleted_db[collection_names::DELETED_USER_PICTURES_COLLECTION_NAME];
    mongocxx::collection deleted_chat_message_pictures_collection = deleted_db[collection_names::DELETED_CHAT_MESSAGE_PICTURES_COLLECTION_NAME];

    mongocxx::collection match_algorithm_statistics_collection = info_for_statistics_db[collection_names::MATCH_ALGORITHM_STATISTICS_COLLECTION_NAME];
    mongocxx::collection match_algorithm_results_collection = info_for_statistics_db[collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME];
    mongocxx::collection individual_match_statistics_collection = info_for_statistics_db[collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME];
    mongocxx::collection user_account_statistics_documents_completed_collection = info_for_statistics_db[collection_names::USER_ACCOUNT_STATISTICS_DOCUMENTS_COMPLETED_COLLECTION_NAME];

    mongocxx::collection activity_suggestion_feedback_collection = feedback_db[collection_names::ACTIVITY_SUGGESTION_FEEDBACK_COLLECTION_NAME];
    mongocxx::collection bug_report_feedback_collection = feedback_db[collection_names::BUG_REPORT_FEEDBACK_COLLECTION_NAME];
    mongocxx::collection other_suggestion_feedback_collection = feedback_db[collection_names::OTHER_SUGGESTION_FEEDBACK_COLLECTION_NAME];

    mongocxx::collection outstanding_reports_collection = reports_db[collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME];
    mongocxx::collection handled_reports_collection = reports_db[collection_names::HANDLED_REPORTS_COLLECTION_NAME];

    //NOTE: Used delete_many() instead of drop() to avoid losing collection indexes.

    std::cout << "Collections Finished\n";

    //Drop databases, this has no indexes for the individual collections (the ones that are specific to the chat rooms).
    //It will also disconnect the change stream if CHAT_ROOMS_DATABASE_NAME is directly dropped.
    std::vector<std::string> collection_names = accounts_db.list_collection_names();

    for(const auto& collection_name : collection_names) {
        //don't delete icons or activities (they are needed for account generation)
        if(collection_name != collection_names::ACTIVITIES_INFO_COLLECTION_NAME
            && collection_name != collection_names::ICONS_INFO_COLLECTION_NAME
        ) {
            accounts_db[collection_name].delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
        }
    }

    std::cout << "accounts_db Finished\n";

    fresh_errors_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    handled_errors_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    handled_errors_list_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);

    deleted_deleted_accounts_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    deleted_user_pictures_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    deleted_chat_message_pictures_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);

    match_algorithm_statistics_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    match_algorithm_results_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    individual_match_statistics_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    user_account_statistics_documents_completed_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);

    activity_suggestion_feedback_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    bug_report_feedback_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    other_suggestion_feedback_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);

    outstanding_reports_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    handled_reports_collection.delete_many(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);

    std::cout << "delete_many Finished\n";

    //Drop databases, this has no indexes for the individual collections (the ones that are specific to the chat rooms).
    //It will also disconnect the change stream if CHAT_ROOMS_DATABASE_NAME is directly dropped.
    collection_names = chat_rooms_db.list_collection_names();

    for(const auto& collection_name : collection_names) {
        //do not drop the info collection
        if(collection_name != collection_names::CHAT_ROOM_INFO) {
            chat_rooms_db[collection_name].drop();
        }
    }

    map_of_chat_rooms_to_users.clear();

#ifdef LG_TESTING
    user_open_chat_streams.clearAll();
    testing_delay_for_messages = std::chrono::milliseconds{-1};
#endif

    std::cout << "Returning\n";

    return true;
}