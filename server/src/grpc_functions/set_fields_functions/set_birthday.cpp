//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <UserAccountStatusEnum.grpc.pb.h>

#include <handle_function_operation_exception.h>
#include <admin_functions_for_set_values.h>
#include <connection_pool_global_variable.h>
#include <store_mongoDB_error_and_exception.h>
#include <store_info_to_user_statistics.h>

#include "set_fields_functions.h"

#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"
#include "server_parameter_restrictions.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "update_single_other_user.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setBirthdayImplementation(
        const setfields::SetBirthdayRequest* request,
        setfields::SetBirthdayResponse* response
);

void setBirthday(
        const setfields::SetBirthdayRequest* request,
        setfields::SetBirthdayResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setBirthdayImplementation(request, response);
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

void setBirthdayImplementation(
        const setfields::SetBirthdayRequest* request,
        setfields::SetBirthdayResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    auto store_error_message = [&](
            bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
            const std::string& passed_error_message
    ) {
        response->set_error_string(passed_error_message);
        admin_info_doc_value = std::move(returned_admin_info_doc);
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
        response->set_return_status(basic_info_return_status);
        return;
    } else if (request->login_info().admin_info_used()) {
       if(!checkForUpdateUserPrivilege(response->mutable_error_string(), admin_info_doc_value)) {
           response->set_return_status(LG_ERROR);
           return;
       } else if(isInvalidOIDString(user_account_oid_str)) {
           response->set_return_status(INVALID_USER_OID);
           return;
       }
    }

    //verify birthday is valid
    const int birth_year = request->birth_year();
    const int birth_month = request->birth_month();
    const int birth_day_of_month = request->birth_day_of_month();

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    if (isInvalidBirthday(
            current_timestamp,
            birth_year,
            birth_month,
            birth_day_of_month)
    ) { //false means valid values in CheckedPassedValuesForValidBirthday
        if (request->login_info().admin_info_used()) { //admin
            response->set_return_status(ReturnStatus::LG_ERROR);
            response->set_error_string("Invalid birthday info passed.");
        } else { //user
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
            std::cout << "isInvalidBirthday() is true" << std::endl;
        }
        return;
    }

    tm birthday_tm = initializeTmByDate(
            birth_year,
            birth_month,
            birth_day_of_month
    );

    const int user_age = calculateAge(
            current_timestamp,
            birth_year,
            birthday_tm.tm_yday
    );

    if (user_age < server_parameter_restrictions::LOWEST_ALLOWED_AGE
        || server_parameter_restrictions::HIGHEST_ALLOWED_AGE < user_age
    ) { //if the age was out of allowed bounds
        std::cout << std::string("userAge invalid: " + std::to_string(user_age)) << std::endl;
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const bsoncxx::types::b_date mongodb_current_date{current_timestamp};

    bsoncxx::builder::stream::document merge_document;
    merge_document
            << user_account_keys::BIRTH_YEAR << bsoncxx::types::b_int32{birth_year}
            << user_account_keys::BIRTH_MONTH << bsoncxx::types::b_int32{birth_month}
            << user_account_keys::BIRTH_DAY_OF_MONTH << bsoncxx::types::b_int32{birth_day_of_month}
            << user_account_keys::BIRTH_DAY_OF_YEAR << bsoncxx::types::b_int32{birthday_tm.tm_yday}
            << user_account_keys::BIRTHDAY_TIMESTAMP << mongodb_current_date
            << user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED << buildUpdateSingleOtherUserProjectionDoc<true>(mongodb_current_date);

    //allowing id to be extracted so error messages will show it
    const bsoncxx::document::value projection_document = document{}
                << "_id" << 1
                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << finalize;

    ReturnStatus return_status = ReturnStatus::SUCCESS;

    if (!request->login_info().admin_info_used()) { //user request

        merge_document
            << user_account_keys::AGE << bsoncxx::types::b_int32{user_age};

        mongocxx::pipeline update_pipeline;

        std::shared_ptr<std::vector<bsoncxx::document::value>> append_to_and_statement_doc = std::make_shared<std::vector<bsoncxx::document::value>>();

        //NOTE: this check is in here because age range will need to be taken into account if the account has
        // reached active status (or suspended or banned)
        append_to_and_statement_doc->emplace_back(
            document{}
                << "$cond" << open_document

                    //if account requires more info, allow birthday to be set
                    << "if" << open_document
                        << "$eq" << open_array
                          << "$" + user_account_keys::STATUS << UserAccountStatus::STATUS_REQUIRES_MORE_INFO
                        << close_array
                    << close_document

                    << "then" << true
                    << "else" << false
                << close_document
            << finalize
        );

        const bsoncxx::document::value login_document = getLoginDocument<true>(
                login_token_str,
                installation_id,
                merge_document,
                current_timestamp,
                append_to_and_statement_doc
        );

        update_pipeline.replace_root(login_document.view());

        mongocxx::stdx::optional<bsoncxx::document::value> find_and_update_user_account;

        if (!runInitialLoginOperation(
                find_and_update_user_account,
                user_accounts_collection,
                bsoncxx::oid{user_account_oid_str},
                login_document,
                projection_document)
        ) {
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        return_status = checkForValidLoginToken(
                find_and_update_user_account,
                user_account_oid_str
        );

        if (return_status == ReturnStatus::SUCCESS
            || return_status == ReturnStatus::UNKNOWN //UNKNOWN means user_account_keys::STATUS==STATUS_REQUIRES_MORE_INFO
                ) {
            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_age(user_age);
            response->set_timestamp(current_timestamp.count());
        } else {
            response->set_return_status(return_status);
        }

    } else { //admin request

        auto error_func = [&response](const std::string& error_str) {
            response->set_return_status(LG_ERROR);
            response->set_error_string(error_str);
        };

        //userAge has already been checked to be within proper age bounds
        AgeRangeDataObject default_age_range = calculateAgeRangeFromUserAge(user_age);

        //recalculate age range if necessary
        merge_document
            << user_account_keys::AGE_RANGE << open_document
                << "$cond" << open_document

                    //if userAge was updated to a value less than 20, re-calculate age range
                    << "if" << open_document
                        << "$and" << open_array

                            //age has changed
                            << open_document
                                << "$ne" << open_array
                                    << user_age << "$" + user_account_keys::AGE
                                << close_array
                            << close_document

                            << open_document
                                << "$or" << open_array

                                    // new age < 20
                                    << (user_age < 20)

                                    // database age < 20
                                    << open_document
                                        << "$lt" << open_array
                                            << "$" + user_account_keys::AGE
                                            << 20
                                        << close_array
                                    << close_document
                                << close_array
                            << close_document

                        << close_array
                    << close_document

                    //update default age range
                    << "then" << open_document
                        << user_account_keys::age_range::MIN << default_age_range.min_age
                        << user_account_keys::age_range::MAX << default_age_range.max_age
                    << close_document

                    //leave age range as default
                    << "else" << "$" + user_account_keys::AGE_RANGE

                << close_document
            << close_document
            << user_account_keys::AGE << bsoncxx::types::b_int32{user_age};

        bsoncxx::stdx::optional<bsoncxx::document::value> update_doc;
        try {

            mongocxx::pipeline pipeline;

            pipeline.add_fields(merge_document.view());

            mongocxx::options::find_one_and_update opts;

            opts.projection(document{}
                    << user_account_keys::AGE_RANGE << 1
                << finalize
            );

            opts.return_document(mongocxx::options::return_document::k_after);

            //find and update user account document
            update_doc = user_accounts_collection.find_one_and_update(
                    document{}
                        << "_id" << bsoncxx::oid{user_account_oid_str}
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
                    "ObjectID_used", user_account_oid_str,
                    "Document_passed", merge_document.view()
            );

            error_func("Error exception thrown when setting user info.");
            return;
        }

        int min_age;
        int max_age;

        if (!update_doc) {
            error_func("Failed to update user info.\n'!update_doc' returned.");
        } else if (!extractMinMaxAgeRange(*update_doc, min_age, max_age)) { //success
            error_func("Failed to extract min and max age range from birthday update.");
        } else {
            response->set_return_status(SUCCESS);
            response->set_timestamp(current_timestamp.count());
            response->set_age(user_age);
            response->set_min_age_range(min_age);
            response->set_max_age_range(max_age);
        }
    }

    //Do not allow this to update if client attempted an update and UNKNOWN was returned.
    if(response->return_status() == SUCCESS
        && return_status != ReturnStatus::UNKNOWN) {
        auto push_update_doc = document{}
            << user_account_statistics_keys::BIRTH_INFO << open_document
                << user_account_statistics_keys::birth_info::YEAR << birth_year
                << user_account_statistics_keys::birth_info::MONTH << birth_month
                << user_account_statistics_keys::birth_info::DAY_OF_MONTH << birth_day_of_month
                << user_account_statistics_keys::birth_info::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
            << close_document
            << finalize;
        storeInfoToUserStatistics(
                mongo_cpp_client,
                accounts_db,
                bsoncxx::oid{user_account_oid_str},
                push_update_doc,
                current_timestamp
        );

    }

}
