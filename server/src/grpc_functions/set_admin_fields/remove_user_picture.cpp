//
// Created by jeremiah on 9/18/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <admin_functions_for_set_values.h>
#include <utility_chat_functions.h>
#include <extract_thumbnail_from_verified_doc.h>
#include <deleted_user_pictures_keys.h>
#include <utility_testing_functions.h>

#include "set_admin_fields.h"

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "store_mongoDB_error_and_exception.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_pictures_keys.h"
#include "chat_room_header_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void removeUserPictureImplementation(
        const set_admin_fields::RemoveUserPictureRequest* request,
        set_admin_fields::SetAdminUnaryCallResponse* response
);

void removeUserPicture(
        const set_admin_fields::RemoveUserPictureRequest* request,
        set_admin_fields::SetAdminUnaryCallResponse* response
) {
    handleFunctionOperationException(
            [&] {
                removeUserPictureImplementation(request, response);
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request
    );
}

void removeUserPictureImplementation(
        const set_admin_fields::RemoveUserPictureRequest* request,
        set_admin_fields::SetAdminUnaryCallResponse* response
) {

    {
        bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
        std::string error_message;
        std::string user_account_oid_str;
        std::string login_token_str;
        std::string installation_id;

        auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                       const std::string& passed_error_message) {
            error_message = passed_error_message;
            admin_info_doc_value = std::move(returned_admin_info_doc);
        };

        const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id,
                store_error_message
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS) {
            response->set_successful(false);
            response->set_error_message(
                    "ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
            return;
        } else if (!admin_info_doc_value) {
            response->set_successful(false);
            response->set_error_message("Could not find admin document.");
            return;
        }

        if (!checkForUpdateUserPrivilege(response->mutable_error_message(), admin_info_doc_value)) {
            response->set_successful(false);
            return;
        }
    }

    if (isInvalidOIDString(request->user_oid())) {
        response->set_error_message("Invalid user ID passed of '" + request->user_oid() + "'.");
        response->set_successful(false);
        return;
    }

    if(isInvalidOIDString(request->picture_oid())) {
        response->set_error_message("Invalid picture ID passed of '" + request->picture_oid() + "'.");
        response->set_successful(false);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    const bsoncxx::oid user_account_oid = bsoncxx::oid{request->user_oid()};
    const bsoncxx::oid picture_oid{request->picture_oid()};

    mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

        bsoncxx::stdx::optional<bsoncxx::document::value> find_and_update_user_account;
        try {
            mongocxx::options::find_one_and_update opts;

            opts.projection(
                document{}
                    << user_account_keys::PICTURES << 1
                << finalize
            );

            opts.return_document(mongocxx::options::return_document::k_after);

            mongocxx::pipeline pipeline;
            pipeline.add_fields(
                document{}
                    << user_account_keys::PICTURES << open_document
                        << "$map" << open_document
                            << "input" << "$" + user_account_keys::PICTURES
                            << "in" << open_document
                                << "$cond" << open_document
                                    << "if" << open_document
                                        << "$eq" << open_array
                                            << picture_oid
                                            << "$$this." + user_account_keys::pictures::OID_REFERENCE
                                        << close_array
                                    << close_document
                                    << "then" << bsoncxx::types::b_null{}
                                    << "else" << "$$this"
                                << close_document
                            << close_document
                        << close_document
                    << close_document
                << finalize
            );

            //update the array element to null for the picture oid
            find_and_update_user_account = user_accounts_collection.find_one_and_update(
                    *callback_session,
                    document{}
                        << "_id" << user_account_oid
                    << finalize,
                    pipeline,
                    opts
            );
        } catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "Searched_OID", user_account_oid,
                    "Picture_OID", picture_oid
            );

            response->set_error_message("Error stored on server.");
            response->set_successful(false);
            return;
        }

        //Even if the user account was not found, it is OK to continue here. If account was deleted
        // do NOT get the pictures from it, they will never be changed again. The values inside
        // user_account_keys::PICTURES will not be changed after the document is put inside the
        // deleted accounts collection. This means that there is no reason to find a new thumbnail
        // simply delete the old one if relevant (done below) and leave the rest alone.

        bsoncxx::stdx::optional<bsoncxx::document::value> find_picture;
        try {
            //find picture document
            find_picture = user_pictures_collection.find_one(
                *callback_session,
                document{}
                    << "_id" << picture_oid
                << finalize
            );
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );

            response->set_error_message("Error stored on server.");
            response->set_successful(false);
            return;
        }

        //findDeletedPicture must be initialized here to keep the array alive
        bsoncxx::stdx::optional<bsoncxx::document::value> find_deleted_picture;

        bsoncxx::document::view picture_doc;

        mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
        mongocxx::collection deleted_pictures_collection = deleted_db[collection_names::DELETED_USER_PICTURES_COLLECTION_NAME];

        if(find_picture) { //picture exists inside of collection
            std::cout << "Picture was found in user account\n";
            picture_doc = find_picture->view();
        } else { //picture was deleted (or never existed)

            try {
                //find deleted picture account document
                find_deleted_picture = deleted_pictures_collection.find_one(
                    *callback_session,
                    document{}
                        << "_id" << picture_oid
                    << finalize
                );
            } catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );
                response->set_error_message("Error stored on server.");
                response->set_successful(false);
                return;
            }

            if(find_deleted_picture) { //deleted picture was found
                std::cout << "Picture was found in deleted pictures\n";
                picture_doc = find_deleted_picture->view();
            } else { //deleted picture was not found
                response->set_error_message("Picture was not found in 'User Picture Collection' OR 'Deleted Picture Collection' unable to delete anything.");
                response->set_successful(false);
                return;
            }
        }

        bsoncxx::array::view picture_thumbnail_references;
        bsoncxx::oid picture_set_by_oid;

        try {
            picture_thumbnail_references = extractFromBsoncxx_k_array(
                    picture_doc,
                    user_pictures_keys::THUMBNAIL_REFERENCES
            );
            picture_set_by_oid = extractFromBsoncxx_k_oid(
                    picture_doc,
                    user_pictures_keys::USER_ACCOUNT_REFERENCE
            );
        }
        catch (const ErrorExtractingFromBsoncxx& e) {
            response->set_error_message(e.what());
            response->set_successful(false);
            return;
        }

        if(!find_picture) { //picture extracted from deleted collection
            auto references_removed_after_delete = picture_doc[deleted_user_pictures_keys::REFERENCES_REMOVED_AFTER_DELETE];
            if(references_removed_after_delete && references_removed_after_delete.type() == bsoncxx::type::k_bool) {
                //All references for this picture have already been removed. No need to continue.
                response->set_successful(true);
                return;
            }
        }

        //The values set here will be the default values if no thumbnail is found for
        // the user. For example if all of their pictures were deleted by an admin.
        int new_thumbnail_size = 0;
        std::string new_thumbnail_reference_oid;
        std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

        if(find_and_update_user_account) {

            const bsoncxx::document::view user_account_doc = find_and_update_user_account->view();

            auto set_new_thumbnail_value = [&](
                    std::string& /*thumbnail*/,
                    int _thumbnail_size,
                    const std::string& _thumbnail_reference_oid,
                    const int /*index*/,
                    const std::chrono::milliseconds& /*thumbnail_timestamp*/
            ) {
                new_thumbnail_size = _thumbnail_size;
                new_thumbnail_reference_oid = _thumbnail_reference_oid;
            };

            ExtractThumbnailAdditionalCommands extract_thumbnail_additional_commands;
            extract_thumbnail_additional_commands.setupForAddArrayValuesToSet(picture_thumbnail_references);

            if(!extractThumbnailFromUserAccountDoc(
                    accounts_db,
                    user_account_doc,
                    user_account_oid,
                    callback_session,
                    set_new_thumbnail_value,
                    extract_thumbnail_additional_commands)
            ) {
                //NOTE: can continue here, still want the passed picture removed even if the new thumbnail is not user
                new_thumbnail_size = 0;
                new_thumbnail_reference_oid.clear();
            }
        }

        for(auto thumbnail_ref_ele : picture_thumbnail_references) {
            if(thumbnail_ref_ele.type() == bsoncxx::type::k_utf8) {
                const std::string chat_room_id = thumbnail_ref_ele.get_string().value.to_string();

                if(isInvalidChatRoomId(chat_room_id)) {
                    //NOTE: want this to complete so the picture is deleted and this will not happen again
                    const std::string error_str = "Invalid chat room id returned.";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_str,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                            "type", convertBsonTypeToString(thumbnail_ref_ele.type()),
                            "id", chat_room_id,
                            "array", makePrettyJson(picture_thumbnail_references)
                    );

                    //NOTE: want this to complete so the picture is deleted and this will not happen again
                    continue;
                }

                mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
                mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

                mongocxx::stdx::optional<mongocxx::result::update> update_chat_room;
                try {
                    //clear picture from chat room thumbnail
                    update_chat_room = chat_room_collection.update_one(
                        *callback_session,
                        document{}
                            << "_id" << chat_room_header_keys::ID
                            << std::string(chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM).append(".").append(chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID) << picture_set_by_oid
                        << finalize,
                        document{}
                            << "$set" << open_document
                                << std::string(chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM).append(".$.").append(chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE) << new_thumbnail_reference_oid
                                << std::string(chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM).append(".$.").append(chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP) << bsoncxx::types::b_date{current_timestamp}
                                << std::string(chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM).append(".$.").append(chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE) << new_thumbnail_size
                            << close_document
                        << finalize
                    );
                }
                catch (const mongocxx::logic_error& e) {
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), std::string(e.what()),
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id
                    );
                    response->set_error_message("Error stored on server.");
                    response->set_successful(false);
                    return;
                }

                if(!update_chat_room) {
                    const std::string error_str = "Failed to update chat room and clear picture.";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_str,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id
                    );

                    //NOTE: want this to complete so the picture is deleted and this will not happen again
                    continue;
                }

            } else {
                const std::string error_str = "Invalid type returned from THUMBNAIL_REFERENCES";

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_str,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                        "type", convertBsonTypeToString(thumbnail_ref_ele.type()),
                        "array", makePrettyJson(picture_thumbnail_references)
                );

                //NOTE: want this to complete so the picture is deleted and this will not happen again
                continue;
            }
        }

        if(find_picture) { //if picture existed inside of user pictures account
            if(!deletePictureDocument(
                    picture_doc,
                    user_pictures_collection,
                    deleted_pictures_collection,
                    picture_oid,
                    current_timestamp,
                    ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE,
                    request->login_info().admin_name(),
                    callback_session)
            ) {
                response->set_error_message("Error stored on server.");
                response->set_successful(false);
                return;
            }
        }

        //This needs to run regardless of it the picture existed inside the user pictures account.
        // That is because it can be used to delete a picture that was already stored inside the
        // deleted account and so did not need to be deleted by this function instance.
        try {
            //update deleted picture to all references removed
            deleted_pictures_collection.update_one(
                    *callback_session,
                    document{}
                    << "_id" << picture_oid
                    << finalize,
                    document{}
                        << "$set" << open_document
                            << deleted_user_pictures_keys::REFERENCES_REMOVED_AFTER_DELETE << true
                        << close_document
                    << finalize
                );
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            response->set_error_message("Error stored on server.");
            response->set_successful(false);
            return;
        }

        response->set_successful(true);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        session.with_transaction(transaction_callback);
    } catch (const mongocxx::logic_error& e) {
        //Finished
        response->set_error_message("Exception calling transaction.\n" + std::string(e.what()));
        response->set_successful(false);
    }

}