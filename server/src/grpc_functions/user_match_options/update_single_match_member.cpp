//
// Created by jeremiah on 3/9/22.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <handle_function_operation_exception.h>
#include <utility_general_functions.h>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>
#include <user_account_keys.h>
#include <global_bsoncxx_docs.h>
#include <store_mongoDB_error_and_exception.h>
#include <how_to_handle_member_pictures.h>

#include <update_single_other_user.h>
#include "user_match_options.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "chat_room_commands_helper_functions.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void updateSingleMatchMemberImplementation(
        const user_match_options::UpdateSingleMatchMemberRequest* request,
        UpdateOtherUserResponse* response
);

void updateSingleMatchMember(
        const user_match_options::UpdateSingleMatchMemberRequest* request,
        UpdateOtherUserResponse* response
) {
    handleFunctionOperationException(
            [&] {
                updateSingleMatchMemberImplementation(request, response);
                },
                [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
                },
                [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
                },
                __LINE__, __FILE__, request);
}

void updateSingleMatchMemberImplementation(
        const user_match_options::UpdateSingleMatchMemberRequest* request,
        UpdateOtherUserResponse* response
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
        response->set_return_status(basic_info_return_status);
        return;
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    //NOTE: Make sure to use the error_checked_member instead of request->chat_room_member_info(). It
    // has been set up properly.
    OtherUserInfoForUpdates error_checked_member;

    if(!filterAndStoreSingleUserToBeUpdated(
            request->chat_room_member_info(),
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            error_checked_member)
    ) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        response->set_timestamp_returned(-2L);
        return;
    }

    const std::string member_account_oid_string = error_checked_member.account_oid();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    response->set_timestamp_returned(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default

    const bsoncxx::oid user_account_oid{user_account_oid_str};
    const bsoncxx::oid member_account_oid{member_account_oid_string};

    const bsoncxx::document::value merge_document = document{} << finalize;

    std::shared_ptr<std::vector<bsoncxx::document::value>> append_to_and_statement_doc = std::make_shared<std::vector<bsoncxx::document::value>>();

    //checks if matched user has been extracted to device (it could have been removed by now)
    const bsoncxx::document::value user_required_in_extracted_list_condition =
        document{}
            << "$reduce" << open_document
                << "input" << "$" + user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST
                << "initialValue" << false
                << "in" << open_document

                    << "$cond" << open_document

                        //if the chat room is the chat room ID
                        << "if" << open_document

                            << "$eq" << open_array
                                << "$$this." + user_account_keys::accounts_list::OID
                                << bsoncxx::oid{member_account_oid_string}
                            << close_array

                        << close_document

                        << "then" << true
                        << "else" << "$$value"
                    << close_document
                << close_document
            << close_document
        << finalize;

    append_to_and_statement_doc->emplace_back(user_required_in_extracted_list_condition.view());

    const bsoncxx::document::value projection_document = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
        << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<false>(
            login_token_str,
            installation_id,
            merge_document,
            current_timestamp,
            append_to_and_statement_doc
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account;

    if (!runInitialLoginOperation(
            find_user_account,
            user_accounts_collection,
            user_account_oid,
            login_document,
            projection_document)
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    const ReturnStatus return_status = checkForValidLoginToken(
            find_user_account,
            user_account_oid_str
    );

    if(return_status == ReturnStatus::UNKNOWN) {
        response->set_return_status(ReturnStatus::SUCCESS);
        response->set_match_still_valid(false);
        response->set_timestamp_returned(current_timestamp.count());
    } else if (return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(return_status);
    } else {

        bsoncxx::stdx::optional<bsoncxx::document::value> find_match_user_account;
        try {

            mongocxx::options::find find_user_acct_opts;

            //project only the specific element representing this user from the header
            find_user_acct_opts.projection(buildUpdateSingleOtherUserProjectionDoc());

            //if account exists and is inside chat room
            find_match_user_account = user_accounts_collection.find_one(
                document{}
                    << "_id" << member_account_oid
                << finalize,
                find_user_acct_opts
            );
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", member_account_oid_string
            );
        }

        if (find_match_user_account) { //if the member is found

            const bsoncxx::document::view current_user_document = find_match_user_account->view();
            UpdateOtherUserResponse response_msg;

            updateSingleOtherUser(
                    mongo_cpp_client,
                    accounts_db,
                    current_user_document,
                    user_accounts_collection,
                    "Called from updateSingleMatchMember().",
                    member_account_oid_string,
                    error_checked_member,
                    current_timestamp,
                    true,
                    error_checked_member.account_state(),
                    std::chrono::milliseconds{-1},
                    HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
                    &response_msg,
                    [&response, &response_msg, &current_timestamp]() {
                        response->Swap(&response_msg);
                        response->set_match_still_valid(true);
                        response->set_timestamp_returned(current_timestamp.count());
                    }
            );

        } else {  //member account does not exist

            //NOTE: this could technically happen if the user was deleted after this RPC was sent
            const std::string error_string = "A member that was passed from the device to be updated was not found .\n";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", member_account_oid_string
            );

            UpdateOtherUserResponse error_response;
            error_response.set_return_status(ReturnStatus::SUCCESS);
            error_response.set_match_still_valid(false);
            error_response.set_timestamp_returned(current_timestamp.count());

            response->Swap(&error_response);
        }
    }

}
