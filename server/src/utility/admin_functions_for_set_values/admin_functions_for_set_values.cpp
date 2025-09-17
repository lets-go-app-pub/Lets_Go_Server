//
// Created by jeremiah on 9/12/21.
//

#include "admin_functions_for_set_values.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"

#include <connection_pool_global_variable.h>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <store_mongoDB_error_and_exception.h>
#include <admin_privileges_vector.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void updateUserAccountWithDocument(
        const std::string& userAccountOIDStr,
        const bsoncxx::document::view& mergeDocument,
        const std::function<void()>& success_func,
        const std::function<void(const std::string& /* error_str */)>& error_func,
        const std::function<void(mongocxx::client&, mongocxx::database&)>& save_statistics
) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::stdx::optional<mongocxx::result::update> update_info;
    try {

        mongocxx::pipeline pipeline;

        pipeline.add_fields(mergeDocument);

        //find user account document
        update_info = user_accounts_collection.update_one(
                document{}
                        << "_id" << bsoncxx::oid{userAccountOIDStr}
                << finalize,
                pipeline
        );

    } catch (const mongocxx::logic_error& e) {
        std::optional<std::string> dummy_string;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      dummy_string, std::string(e.what()),
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                      "ObjectID_used", userAccountOIDStr,
                                      "Document_passed", mergeDocument
        );

        error_func("Error exception thrown when setting user info.");
        return;
    }

    if (!update_info) {
        error_func("Failed to update user info.\n'!update_info' returned.");
    } else if (update_info->modified_count() < 1) {
        error_func("Failed to update user info.\nupdate_info->modified_count(): " +
                   std::to_string(update_info->modified_count()));
    } else { //success
        if(save_statistics) {
            save_statistics(mongoCppClient, accountsDB);
        }

        success_func();
    }
}

bool checkForUpdateUserPrivilege(std::basic_string<char>* error_str,
                                 const bsoncxx::stdx::optional<bsoncxx::document::value>& admin_info_doc_value) {
    bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;

    auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
    if (admin_privilege_element &&
        admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__,
                        admin_privilege_element,
                        admin_info_doc_view, bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME);

        error_str->append("Error stored on server.");
        return false;
    }

    if (!admin_privileges[admin_level].update_single_users()) {
        error_str->append("Admin level " + AdminLevelEnum_Name(admin_level) +
                          " does not have 'update_single_users' access.");
        return false;
    }

    return true;
}