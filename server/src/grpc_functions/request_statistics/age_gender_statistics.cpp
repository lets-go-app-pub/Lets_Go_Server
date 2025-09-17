//
// Created by jeremiah on 8/30/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <AdminLevelEnum.grpc.pb.h>
#include <UserAccountStatusEnum.grpc.pb.h>

#include "store_mongoDB_error_and_exception.h"
#include "admin_privileges_vector.h"
#include "connection_pool_global_variable.h"
#include "handle_function_operation_exception.h"

#include "utility_general_functions.h"
#include "request_statistics.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "user_account_keys.h"
#include "request_statistics_values.h"
#include "general_values.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void runMatchingAgeGenderStatisticsImplementation(
        const request_statistics::AgeGenderStatisticsRequest* request,
        request_statistics::AgeGenderStatisticsResponse* response
);

void runMatchingAgeGenderStatistics(
        const request_statistics::AgeGenderStatisticsRequest* request,
        request_statistics::AgeGenderStatisticsResponse* response
) {
    handleFunctionOperationException(
            [&] {
                runMatchingAgeGenderStatisticsImplementation(request, response);
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

void runMatchingAgeGenderStatisticsImplementation(
        const request_statistics::AgeGenderStatisticsRequest* request,
        request_statistics::AgeGenderStatisticsResponse* response
) {

    std::chrono::milliseconds last_time_extracted_age_gender;

    {
        bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
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
            response->set_success(false);
            response->set_error_msg(
                    "ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
            return;
        }

        const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
        AdminLevelEnum admin_level;

        auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
        if (admin_privilege_element
            && admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
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

        if (!admin_privileges[admin_level].view_age_gender_statistics()) {
            response->set_success(false);
            response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                    " does not have 'view_matching_activity_statistics' access.");
            return;
        }

        auto last_time_extracted_age_gender_element = admin_info_doc_view[admin_account_key::LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS];
        if (last_time_extracted_age_gender_element
            && last_time_extracted_age_gender_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
            last_time_extracted_age_gender = last_time_extracted_age_gender_element.get_date().value;
        } else { //if element does not exist or is not type date
            logElementError(
                    __LINE__, __FILE__,
                    last_time_extracted_age_gender_element, admin_info_doc_view,
                    bsoncxx::type::k_date, admin_account_key::PRIVILEGE_LEVEL,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
            );

            response->set_success(false);
            response->set_error_msg("Error stored on server.");
            return;
        }
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    {
        const std::chrono::milliseconds cool_down_remaining = current_timestamp - last_time_extracted_age_gender;

        if (cool_down_remaining < request_statistics_values::COOL_DOWN_BETWEEN_REQUESTING_AGE_GENDER_STATISTICS) {

            const std::string cool_down_str = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                    request_statistics_values::COOL_DOWN_BETWEEN_REQUESTING_AGE_GENDER_STATISTICS -
                    cool_down_remaining).count() + 1);
            response->set_success(false);
            response->set_error_msg("Please wait " + cool_down_str + " seconds before requesting info.");
            return;
        }
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection admin_accounts_collection = accounts_db[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::stdx::optional<mongocxx::result::update> update_statistics_ran_time;
    try {
        update_statistics_ran_time = admin_accounts_collection.update_one(
            document{}
                << admin_account_key::NAME << request->login_info().admin_name()
                << admin_account_key::PASSWORD << request->login_info().admin_password()
            << finalize,
            document{}
                << "$set" << open_document
                    << admin_account_key::LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS << bsoncxx::types::b_date{
                    current_timestamp}
                << close_document
            << finalize
        );
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server.");
        return;
    }

    if (!update_statistics_ran_time
        || update_statistics_ran_time->matched_count() < 1
        || update_statistics_ran_time->modified_count() < 1
    ) {
        response->set_success(false);
        response->set_error_msg("Failed to updated admin account statistics last ran time.");
        return;
    }

    mongocxx::pipeline pipeline;

    pipeline.project(
        document{}
            << user_account_keys::STATUS << 1
            << user_account_keys::ACCOUNT_TYPE << 1
            << user_account_keys::AGE << 1
            << user_account_keys::GENDER << 1
        << finalize
    );

    pipeline.match(
            document{}
                    << user_account_keys::STATUS << UserAccountStatus::STATUS_ACTIVE
                    << user_account_keys::ACCOUNT_TYPE << UserAccountType::USER_ACCOUNT_TYPE
            << finalize
    );

    static const std::string PIPELINE_COMPOUND_INDEX_KEY = "comp_ag_index";
    static const std::string OTHER_GENDER_PLACEHOLDER = "other_gender";

    pipeline.add_fields(
        document{}
            << PIPELINE_COMPOUND_INDEX_KEY << open_document
                << "$concat" << open_array
                    << open_document
                        << "$toString" << "$" + user_account_keys::AGE
                    << close_document
                    << "_"
                    << open_document
                        << "$cond" << open_document
                            << "if" << open_document
                                << "$or" << open_array
                                    << open_document
                                        << "$eq" << open_array
                                            << "$" + user_account_keys::GENDER
                                            << general_values::MALE_GENDER_VALUE
                                        << close_array
                                    << close_document
                                    << open_document
                                        << "$eq" << open_array
                                            << "$" + user_account_keys::GENDER
                                            << general_values::FEMALE_GENDER_VALUE
                                        << close_array
                                    << close_document
                                << close_array
                            << close_document
                            << "then" << "$" + user_account_keys::GENDER
                            << "else" << OTHER_GENDER_PLACEHOLDER
                        << close_document
                    << close_document
                << close_array
            << close_document
        << finalize
    );

    static const std::string NUMBER_DOCS_PIPELINE_KEY = "number_docs";

    pipeline.group(
        document{}
            << "_id" << "$" + PIPELINE_COMPOUND_INDEX_KEY
            << user_account_keys::GENDER << open_document
                << "$first" << open_document
                    << "$cond" << open_document
                        << "if" << open_document
                            << "$or" << open_array
                                << open_document
                                    << "$eq" << open_array
                                        << "$" + user_account_keys::GENDER
                                        << general_values::MALE_GENDER_VALUE
                                    << close_array
                                << close_document
                                << open_document
                                    << "$eq" << open_array
                                        << "$" + user_account_keys::GENDER
                                        << general_values::FEMALE_GENDER_VALUE
                                    << close_array
                                << close_document
                            << close_array
                        << close_document
                        << "then" << "$" + user_account_keys::GENDER
                        << "else" << OTHER_GENDER_PLACEHOLDER
                    << close_document
                << close_document
            << close_document
            << user_account_keys::AGE << open_document
                << "$first" << "$" + user_account_keys::AGE
            << close_document
            << NUMBER_DOCS_PIPELINE_KEY << open_document
                << "$sum" << bsoncxx::types::b_int64{1}
            << close_document
        << finalize
    );

    static const std::string NUMBER_MALE_GENDERS_PIPELINE_KEY = "num_male_genders";
    static const std::string NUMBER_FEMALE_GENDERS_PIPELINE_KEY = "num_female_genders";
    static const std::string NUMBER_OTHER_GENDERS_PIPELINE_KEY = "num_other_genders";

    pipeline.group(
        document{}
            << "_id" << "$" + user_account_keys::AGE
            << NUMBER_MALE_GENDERS_PIPELINE_KEY << open_document
                << "$sum" << open_document
                    << "$cond" << open_document

                        << "if" << open_document
                            << "$eq" << open_array
                                << "$" + user_account_keys::GENDER
                                << general_values::MALE_GENDER_VALUE
                            << close_array
                        << close_document
                        << "then" << "$" + NUMBER_DOCS_PIPELINE_KEY
                        << "else" << bsoncxx::types::b_int64{0}

                    << close_document
                << close_document
            << close_document
            << NUMBER_FEMALE_GENDERS_PIPELINE_KEY << open_document
                << "$sum" << open_document
                    << "$cond" << open_document

                        << "if" << open_document
                            << "$eq" << open_array
                                << "$" + user_account_keys::GENDER
                                << general_values::FEMALE_GENDER_VALUE
                            << close_array
                        << close_document
                        << "then" << "$" + NUMBER_DOCS_PIPELINE_KEY
                        << "else" << bsoncxx::types::b_int64{0}

                    << close_document
                << close_document
            << close_document
            << NUMBER_OTHER_GENDERS_PIPELINE_KEY << open_document
                << "$sum" << open_document
                    << "$cond" << open_document

                        << "if" << open_document
                            << "$eq" << open_array
                                << "$" + user_account_keys::GENDER
                                << OTHER_GENDER_PLACEHOLDER
                            << close_array
                        << close_document
                        << "then" << "$" + NUMBER_DOCS_PIPELINE_KEY
                        << "else" << bsoncxx::types::b_int64{0}

                    << close_document
                << close_document
            << close_document
        << finalize
    );

    mongocxx::stdx::optional<mongocxx::cursor> find_age_gender_cursor;
    try {
        find_age_gender_cursor = user_accounts_collection.aggregate(pipeline);
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server.");
        return;
    }

    if (!find_age_gender_cursor) {
        const std::string error_string = "Cursor for user_accounts_collection came back un-set when no exception was"
                                         " thrown. This should never happen";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server.");
        return;
    }

    for (auto& document : *find_age_gender_cursor) {

        int age;
        long number_gender_male;
        long number_gender_female;
        long number_gender_other;

        try {

            age = extractFromBsoncxx_k_int32(
                    document,
                    "_id"
            );

            number_gender_male = extractFromBsoncxx_k_int64(
                    document,
                    NUMBER_MALE_GENDERS_PIPELINE_KEY
            );

            number_gender_female = extractFromBsoncxx_k_int64(
                    document,
                    NUMBER_FEMALE_GENDERS_PIPELINE_KEY
            );

            number_gender_other = extractFromBsoncxx_k_int64(
                    document,
                    NUMBER_OTHER_GENDERS_PIPELINE_KEY
            );

        } catch (const ErrorExtractingFromBsoncxx& e) {
            //Error already stored.
            continue;
        }

        request_statistics::NumberOfTimesGenderSelectedAtAge* added_element = response->add_gender_selected_at_age_list();

        added_element->set_age(age);
        added_element->set_gender_male(number_gender_male);
        added_element->set_gender_female(number_gender_female);
        added_element->set_gender_other(number_gender_other);
    }

    response->set_success(true);
}