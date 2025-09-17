//
// Created by jeremiah on 9/13/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <UserAccountStatusEnum.grpc.pb.h>
#include <admin_functions_for_set_values.h>
#include <DisciplinaryActionType.grpc.pb.h>
#include <report_handled_move_reason.h>
#include <report_helper_functions.h>

#include "set_admin_fields.h"

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "store_mongoDB_error_and_exception.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "server_parameter_restrictions.h"
#include "report_values.h"
#include "admin_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void runSetServerAccessStatusImplementation(
        const set_admin_fields::SetAccessStatusRequest* request,
        set_admin_fields::SetAccessStatusResponse* response
);


void setServerAccessStatus(
        const set_admin_fields::SetAccessStatusRequest* request,
        set_admin_fields::SetAccessStatusResponse* response
) {
    handleFunctionOperationException(
            [&] {
                runSetServerAccessStatusImplementation(request, response);
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

void runSetServerAccessStatusImplementation(
        const set_admin_fields::SetAccessStatusRequest* request,
        set_admin_fields::SetAccessStatusResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string user_account_oid_str;
    {
        std::string error_message;
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
        } else if (!checkForUpdateUserPrivilege(response->mutable_error_message(),
                                                admin_info_doc_value)) {
            response->set_successful(false);
            return;
        }
    }

    DisciplinaryActionTypeEnum action_taken;

    switch(request->new_account_status()) {
        case STATUS_ACTIVE:
            action_taken = DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_ACTIVE;
            break;
        case STATUS_REQUIRES_MORE_INFO:
            action_taken = DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_REQUIRES_MORE_INFO;
            break;
        case STATUS_SUSPENDED:
            action_taken = DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_SUSPENDED;
            break;
        case STATUS_BANNED:
            action_taken = DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_BANNED;
            break;
        default:
            response->set_successful(false);
            response->set_error_message(
                    "Invalid account status of '" + UserAccountStatus_Name(request->new_account_status()) + "' passed.");
            return;
    }

    if (request->inactive_message().size() < server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE
        || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE < request->inactive_message().size()) {
        response->set_successful(false);
        response->set_error_message(
                "Inactive message passed was incorrect length.\nMust be between " +
                std::to_string(server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE) + "and " +
                std::to_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE) + " characters.\nMessage contains " +
                std::to_string(request->inactive_message().size()) + " characters.");
        return;
    }

    if(isInvalidOIDString(user_account_oid_str)) {
        response->set_successful(false);
        response->set_error_message("User id '" + user_account_oid_str +
                                    "' is invalid.");
        return;
    }

    const bsoncxx::oid user_account_oid = bsoncxx::oid{user_account_oid_str};

    const std::chrono::hours suspension_duration = std::chrono::duration_cast<std::chrono::hours>(
            std::chrono::milliseconds{request->duration_in_millis()});

    if (request->new_account_status() == UserAccountStatus::STATUS_SUSPENDED
        && (suspension_duration < report_values::MINIMUM_TIME_FOR_SUSPENSION
            || report_values::MAXIMUM_TIME_FOR_SUSPENSION < suspension_duration)
            ) {
        response->set_successful(false);
        response->set_error_message(
                "Invalid amount of time passed for the suspension.");
        return;
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    auto error_func = [&response](const std::string& error_str) {
        response->set_successful(false);
        response->set_error_message(error_str);
    };

    auto end_time = std::chrono::milliseconds{-1};

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::stdx::optional<mongocxx::result::update> update_doc;
    try {

        bsoncxx::builder::stream::document set_document;

        set_document
            << user_account_keys::STATUS << request->new_account_status()
            << user_account_keys::INACTIVE_MESSAGE << request->inactive_message();

        switch (request->new_account_status()) {
            case STATUS_ACTIVE:
            case STATUS_REQUIRES_MORE_INFO:
            case STATUS_BANNED: {
                set_document
                        << user_account_keys::INACTIVE_END_TIME << bsoncxx::types::b_null{};
                break;
            }
            case STATUS_SUSPENDED: {
                end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                        current_timestamp + suspension_duration);
                set_document
                        << user_account_keys::INACTIVE_END_TIME << bsoncxx::types::b_date{end_time};
                break;
            }
            default:
                response->set_successful(false);
                response->set_error_message(
                        "Invalid account status of '" + UserAccountStatus_Name(request->new_account_status()) + "' passed.");
                return;
        }

        //find and update user account document
        update_doc = user_accounts_collection.update_one(
            document{}
                << "_id" << user_account_oid
                << user_account_keys::STATUS << open_document
                    << "$ne" << UserAccountStatus::STATUS_REQUIRES_MORE_INFO
                << close_document
            << finalize,
            document{}
                << "$set" << set_document.view()
                << "$push" << open_document
                    << user_account_keys::DISCIPLINARY_RECORD << open_document
                        << user_account_keys::disciplinary_record::SUBMITTED_TIME << bsoncxx::types::b_date{current_timestamp}
                        << user_account_keys::disciplinary_record::END_TIME << bsoncxx::types::b_date{end_time}
                        << user_account_keys::disciplinary_record::ACTION_TYPE << action_taken
                        << user_account_keys::disciplinary_record::REASON << request->inactive_message()
                        << user_account_keys::disciplinary_record::ADMIN_NAME << request->login_info().admin_name()
                    << close_document
                << close_document
            << finalize
        );

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "ObjectID_used", user_account_oid_str
        );

        error_func("Error exception thrown when setting user info.");
        return;
    }

    if (!update_doc) {
        error_func("Failed to update user info.\n'!update_doc' returned.");
    } else if (update_doc->matched_count() != 1) {
        //this could happen when either
        // 1) the account was deleted
        // 2) the account state was set to REQUIRES_MORE_INFO
        error_func("Failed to find user, could happen when either.\n1) Account has been deleted.\n2) The account state was UserAccountStatus::STATUS_REQUIRES_MORE_INFO.");
    } else if (update_doc->modified_count() != 1) {
        error_func("Failed to update user info.\nupdate_doc->modified_count() = " +
                   std::to_string(update_doc->modified_count()));
    } else {

        //if set to suspended or banned, remove the reports
        if(request->new_account_status() == STATUS_SUSPENDED
            || request->new_account_status() == STATUS_BANNED
            ) {

            DisciplinaryActionTypeEnum disciplinary_action;

            if(request->new_account_status() == STATUS_SUSPENDED) {
                disciplinary_action = DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_SUSPENDED;
            } else {
                disciplinary_action = DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_BANNED;
            }

            std::string admin_name;

            const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
            auto admin_name_element = admin_info_doc_view[admin_account_key::NAME];
            if (admin_name_element
                && admin_name_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type int32
                admin_name = admin_name_element.get_string().value.to_string();
            } else { //if element does not exist or is not type int32
                logElementError(
                        __LINE__, __FILE__,
                        admin_name_element, admin_info_doc_view,
                        bsoncxx::type::k_utf8, admin_account_key::PRIVILEGE_LEVEL,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
                );

                response->set_successful(false);
                response->set_error_message("Error stored on server.");
                return;
            }

            moveOutstandingReportsToHandled(
                    mongo_cpp_client,
                    current_timestamp,
                    user_account_oid,
                    admin_name,
                    ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN,
                    nullptr,
                    disciplinary_action
                    );
        }

        response->set_successful(true);
        response->set_current_timestamp(current_timestamp.count());
        response->set_final_timestamp(end_time.count());
    }

}