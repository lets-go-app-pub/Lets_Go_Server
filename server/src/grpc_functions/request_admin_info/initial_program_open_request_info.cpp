//
// Created by jeremiah on 8/26/21.
//

#include <mongocxx/database.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <server_values.h>

#include "request_helper_functions.h"
#include "admin_privileges_vector.h"
#include "request_admin_info.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "handle_function_operation_exception.h"

#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "server_parameter_restrictions.h"
#include "report_values.h"
#include "error_values.h"
#include "general_values.h"
#include "android_specific_values.h"

void initialProgramOpenRequestInfoImplementation(
        const request_admin_info::InitialProgramOpenRequestInfoRequest* request,
        request_admin_info::InitialProgramOpenRequestInfoResponse* response
);

void initialProgramOpenRequestInfo(
        const request_admin_info::InitialProgramOpenRequestInfoRequest* request,
        request_admin_info::InitialProgramOpenRequestInfoResponse* response
) {
    handleFunctionOperationException(
            [&] {
                initialProgramOpenRequestInfoImplementation(request, response);
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request
    );
}

void initialProgramOpenRequestInfoImplementation(
        const request_admin_info::InitialProgramOpenRequestInfoRequest* request,
        request_admin_info::InitialProgramOpenRequestInfoResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;

    {

        std::string error_message;
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
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id,
                store_error_message
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS || !admin_info_doc_value) {

            if(error_message.empty()) {
                error_message = "Error logging in";
            }

            response->set_success(false);
            response->set_error_msg(error_message);
            return;
        }

    }

    const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;

    auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
    if (admin_privilege_element &&
        admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
    } else { //if element does not exist or is not type oid
        logElementError(
                __LINE__, __FILE__,
                admin_privilege_element, admin_info_doc_view,
                bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server.");
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    //request categories and activities from database
    if (!requestAllServerCategoriesActivitiesHelper(
            response->mutable_server_categories(),
            response->mutable_server_activities(),
            accounts_db)
    ) {
        response->Clear();
        response->set_success(false);
        response->set_error_msg("Failed to request categories and/or activities.");
        return;
    }

    response->set_admin_level(admin_level);
    response->mutable_admin_privileges()->CopyFrom(admin_privileges[admin_level]);

    auto desktop_global_values = response->mutable_globals();

    desktop_global_values->set_minimum_activity_or_category_name_size(
            server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE);
    desktop_global_values->set_maximum_activity_or_category_name_size(
            server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE);
    desktop_global_values->set_lowest_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
    desktop_global_values->set_highest_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);
    desktop_global_values->set_activity_icon_width_in_pixels(
            server_parameter_restrictions::ACTIVITY_ICON_WIDTH_IN_PIXELS);
    desktop_global_values->set_activity_icon_height_in_pixels(
            server_parameter_restrictions::ACTIVITY_ICON_HEIGHT_IN_PIXELS);
    desktop_global_values->set_minimum_allowed_distance(server_parameter_restrictions::MINIMUM_ALLOWED_DISTANCE);
    desktop_global_values->set_maximum_allowed_distance(server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE);

    desktop_global_values->set_maximum_number_allowed_bytes(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES);
    desktop_global_values->set_maximum_number_allowed_bytes_first_name(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_FIRST_NAME);
    desktop_global_values->set_maximum_number_allowed_bytes_user_bio(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_BIO);
    desktop_global_values->set_minimum_number_allowed_bytes_inactive_message(
            server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE);
    desktop_global_values->set_maximum_number_allowed_bytes_inactive_message(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE);
    desktop_global_values->set_email_authorization_regex(general_values::EMAIL_AUTHORIZATION_REGEX);
    desktop_global_values->set_male_gender_string(general_values::MALE_GENDER_VALUE);
    desktop_global_values->set_female_gender_string(general_values::FEMALE_GENDER_VALUE);
    desktop_global_values->set_everyone_gender_string(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
    desktop_global_values->set_number_genders_can_match_with(
            server_parameter_restrictions::NUMBER_GENDER_USER_CAN_MATCH_WITH);
    desktop_global_values->set_minimum_time_for_suspension(
            std::chrono::duration_cast<std::chrono::milliseconds>(report_values::MINIMUM_TIME_FOR_SUSPENSION).count());
    desktop_global_values->set_maximum_time_for_suspension(
            std::chrono::duration_cast<std::chrono::milliseconds>(report_values::MAXIMUM_TIME_FOR_SUSPENSION).count());
    desktop_global_values->set_number_times_timed_out_before_ban(report_values::NUMBER_TIMES_TIMED_OUT_BEFORE_BAN);
    desktop_global_values->set_max_number_of_reported_users_admin_can_request_notified(
            report_values::MAX_NUMBER_OF_REPORTED_USERS_ADMIN_CAN_REQUEST_NOTIFIED);
    desktop_global_values->set_time_before_report_checkout_expires(report_values::CHECK_OUT_TIME.count());

    for(const auto& server : GRPC_SERVER_ADDRESSES_URI) {
        auto address_info = desktop_global_values->add_server_address_info();
        address_info->set_address(server.ADDRESS);
        address_info->set_port(server.PORT);
    }

    desktop_global_values->set_maximum_number_allowed_bytes_to_request_error_message(
            error_values::MAXIMUM_NUMBER_ALLOWED_BYTES_TO_REQUEST_ERROR_MESSAGE);

    desktop_global_values->set_extract_errors_total_bytes_key(error_values::EXTRACT_ERRORS_TOTAL_BYTES_KEY);
    desktop_global_values->set_extract_errors_extracted_bytes_key(error_values::EXTRACT_ERRORS_EXTRACTED_BYTES_KEY);
    desktop_global_values->set_extract_errors_total_docs_key(error_values::EXTRACT_ERRORS_TOTAL_DOCS_KEY);
    desktop_global_values->set_extract_errors_extracted_docs_key(error_values::EXTRACT_ERRORS_EXTRACTED_DOCS_KEY);
    desktop_global_values->set_extract_errors_max_search_results(error_values::EXTRACT_ERRORS_MAX_SEARCH_RESULTS);
    desktop_global_values->set_minimum_number_allowed_bytes_modify_error_message_reason(
            server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON);
    desktop_global_values->set_maximum_number_allowed_bytes_modify_error_message_reason(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON);
    desktop_global_values->set_maximum_number_allowed_bytes_event_title(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_EVENT_TITLE);
    desktop_global_values->set_maximum_qr_code_size_in_bytes(server_parameter_restrictions::MAXIMUM_QR_CODE_SIZE_IN_BYTES);
    desktop_global_values->set_maximum_number_allowed_bytes_user_qr_code_message(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_QR_CODE_MESSAGE);
    desktop_global_values->set_event_gender_value(general_values::EVENT_GENDER_VALUE);
    desktop_global_values->set_event_age_value(general_values::EVENT_AGE_VALUE);
    desktop_global_values->set_image_quality_value(android_specific_values::IMAGE_QUALITY_VALUE);
    desktop_global_values->set_picture_thumbnail_maximum_cropped_size_px(android_specific_values::PICTURE_THUMBNAIL_MAXIMUM_CROPPED_SIZE_PX);
    desktop_global_values->set_picture_maximum_cropped_size_px(android_specific_values::PICTURE_MAXIMUM_CROPPED_SIZE_PX);
    desktop_global_values->set_number_pictures_stored_per_account(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT);
    desktop_global_values->set_maximum_picture_thumbnail_size_in_bytes(server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES);
    desktop_global_values->set_maximum_picture_size_in_bytes(server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES);

    response->set_success(true);
}

