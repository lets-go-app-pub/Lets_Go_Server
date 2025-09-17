//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <grpc_function_server_template.h>
#include <store_mongoDB_error_and_exception.h>
#include <sstream>
#include <handle_function_operation_exception.h>

#include "bsoncxx/builder/stream/document.hpp"

#include "request_fields_functions.h"
#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "icons_info_keys.h"
#include "user_account_keys.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
void requestIconsHelper(
        grpc::ServerWriterInterface<request_fields::ServerIconsResponse>* response,
        bsoncxx::document::view find_icons_doc_view,
        const mongocxx::database& accounts_db
);

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void requestServerIconsImplementation(
        const request_fields::ServerIconsRequest* request,
        grpc::ServerWriterInterface<request_fields::ServerIconsResponse>* response
);

void requestServerIcons(
        const request_fields::ServerIconsRequest* request,
        grpc::ServerWriterInterface<request_fields::ServerIconsResponse>* response
) {

    handleFunctionOperationException(
            [&] {
                requestServerIconsImplementation(request, response);
            },
            [&] {
                request_fields::ServerIconsResponse responseMessage;
                responseMessage.set_return_status(ReturnStatus::DATABASE_DOWN);
                response->Write(responseMessage);
            },
            [&] {
                request_fields::ServerIconsResponse responseMessage;
                responseMessage.set_return_status(ReturnStatus::LG_ERROR);
                response->Write(responseMessage);
            },
            __LINE__, __FILE__, request
    );

}

void requestServerIconsImplementation(
        const request_fields::ServerIconsRequest* request,
        grpc::ServerWriterInterface<request_fields::ServerIconsResponse>* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    auto store_error_message = [&](
            bsoncxx::stdx::optional<bsoncxx::document::value>& /*returned_admin_info_doc*/,
            const std::string& /*passed_error_message*/) {
        //The response does not have a 'place' for an error message.
    };

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ADMIN_AND_CLIENT,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id,
            store_error_message
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        request_fields::ServerIconsResponse response_message;
        response_message.set_return_status(basic_info_return_status);
        response->Write(response_message);
        return;
    }

    if(!request->login_info().admin_info_used() && request->icon_index().empty()) {
        request_fields::ServerIconsResponse response_message;
        response_message.set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        response->Write(response_message);
        return;
    }

    std::vector<long> index_to_request;
    {
        std::unordered_set<long> index_to_request_set;
        for (long index: request->icon_index()) {
            auto inserted = index_to_request_set.emplace(index);
            if(inserted.second) {
                index_to_request.emplace_back(index);
            }
        }

        std::sort(index_to_request.begin(), index_to_request.end());
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const bsoncxx::document::value merge_document = document{} << finalize;

    std::shared_ptr<document> projection_document = std::make_shared<document>();

    (*projection_document)
            << user_account_keys::FIRST_NAME << 1;

    //will return an empty document if user is admin to retrieve all icons
    if (!request->login_info().admin_info_used()) {
        bsoncxx::builder::stream::document find_icons_document;
        bsoncxx::builder::basic::array index_to_find;

        const auto set_return_status = [&response](const ReturnStatus& return_status) {
            request_fields::ServerIconsResponse response_message;
            response_message.set_return_status(return_status);
            response->Write(response_message);
        };

        for (const auto index: request->icon_index()) {
            index_to_find.append(bsoncxx::types::b_int64{index});
        }

        find_icons_document
                << "_id" << open_document
                << "$in" << index_to_find.view()
                << close_document;

        /** NOTE: This does not need a transaction because the icons are not part of the 'account'. **/

        const auto set_success = [&](const bsoncxx::document::view& /*user_account_doc_view*/) {

            mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
            mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

            mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

            requestIconsHelper(
                    response,
                    find_icons_document.view(),
                    accounts_db
            );
        };

        grpcValidateLoginFunctionTemplate<true>(
                user_account_oid_str,
                login_token_str,
                installation_id,
                current_timestamp,
                merge_document,
                set_return_status,
                set_success,
                projection_document
        );
    } else { //admin request
        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

        requestIconsHelper(
                response,
                document{} << finalize,
                accounts_db
        );
    }
}

void requestIconsHelper(
        grpc::ServerWriterInterface<request_fields::ServerIconsResponse>* response,
        const bsoncxx::document::view find_icons_doc_view,
        const mongocxx::database& accounts_db
) {

    mongocxx::collection icons_info_collection = accounts_db[collection_names::ICONS_INFO_COLLECTION_NAME];

    mongocxx::stdx::optional<mongocxx::cursor> find_icons_cursor;
    std::optional<std::string> find_icons_exception_string;

    try {
        mongocxx::options::find opts;

        opts.sort(
            document{}
                << icons_info_keys::INDEX << 1
            << finalize
        );

        find_icons_cursor = icons_info_collection.find(find_icons_doc_view, opts);
    } catch (const mongocxx::logic_error& e) {
        find_icons_exception_string = e.what();
    }

    if (find_icons_cursor && find_icons_cursor->begin() != find_icons_cursor->end()) {

        int number_sent = 0;
        for (const auto& icon_doc_view: *find_icons_cursor) {
            request_fields::ServerIconsResponse response_message;

            //basic image
            std::string basic_image_file;
            long basic_image_file_size;
            bool icon_is_active;
            std::chrono::milliseconds icon_last_updated_timestamp;

            try {

                response_message.set_index_number(
                        extractFromBsoncxx_k_int64(
                                icon_doc_view,
                                icons_info_keys::INDEX
                        )
                );

                basic_image_file = extractFromBsoncxx_k_utf8(
                        icon_doc_view,
                        icons_info_keys::ICON_IN_BYTES
                );

                basic_image_file_size = extractFromBsoncxx_k_int64(
                        icon_doc_view,
                        icons_info_keys::ICON_SIZE_IN_BYTES
                );

                icon_is_active = extractFromBsoncxx_k_bool(
                        icon_doc_view,
                        icons_info_keys::ICON_ACTIVE
                );

                icon_last_updated_timestamp = extractFromBsoncxx_k_date(
                        icon_doc_view,
                        icons_info_keys::TIMESTAMP_LAST_UPDATED
                ).value;

            } catch (const ErrorExtractingFromBsoncxx& e) {
                //Error already stored.
                continue;
            }

            if ((long) basic_image_file.length() != basic_image_file_size) { //if file is corrupt

                //get current largest _id value
                mongocxx::stdx::optional<mongocxx::result::update> updateOneResult;
                std::optional<std::string> updateIconExceptionString;
                try {
                    updateOneResult = icons_info_collection.update_one(
                            document{}
                                 << "_id" << bsoncxx::types::b_int64{
                                   response_message.index_number()}
                                 << finalize,
                           document{}
                                 << "$set" << open_document
                                 << icons_info_keys::TIMESTAMP_LAST_UPDATED << bsoncxx::types::b_date{
                                 std::chrono::milliseconds{
                                         -2L}} //setting this so anything still with a working icon will not request the corrupt one
                                 << icons_info_keys::ICON_IN_BYTES << ""
                                 << icons_info_keys::ICON_SIZE_IN_BYTES << bsoncxx::types::b_int64{
                                 0}
                                 << close_document
                             << finalize);
                } catch (const mongocxx::logic_error& e) {
                    updateIconExceptionString = e.what();
                }

                if (!updateOneResult) {

                    std::stringstream errorString{};
                    errorString
                            << "A corrupted icon file was found however the updated failed to remove it."
                            << "\nbasicImageFile.length(): " << basic_image_file.length() << " basicImageFileSize: " << basic_image_file_size;

                    storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                  updateIconExceptionString, errorString.str(),
                                                  "database", database_names::ACCOUNTS_DATABASE_NAME,
                                                  "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                                  "index_number", std::to_string(response_message.index_number())
                    );

                    //NOTE: need to continue here, see below for why
                }

                //NOTE: if values are not returned here, the client will continue to request this icon even though it is corrupt
                response_message.set_icon_in_bytes("");
                response_message.set_icon_size_in_bytes(0);
                response_message.set_is_active(false);

            }
            else { //file was not corrupt
                response_message.set_icon_in_bytes(std::move(basic_image_file));
                response_message.set_icon_size_in_bytes((int) basic_image_file_size);
                response_message.set_is_active(icon_is_active);
            }

            response_message.set_return_status(SUCCESS);
            response_message.set_icon_last_updated_timestamp(icon_last_updated_timestamp.count());

            response->Write(response_message);
            number_sent++;
        }

        if (number_sent == 0) {
            //NOTE: error was already stored here
            request_fields::ServerIconsResponse responseMessage;
            responseMessage.set_return_status(LG_ERROR);
            response->Write(responseMessage);
        }

    } else {

        const std::string error_string = "Could not find ID inside ACTIVITIES_INFO_COLLECTION_NAME, this should ALWAYS exist.";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                find_icons_exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ICONS_INFO_COLLECTION_NAME,
                "find_document", makePrettyJson(find_icons_doc_view)
        );

    }

}
