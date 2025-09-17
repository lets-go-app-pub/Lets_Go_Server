//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <handle_function_operation_exception.h>

#include "set_fields_functions.h"

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_pictures_keys.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setPictureImplementation(
        setfields::SetPictureRequest* request,
        setfields::SetFieldResponse* response
);

bool insertPictureDocument(
        const std::chrono::milliseconds& timestamp,
        mongocxx::collection& pictures_collection,
        mongocxx::collection& user_collection,
        mongocxx::client_session* session,
        const bsoncxx::oid& user_account_oid,
        setfields::SetPictureRequest* request,
        const bsoncxx::document::view& user_account_view,
        int index_of_picture
);

void setPicture(
        setfields::SetPictureRequest* request,
        setfields::SetFieldResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setPictureImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            },
            __LINE__, __FILE__, request
    );
}

void setPictureImplementation(
        setfields::SetPictureRequest* request,
        setfields::SetFieldResponse* response
) {
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
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

    const int index_of_array = request->picture_array_index();
    if (index_of_array < 0 || general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT <= index_of_array) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    const int thumbnail_size = request->thumbnail_size();
    if (thumbnail_size <= 0 || server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES < thumbnail_size) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    if ((int)request->thumbnail_in_bytes().length() != thumbnail_size) { //thumbnail is corrupt
        response->set_return_status(ReturnStatus::CORRUPTED_FILE);
        return;
    }

    const int file_size = request->file_size();
    if (file_size <= 0 || server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES < file_size) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    if ((int)request->file_in_bytes().length() != file_size) { //picture is corrupt
        response->set_return_status(ReturnStatus::CORRUPTED_FILE);
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    const bsoncxx::oid user_account_oid = bsoncxx::oid{user_account_oid_str};

    const bsoncxx::document::value merge_document = document{} << finalize;

    const bsoncxx::document::value projection_document = document{}
                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                << user_account_keys::PICTURES << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<true>(
            login_token_str,
            installation_id,
            merge_document,
            current_timestamp
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account;
    mongocxx::client_session::with_transaction_cb transaction_callback = [&](
            mongocxx::client_session* callback_session
    ) {
        if (!runInitialLoginOperation(
                find_user_account,
                user_accounts_collection,
                user_account_oid,
                login_document,
                projection_document,
                callback_session)
        ) {
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        const ReturnStatus return_status = checkForValidLoginToken(
                find_user_account,
                user_account_oid_str
        );

        if (return_status != ReturnStatus::SUCCESS) {
            response->set_return_status(return_status);
        }
        else {

            //NOTE: It is very important that the picture timestamp is the exact same and not say +2, the android
            // client relies on the timestamp being the exact same as the stored timestamp (not just greater).
            response->set_timestamp(current_timestamp.count());

            //NOTE: checkForValidLoginToken() will not return SUCCESS if findUserAccount has not been set
            const bsoncxx::document::view find_user_account_view = find_user_account->view();

            auto pictures_element = find_user_account_view[user_account_keys::PICTURES];
            if (pictures_element
                && pictures_element.type() == bsoncxx::type::k_array) { //if element exists and is type array

                const auto insert_picture = [&] {

                    //insert new picture and update array
                    if (!insertPictureDocument(
                            current_timestamp,
                            user_pictures_collection,
                            user_accounts_collection,
                            callback_session,
                            user_account_oid,
                            request,
                            find_user_account_view,
                            index_of_array)
                    ) { //if upsert picture fails
                        removePictureArrayElement(
                                index_of_array,
                                user_accounts_collection,
                                find_user_account_view,
                                user_account_oid,
                                callback_session
                        );
                        response->set_return_status(ReturnStatus::LG_ERROR);
                    } else {
                        response->set_return_status(ReturnStatus::SUCCESS);
                    }
                };

                const bsoncxx::array::view picture_reference_array = pictures_element.get_array().value;

                auto picture_element = picture_reference_array[index_of_array];
                if (picture_element
                    && picture_element.type() == bsoncxx::type::k_document) { //if element exists and is type OID
                    const bsoncxx::document::view picture_document = picture_element.get_document().value;

                    //extract member picture oid
                    auto picture_oid_element = picture_document[user_account_keys::pictures::OID_REFERENCE];
                    if (picture_oid_element
                        && picture_oid_element.type() == bsoncxx::type::k_oid) { //if element exists and is type array
                        const bsoncxx::oid picture_oid = picture_oid_element.get_oid().value;

                        //need to delete old stuff then upsert new stuff
                        if (findAndDeletePictureDocument(
                                mongo_cpp_client,
                                user_pictures_collection,
                                picture_oid,
                                current_timestamp,
                                callback_session
                            )
                        ) {
                            insert_picture();
                        }
                    } else { //if element does not exist or is not type array
                        logElementError(
                                __LINE__, __FILE__,
                                picture_oid_element, picture_document,
                                bsoncxx::type::k_oid, user_account_keys::pictures::OID_REFERENCE,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_ACCOUNTS_COLLECTION_NAME
                        );
                        response->set_return_status(ReturnStatus::LG_ERROR);
                        return;
                    }

                } else { //this could mean the element does not exist, or it is a different type than oid (like null)
                    insert_picture();
                }

            } else { //if element does not exist or is not type array
                logElementError(
                        __LINE__, __FILE__,
                        pictures_element, find_user_account_view,
                        bsoncxx::type::k_array, user_account_keys::PICTURES,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }
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
        std::cout << "Exception calling setPictureImplementation() transaction.\n" << e.what() << '\n';
#endif
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_OID", user_account_oid_str,
                "index", std::to_string(index_of_array)
        );
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

}

bool insertPictureDocument(
        const std::chrono::milliseconds& timestamp,
        mongocxx::collection& pictures_collection,
        mongocxx::collection& user_collection,
        mongocxx::client_session* session,
        const bsoncxx::oid& user_account_oid,
        setfields::SetPictureRequest* request,
        const bsoncxx::document::view& user_account_view,
        int index_of_picture
) {

    std::optional<std::string> insert_exception_string;
    bsoncxx::stdx::optional<mongocxx::result::insert_one> insert_success;
    try {

        std::string* file_in_bytes = request->release_file_in_bytes();
        std::string* thumbnail_in_bytes = request->release_thumbnail_in_bytes();

        if(file_in_bytes == nullptr || thumbnail_in_bytes == nullptr) {

            const std::string error_string = std::string("A picture array value was null.\n")
                    .append("file_in_bytes: " + std::to_string(file_in_bytes == nullptr ? -1L : (long)file_in_bytes->size()) + "\n")
                    .append("thumbnail_in_bytes: " + std::to_string(thumbnail_in_bytes == nullptr ? -1L : (long)thumbnail_in_bytes->size()) + "\n");

            //deleting a nullptr has no effect
            delete file_in_bytes;
            delete thumbnail_in_bytes;
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "user_account_doc", user_account_view,
                    "user_account_oid", user_account_oid
            );

            return false;
        }

        const bsoncxx::document::value insert_doc = createUserPictureDoc(
                bsoncxx::oid{},
                user_account_oid,
                timestamp,
                index_of_picture,
                thumbnail_in_bytes,
                request->thumbnail_size(),
                file_in_bytes,
                request->file_size()
        );

        delete file_in_bytes;
        delete thumbnail_in_bytes;

        //insert new user picture
        insert_success = pictures_collection.insert_one(
                *session,
                insert_doc.view()
        );

    }
    catch (const mongocxx::logic_error& e) {
        insert_exception_string = e.what();
    }

    if (insert_success && insert_success->result().inserted_count() == 1) { //insert success

        const bsoncxx::oid inserted_oid = insert_success->inserted_id().get_oid().value;

        const std::string picture_key_with_index = user_account_keys::PICTURES + '.' + std::to_string(index_of_picture);

        //if the upsert was successful, update the user account picture reference
        std::optional<std::string> update_user_exception_string;
        bsoncxx::stdx::optional<mongocxx::result::update> update_user_account;
        try {
            bsoncxx::builder::stream::document update_doc_builder;
            update_doc_builder
                << "$set" << open_document
                    << picture_key_with_index << open_document
                        << user_account_keys::pictures::OID_REFERENCE << bsoncxx::types::b_oid{inserted_oid}
                        << user_account_keys::pictures::TIMESTAMP_STORED << bsoncxx::types::b_date{timestamp}
                    << close_document
                << close_document;

            //change the failed array element
            update_user_account = user_collection.update_one(
                    *session,
                    document{}
                        << "_id" << user_account_oid
                    << finalize,
                    update_doc_builder.view()
            );
        }
        catch (const mongocxx::logic_error& e) {
            update_user_exception_string = std::string(e.what());
        }

        if (!update_user_account) {
            const std::string error_string = std::string("Updating picture array '")
                            .append(user_account_keys::PICTURES)
                            .append(".")
                            .append(std::to_string(index_of_picture))
                            .append("' failed.");
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    update_user_exception_string, error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "verifiedAccountDocument", user_account_view,
                    "Searched_OID", user_account_oid
            );

            return false;
        } else if (update_user_account->modified_count() != 1) {
            const std::string error_string = std::string("Updating picture array '")
                            .append(user_account_keys::PICTURES)
                            .append(".")
                            .append(std::to_string(index_of_picture))
                            .append("' failed.");
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    update_user_exception_string, error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "verifiedAccountDocument", user_account_view,
                    "Searched_OID", user_account_oid,
                    "modified_count", std::to_string(update_user_account->modified_count())
            );

            return false;
        }
    } else { //insert failed
        const std::string error_string = "Insert picture account failed when called from setPicture().";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                insert_exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                "verifiedAccountDocument", user_account_view,
                "pictureSizeInBytes", std::to_string(request->file_size()),
                "indexOfPicture", std::to_string(index_of_picture)
        );
        return false;
    }

    return true;
}
