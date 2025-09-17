//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <global_bsoncxx_docs.h>
#include <handle_function_operation_exception.h>
#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>

#include "set_admin_fields.h"

#include "store_mongoDB_error_and_exception.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "activities_info_keys.h"
#include "icons_info_keys.h"
#include "admin_account_keys.h"
#include "server_parameter_restrictions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool setServerCategoryOrActivityImplementation(
        set_admin_fields::SetServerActivityOrCategoryRequest& request,
        std::string& error_message,
        bool is_activity
);

bool setServerCategoryOrActivity(
        set_admin_fields::SetServerActivityOrCategoryRequest& request,
        std::string& error_message,
        bool is_activity
) {
    bool successful = true;

    handleFunctionOperationException(
            [&] {
                if (!setServerCategoryOrActivityImplementation(request, error_message, is_activity)) {
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

bool setServerCategoryOrActivityImplementation(
        set_admin_fields::SetServerActivityOrCategoryRequest& request,
        std::string& error_message,
        bool is_activity
) {

    /** This is set up to be run fairly rarely. It must iterate through and rebuild the activities array every time
     * it runs (see buildSetCategoryOrActivityDoc()). Also it is bottle necked inside of a single document and so
     * if users can say add Activities it would probably be slow (unless MongoDB does something about it). Right now
     * it should be ok because 99.99% of them are just reads. **/

    {
        bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
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
            error_message = "Error stored on server.";
            return false;
        }

        if (!admin_privileges[admin_level].update_activities_and_categories()) {
            error_message = "Admin level " + AdminLevelEnum_Name(admin_level) +
                            " does not have 'update_activities_and_categories' access.";
            return false;
        }
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];
    mongocxx::collection icons_info_collection = accounts_db[collection_names::ICONS_INFO_COLLECTION_NAME];

    if (request.display_name().size() < server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE
        || request.display_name().size() > server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE) {
        error_message = "Display_Name must be between " + std::to_string(
                server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE) +
                        " & " + std::to_string(server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE) +
                        " chars long.";
        return false;
    }

    if (request.icon_display_name().size() < server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE
        || request.icon_display_name().size() > server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE) {
        error_message = "Icon_Display_Name must be between " + std::to_string(
                server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE) +
                        " & " + std::to_string(server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE) +
                        " chars long.";
        return false;
    }

    const std::string stored_name = convertCategoryActivityNameToStoredName(request.display_name());

    //The HIGHEST_ALLOWED_AGE+1 is so that activities/categories can be set to 121 to be inactivated
    if (request.min_age() < server_parameter_restrictions::LOWEST_ALLOWED_AGE
        || request.min_age() > (server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1)
            ) {
        error_message = "Minimum age must be between " + std::to_string(
                server_parameter_restrictions::LOWEST_ALLOWED_AGE) +
                        " & " + std::to_string(server_parameter_restrictions::HIGHEST_ALLOWED_AGE) + " years old.";
        return false;
    }

    if (is_activity) { //activity type message

#if defined(_RELEASE) || defined(LG_TESTING)
        //If this is defined during _DEBUG, will be unable to set default activities and categories.
        if (request.category_index() == 0) {
            error_message = "Cannot add or modify activities to the 'Unknown' category.";
            return false;
        }
#endif

        //check if icon index exists
        bsoncxx::stdx::optional<bsoncxx::document::value> find_icons_document;
        try {
            find_icons_document = icons_info_collection.find_one(
                    document{}
                            << icons_info_keys::INDEX << bsoncxx::types::b_int64{(long) request.icon_index()}
                    << finalize
            );
        } catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "isActivity", std::to_string(is_activity),
                    "admin_name", request.login_info().admin_name()
            );
            error_message = "Error message stored on server.";
            return false;
        }

        if (!find_icons_document) {
            error_message =
                    "Icon index of " + std::to_string(request.icon_index()) + " was not found on the server.";
            return false;
        }

    } else { //category type message

#if defined(_RELEASE) || defined(LG_TESTING)
        //If this is defined during _DEBUG, will be unable to set default activities and categories.
        if(request.display_name() == "Unknown") {
            error_message = "Cannot modify 'Unknown' category.";
            return false;
        }
#endif

        if (request.color().size() != 7) {
            error_message = "Invalid color code " + request.color() + " passed.";
            return false;
        }

        if (request.color().front() != '#') {
            error_message = "Invalid color code " + request.color() + " passed.";
            return false;
        }

        for (size_t i = 1; i < request.color().size(); i++) {
            if (!std::isxdigit(request.color().at(1))) {
                error_message = "Invalid color code " + request.color() + " passed.";
                return false;
            }
        }
    }

    std::string upper_case_color_code;

    //convert color code to upper case
    for (const char c : request.color()) {
        upper_case_color_code += isalpha(int(c)) ? char(toupper(c)) : c;
    }

    bsoncxx::builder::stream::document activity_or_category_doc_builder;
    bsoncxx::builder::basic::array equality_array;
    std::string array_field_key_to_update;

    activity_or_category_doc_builder
        << activities_info_keys::SHARED_DISPLAY_NAME_KEY << request.display_name()
        << activities_info_keys::SHARED_ICON_DISPLAY_NAME_KEY << request.icon_display_name()
        << activities_info_keys::SHARED_STORED_NAME_KEY << stored_name;

    equality_array.append(
        document{}
            << "$eq" << open_array
                << "$$value.value_found" << false
            << close_array
        << finalize
    );

    equality_array.append(
        document{}
            << "$eq" << open_array
                << "$$this." + activities_info_keys::SHARED_STORED_NAME_KEY << stored_name
            << close_array
        << finalize
    );

    if (!request.delete_this()) {
        activity_or_category_doc_builder
                << activities_info_keys::SHARED_MIN_AGE_KEY << bsoncxx::types::b_int32{(int) request.min_age()};
    } else {
        activity_or_category_doc_builder
                << activities_info_keys::SHARED_MIN_AGE_KEY << bsoncxx::types::b_int32{server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1};
    }

    if (is_activity) {
        array_field_key_to_update = activities_info_keys::ACTIVITIES;
        activity_or_category_doc_builder
                << activities_info_keys::activities::CATEGORY_INDEX << bsoncxx::types::b_int32{(int) request.category_index()}
                << activities_info_keys::activities::ICON_INDEX << bsoncxx::types::b_int32{(int) request.icon_index()};

        equality_array.append(
            document{}
                << "$eq" << open_array
                    << "$$this." + activities_info_keys::activities::CATEGORY_INDEX << bsoncxx::types::b_int32{(int) request.category_index()}
                << close_array
            << finalize
        );
    } else {
        array_field_key_to_update = activities_info_keys::CATEGORIES;
        activity_or_category_doc_builder
                << activities_info_keys::categories::ORDER_NUMBER << bsoncxx::types::b_double{request.order_number()}
                << activities_info_keys::categories::COLOR << upper_case_color_code;
    }

    mongocxx::pipeline update_pipeline;

    const bsoncxx::document::value set_category_or_activity_doc =
            buildSetCategoryOrActivityDoc(
                    activity_or_category_doc_builder,
                    array_field_key_to_update,
                    equality_array.view()
            );

    update_pipeline.add_fields(set_category_or_activity_doc.view());

    bsoncxx::stdx::optional<mongocxx::result::update> update_activity_or_category_result;
    std::optional<std::string> update_activity_or_category_exception_string;
    try {
        bsoncxx::builder::stream::document find_doc;

        find_doc
                << "_id" << activities_info_keys::ID;

        if (is_activity) {
            find_doc
                << activities_info_keys::CATEGORIES + "." + std::to_string(request.category_index()) << open_document
                    << "$exists" << true
                << close_document;
        }

        update_activity_or_category_result = activities_info_collection.update_one(
                find_doc.view(),
               update_pipeline
        );
    } catch (const mongocxx::logic_error& e) {
        update_activity_or_category_exception_string = e.what();
    }

    if (!update_activity_or_category_result) { //error
        //NOTE: If the new array is the same as the previous array then modified_count will be 0, so
        // it is not a valid check of success.

        const std::string error_string = "Failed to set activity or category to document.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                update_activity_or_category_exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "failed_document", activity_or_category_doc_builder.view(),
                "isActivity", std::to_string(is_activity),
                "admin_name", request.login_info().admin_name()
        );

        return false;
    } else if (update_activity_or_category_result->matched_count() == 0) {
        error_message =
                "TestCategory index of " + std::to_string(request.category_index()) + " was not found on the server.";
        return false;
    }

    return true;
}

std::string convertCategoryActivityNameToStoredName(const std::string& original_name) {
    std::string stored_name;

    for(char c : original_name) {
        if(isalpha(c))
            stored_name.push_back((char)tolower(c));
        else if(std::isdigit(c))
            stored_name.push_back(c);
    }

    return stored_name;
}