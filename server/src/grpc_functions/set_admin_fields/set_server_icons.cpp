//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <handle_function_operation_exception.h>
#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>

#include "set_admin_fields.h"

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "icons_info_keys.h"
#include "admin_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool setServerIconImplementation(
        set_admin_fields::SetServerIconRequest& request,
        std::string& error_message
);

bool setServerIcon(
        set_admin_fields::SetServerIconRequest& request,
        std::string& error_message
) {

    bool successful = true;

    handleFunctionOperationException(
            [&] {
                if (!setServerIconImplementation(request, error_message)) {
                    successful = false;
                }
            },
            [&] {
                successful = false;
            },
            [&] {
                successful = false;
            },
            __LINE__, __FILE__, &request
    );

    return successful;
}

bool setServerIconImplementation(
        set_admin_fields::SetServerIconRequest& request,
        std::string& error_message
) {

    {
        bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
        std::string user_account_oid_str;
        std::string login_token_str;
        std::string installation_id;

        auto store_error_message = [&](
                bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                const std::string& passed_error_message
        ) {
            error_message = passed_error_message;
            admin_info_doc_value = std::move(returned_admin_info_doc);
        };

        const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
                request.login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id,
                store_error_message
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS) {
            error_message = "ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message;
            return false;
        }

        const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
        AdminLevelEnum admin_level;

        auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
        if (admin_privilege_element
            && admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
        } else { //if element does not exist or is not type oid
            logElementError(__LINE__, __FILE__,
                            admin_privilege_element,
                            admin_info_doc_view, bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME);

            error_message = "Error stored on server.";
            return false;
        }

        if (!admin_privileges[admin_level].update_icons()) {
            error_message = "Admin level " + AdminLevelEnum_Name(admin_level) + " does not have 'update_icons' access.";
            return false;
        }
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection icons_info_collection = accounts_db[collection_names::ICONS_INFO_COLLECTION_NAME];

    if ((int)!request.icon_active() & (int)request.push_back()) {
        error_message = "Can not upsert a new inactive icon.";
        return false;
    }

    if ((int)request.icon_active() & (int)request.icon_in_bytes().empty()) {
        error_message = "Empty icon image received.";
        return false;
    }

    if ((long)request.icon_in_bytes().size() != request.icon_size_in_bytes()) {
        error_message = "Server received corrupt image. Please retry process.";
        return false;
    }

    //This needs to be moved, don't make it const.
    std::string* file_in_bytes = request.release_icon_in_bytes();

    if(file_in_bytes == nullptr) {
        const std::string error_string = "An icon byte array value was null inside the request.\n";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "icon_size", request.icon_size_in_bytes()
        );

        return false;
    }

    bsoncxx::builder::stream::document insert_document;
    insert_document
            << icons_info_keys::TIMESTAMP_LAST_UPDATED << bsoncxx::types::b_date{current_timestamp}
            << icons_info_keys::ICON_IN_BYTES << std::move(*file_in_bytes)
            << icons_info_keys::ICON_SIZE_IN_BYTES << bsoncxx::types::b_int64{(long)request.icon_size_in_bytes()}
            << icons_info_keys::ICON_ACTIVE << request.icon_active();

    delete file_in_bytes;

    if (!request.push_back()) {

        bsoncxx::stdx::optional<mongocxx::result::update> update_one_result;
        std::optional<std::string> update_icon_exception_string;
        try {
            update_one_result = icons_info_collection.update_one(
                document{}
                    << "_id" << bsoncxx::types::b_int64{(long)request.index_number()}
                << finalize,
                document{}
                    << "$set" << insert_document.view()
                << finalize
            );
        } catch (const mongocxx::logic_error& e) {
            update_icon_exception_string = e.what();
        }

        if (!update_one_result) {
            const std::string error_string = "Update icon was passed however update failed.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    update_icon_exception_string, error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "icon_image_size", std::to_string(request.icon_size_in_bytes())
            );

            error_message = "Error stored on server.";
            return false;
        } else if(update_one_result->matched_count() == 0) {
            error_message = "Icon index did not exist on server to be Updated.";
            return false;
        }

    } else { //upsert the element

        bool transaction_successful = true;
        mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

            mongocxx::pipeline find_largest_index_pipe;

            find_largest_index_pipe.sort(
                document{}
                    << "_id" << -1
                << finalize
            );

            find_largest_index_pipe.limit(1);

            mongocxx::stdx::optional<mongocxx::cursor> find_results;
            try {
                //get current largest _id (icon index) value
                find_results = icons_info_collection.aggregate(*callback_session, find_largest_index_pipe);
            } catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::ICONS_INFO_COLLECTION_NAME,
                        "icon_image_size", std::to_string(request.icon_size_in_bytes()),
                        "pipeline", find_largest_index_pipe.view_array(),
                        "insert_document", insert_document.view()
                );

                transaction_successful = false;
                return;
            }

            //calculate next _id value
            long long next_id = 0;
            if (find_results->begin() != find_results->end()) { //if cursor is NOT empty
                //should only be 1 element here
                for (const auto& doc : *find_results) {
                    next_id = doc["_id"].get_int64().value;
                }

                next_id++;
            }

            insert_document
                    << "_id" << bsoncxx::types::b_int64{next_id};

            try {
                icons_info_collection.insert_one(*callback_session, insert_document.view());
            } catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::ICONS_INFO_COLLECTION_NAME,
                        "icon_image_size", std::to_string(request.icon_size_in_bytes()),
                        "pipeline", find_largest_index_pipe.view_array(),
                        "insert_document", insert_document.view()
                );

                transaction_successful = false;
                return;
            }

            transaction_successful = true;
        };

        mongocxx::options::client_session session_opts{};
        mongocxx::options::transaction trans_opts{};
        trans_opts.max_commit_time_ms(std::chrono::milliseconds(5000));
        session_opts.default_transaction_opts(trans_opts);

        auto session = mongo_cpp_client.start_session(session_opts);

        try {
            session.with_transaction(transaction_callback);
        } catch (const mongocxx::logic_error& e) {
            //Finished
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::ICONS_INFO_COLLECTION_NAME,
                    "icon_image_size", std::to_string(request.icon_size_in_bytes())
            );

            transaction_successful = false;
        }

        return transaction_successful;
    }

    return true;
}
