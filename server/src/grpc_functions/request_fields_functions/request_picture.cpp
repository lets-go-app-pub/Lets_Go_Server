//
// Created by jeremiah on 3/19/21.
//
#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <global_bsoncxx_docs.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>
#include <request_helper_functions.h>
#include <store_mongoDB_error_and_exception.h>

#include "bsoncxx/builder/stream/document.hpp"


#include "request_fields_functions.h"
#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "general_values.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "empty_or_deleted_picture_info.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void requestPicturesImplementation(
        const request_fields::PictureRequest* request,
        grpc::ServerWriterInterface<request_fields::PictureResponse>* response
);

void requestPictures(
        const request_fields::PictureRequest* request,
        grpc::ServerWriterInterface<request_fields::PictureResponse>* response
) {
    handleFunctionOperationException(
            [&] {
                requestPicturesImplementation(request, response);
            },
            [&] {
                request_fields::PictureResponse pictureResponse;
                pictureResponse.set_return_status(ReturnStatus::DATABASE_DOWN);
                response->Write(pictureResponse);
            },
            [&] {
                request_fields::PictureResponse pictureResponse;
                pictureResponse.set_return_status(ReturnStatus::LG_ERROR);
                response->Write(pictureResponse);
            },
            __LINE__, __FILE__, request
    );
}

void requestPicturesImplementation(
        const request_fields::PictureRequest* request,
        grpc::ServerWriterInterface<request_fields::PictureResponse>* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        request_fields::PictureResponse picture_response;
        picture_response.set_return_status(basic_info_return_status);
        response->Write(picture_response);
        return;
    }

    if(request->requested_indexes().empty() || request->requested_indexes().size() > general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) {
        request_fields::PictureResponse picture_response;
        picture_response.set_return_status(INVALID_PARAMETER_PASSED);
        response->Write(picture_response);
        return;
    }

    std::set<int> requested_indexes_set;

    for(int index : request->requested_indexes()) {
        if (index < 0 || general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT <= index) {
            request_fields::PictureResponse picture_response;
            picture_response.set_return_status(INVALID_PARAMETER_PASSED);
            response->Write(picture_response);
            return;
        } else {
            requested_indexes_set.insert(index);
        }
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

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

    bsoncxx::stdx::optional<bsoncxx::document::value> find_and_update_user_account;

    mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

        if (!runInitialLoginOperation(
                find_and_update_user_account,
                user_accounts_collection,
                user_account_oid,
                login_document,
                projection_document,
                callback_session)
        ) {
            request_fields::PictureResponse picture_response;
            picture_response.set_return_status(LG_ERROR);
            response->Write(picture_response);
            return;
        }

        ReturnStatus return_status = checkForValidLoginToken(
                find_and_update_user_account,
                user_account_oid_str
        );

        if (return_status != SUCCESS) {
            request_fields::PictureResponse picture_response;
            picture_response.set_return_status(return_status);
            response->Write(picture_response);
            return;
        }

        const bsoncxx::document::view user_account_doc_view = *find_and_update_user_account;

        std::function<void(int)> set_picture_empty_response =
                [&response, &current_timestamp](int index_number) {

            request_fields::PictureResponse picture_response;

            //this means there is no picture at this index
            EmptyOrDeletedPictureInfo::savePictureInfoClient(
                    &picture_response,
                    current_timestamp,
                    index_number
            );

            response->Write(picture_response);
        };

        auto set_picture_to_response =
                [&response, &current_timestamp](
                        const bsoncxx::oid& /*picture_oid*/,
                        std::string&& picture_byte_string,
                        int picture_size,
                        int index_number,
                        const std::chrono::milliseconds& picture_timestamp,
                        bool /*extracted_from_deleted_pictures*/,
                        bool /*references_removed_after_delete*/
                        ) {

                    //NOTE: no reason to send picture_oid back to the client

                    request_fields::PictureResponse picture_response;

                    picture_response.set_timestamp(current_timestamp.count());
                    picture_response.mutable_picture_info()->set_file_size(picture_size);
                    picture_response.mutable_picture_info()->set_file_in_bytes(std::move(picture_byte_string));
                    picture_response.mutable_picture_info()->set_index_number(index_number);
                    picture_response.mutable_picture_info()->set_timestamp_picture_last_updated(picture_timestamp.count());
                    picture_response.set_return_status(SUCCESS);

                    response->Write(picture_response);
                };

        if(!requestPicturesHelper(
                requested_indexes_set,
                accounts_db,
                user_account_oid,
                user_account_doc_view,
                mongo_cpp_client,
                user_accounts_collection,
                current_timestamp,
                set_picture_empty_response,
                set_picture_to_response,
                callback_session)
        ) {
            request_fields::PictureResponse picture_response;
            picture_response.set_return_status(LG_ERROR);
            response->Write(picture_response);
            return;
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
        std::cout << "Exception calling requestPicturesImplementation() transaction.\n" << e.what() << '\n';
#endif

        std::string requested_indexes;
        for(int i : requested_indexes_set) {
            requested_indexes += (char)(i + '0');
        }

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional <std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_OID", user_account_oid_str,
                "requested_indexes", requested_indexes
        );

        request_fields::PictureResponse picture_response;
        picture_response.set_return_status(LG_ERROR);
        response->Write(picture_response);
        return;
    }

}



