//proto file is LoginSupportFunctions.proto

#include "mongocxx/uri.hpp"
#include "mongocxx/client.hpp"

#include <UserAccountStatusEnum.grpc.pb.h>

#include <bsoncxx/builder/stream/document.hpp>
#include <utility_testing_functions.h>
#include <handle_function_operation_exception.h>
#include <store_info_to_user_statistics.h>

#include "login_support_functions.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "get_user_info_timestamps.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"
#include "server_parameter_restrictions.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void findNeededVerificationInfoImplementation(
        const loginsupport::NeededVeriInfoRequest* request,
        loginsupport::NeededVeriInfoResponse* response
);

void findNeededVerificationInfo(
        const loginsupport::NeededVeriInfoRequest* request,
        loginsupport::NeededVeriInfoResponse* response
) {

    handleFunctionOperationException(
            [&] {
                findNeededVerificationInfoImplementation(request, response);
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

void findNeededVerificationInfoImplementation(
        const loginsupport::NeededVeriInfoRequest* request,
        loginsupport::NeededVeriInfoResponse* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    {
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
    }

    const double& longitude = request->client_longitude();
    const double& latitude = request->client_latitude();
    if (isInvalidLocation(longitude, latitude)) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        response->set_server_timestamp(-2L);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    //setting error as default
    response->set_return_status(ReturnStatus::UNKNOWN);
    response->set_server_timestamp(-1L);

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid user_account_oid{user_account_oid_str};

    const bsoncxx::document::value merge_document = document{} << finalize;

    //NOTE: User pictures key is projected to extract thumbnail.
    const bsoncxx::document::value projection_document = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << user_account_keys::STATUS << 1
            << user_account_keys::AGE << 1
            << user_account_keys::PICTURES << 1
            << user_account_keys::BIRTHDAY_TIMESTAMP << 1
            << user_account_keys::EMAIL_TIMESTAMP << 1
            << user_account_keys::GENDER_TIMESTAMP << 1
            << user_account_keys::FIRST_NAME_TIMESTAMP << 1
            << user_account_keys::CATEGORIES_TIMESTAMP << 1
            << user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION << 1
            << user_account_keys::INACTIVE_MESSAGE << 1
            << user_account_keys::INACTIVE_END_TIME << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<true>(
            login_token_str,
            installation_id,
            merge_document,
            current_timestamp
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account;

    mongocxx::client_session::with_transaction_cb transaction_callback = [&](
            mongocxx::client_session* callback_session) {

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

        const ReturnStatus return_status = checkForValidLoginToken(find_user_account, user_account_oid_str);

        if (return_status != ReturnStatus::SUCCESS) {
            response->set_return_status(return_status);
        }
        else {

            try {

                const bsoncxx::document::view user_account_doc_view = *find_user_account;
                const auto account_active_value = UserAccountStatus(
                        extractFromBsoncxx_k_int32(
                                user_account_doc_view,
                                user_account_keys::STATUS
                        )
                );

                switch (account_active_value) {
                    case STATUS_ACTIVE:
                    case STATUS_REQUIRES_MORE_INFO: {
                        const bool need_more_info = getUserInfoTimestamps(
                                user_account_doc_view,
                                response->mutable_pre_login_timestamps(),
                                response->mutable_picture_timestamps()
                        );

                        //extract age to create default age range
                        const int user_age = extractFromBsoncxx_k_int32(
                                user_account_doc_view,
                                user_account_keys::AGE
                        );

                        if (need_more_info) {
                            response->set_access_status(NEEDS_MORE_INFO);
                            response->set_return_status(ReturnStatus::SUCCESS);
                            std::cout << "Return Status In Func: " << convertReturnStatusToString(
                                    response->return_status()) << '\n';
                        }
                        else if (account_active_value ==
                                 UserAccountStatus::STATUS_REQUIRES_MORE_INFO) { //if account has all mandatory info up to date

                            const AgeRangeDataObject default_age_range = calculateAgeRangeFromUserAge(user_age);

                            if (default_age_range.min_age == -1 || default_age_range.max_age == -1)
                            { //error generating default_age_range

                                const std::string err_string = std::string("error verified age was either below 13 or over ")
                                        .append(std::to_string(server_parameter_restrictions::HIGHEST_ALLOWED_AGE));

                                storeMongoDBErrorAndException(
                                        __LINE__, __FILE__,
                                        std::optional<std::string>(), err_string,
                                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                        "age_value", std::to_string(user_age),
                                        "user_account_doc", user_account_doc_view);

                                //attempt to fix the situation
                                bsoncxx::stdx::optional<mongocxx::result::update> update_account_status;
                                try {

                                    //update account so age and birthday are no longer be valid
                                    update_account_status = user_accounts_collection.update_one(
                                            *callback_session,
                                            document{}
                                                    << "_id" << user_account_oid
                                                    << finalize,
                                            document{}
                                                    << "$set" << open_document
                                                    << user_account_keys::BIRTH_YEAR << -1
                                                    << user_account_keys::BIRTH_MONTH << -1
                                                    << user_account_keys::BIRTH_DAY_OF_MONTH << -1
                                                    << user_account_keys::BIRTH_DAY_OF_YEAR << -1
                                                    << user_account_keys::AGE << -1
                                                    << user_account_keys::BIRTHDAY_TIMESTAMP << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
                                                    << close_document
                                                    << finalize
                                    );

                                }
                                catch (const mongocxx::logic_error& e) {
                                    storeMongoDBErrorAndException(
                                            __LINE__, __FILE__,
                                            std::optional<std::string>(), e.what(),
                                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                            "document", user_account_doc_view);

                                    response->set_return_status(ReturnStatus::LG_ERROR);
                                    return;
                                }

                                if (update_account_status) {
                                    response->mutable_pre_login_timestamps()->set_birthday_timestamp(-1L);
                                    response->set_access_status(NEEDS_MORE_INFO);
                                    response->set_return_status(ReturnStatus::SUCCESS);
                                    return;
                                } else {
                                    const std::string error_string = "Failed to update user account document.";
                                    storeMongoDBErrorAndException(
                                            __LINE__, __FILE__,
                                            std::optional<std::string>(), error_string,
                                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                            "document", user_account_doc_view
                                    );

                                    response->set_return_status(ReturnStatus::LG_ERROR);
                                    return;
                                }
                            }

                            bsoncxx::stdx::optional<mongocxx::result::update> update_account_status;
                            try {

                                //set status to active for user account document
                                update_account_status = user_accounts_collection.update_one(
                                    *callback_session,
                                    document{}
                                        << "_id" << user_account_oid
                                        << finalize,
                                    document{}
                                        << "$set" << open_document
                                            << user_account_keys::STATUS << UserAccountStatus::STATUS_ACTIVE
                                            << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
                                            << user_account_keys::MATCHING_ACTIVATED << true
                                            << user_account_keys::AGE_RANGE << open_document
                                                << user_account_keys::age_range::MIN << default_age_range.min_age
                                                << user_account_keys::age_range::MAX << default_age_range.max_age
                                            << close_document
                                            << user_account_keys::GENDERS_RANGE << open_array
                                                << general_values::MALE_GENDER_VALUE
                                                << general_values::FEMALE_GENDER_VALUE
                                            << close_array
                                                << user_account_keys::LOCATION + ".coordinates" << open_array
                                                    << longitude
                                                    << latitude
                                                << close_array
                                            << user_account_keys::MAX_DISTANCE << server_parameter_restrictions::DEFAULT_MAX_DISTANCE
                                        << close_document
                                    << finalize
                                );
                            }
                            catch (const mongocxx::logic_error& e) {
                                const std::string error_string = std::string("Failed to update user account fields failed inside findNeededVerificationInfo.")
                                        .append("\ndefaultUserMinAgeRange: ")
                                        .append(std::to_string(default_age_range.min_age))
                                        .append("\ndefaultUserMaxAgeRange: ")
                                        .append(std::to_string(default_age_range.max_age))
                                        .append("\nmaxDistance: ")
                                        .append(std::to_string(server_parameter_restrictions::DEFAULT_MAX_DISTANCE));

                                storeMongoDBErrorAndException(
                                        __LINE__, __FILE__,
                                        std::optional<std::string>(), error_string,
                                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                        "keyUsed", user_account_keys::STATUS,
                                        "document", user_account_doc_view
                                );

                                response->set_return_status(ReturnStatus::LG_ERROR);
                                return;
                            }

                            if (!update_account_status) {
                                const std::string error_string = std::string("Failed to update user account fields failed inside findNeededVerificationInfo.")
                                        .append("\ndefaultUserMinAgeRange: ")
                                        .append(std::to_string(default_age_range.min_age))
                                        .append("\ndefaultUserMaxAgeRange: ")
                                        .append(std::to_string(default_age_range.max_age))
                                        .append("\nmaxDistance: ")
                                        .append(std::to_string(server_parameter_restrictions::DEFAULT_MAX_DISTANCE));

                                storeMongoDBErrorAndException(
                                        __LINE__, __FILE__,
                                        std::optional<std::string>(), error_string,
                                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                        "keyUsed", user_account_keys::STATUS,
                                        "document", user_account_doc_view
                                );

                                response->set_return_status(ReturnStatus::LG_ERROR);
                                return;
                            }

                            const bsoncxx::document::value push_update_doc = document{}
                                        << user_account_statistics_keys::LOCATIONS << open_document
                                            << user_account_statistics_keys::locations::LOCATION << open_array
                                                << longitude
                                                << latitude
                                            << close_array
                                            << user_account_statistics_keys::locations::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
                                        << close_document
                                    << finalize;

                            storeInfoToUserStatistics(
                                    mongo_cpp_client,
                                    accounts_db,
                                    user_account_oid,
                                    push_update_doc,
                                    current_timestamp
                            );

                            //Client expects min age range, gender range and max distance to be passed back.
                            auto postLoginInfo = response->mutable_post_login_info();
                            postLoginInfo->set_user_bio("~");
                            postLoginInfo->set_user_city("~");
                            postLoginInfo->add_gender_range(general_values::MALE_GENDER_VALUE);
                            postLoginInfo->add_gender_range(general_values::FEMALE_GENDER_VALUE);
                            postLoginInfo->set_min_age(default_age_range.min_age);
                            postLoginInfo->set_max_age(default_age_range.max_age);
                            postLoginInfo->set_max_distance(server_parameter_restrictions::DEFAULT_MAX_DISTANCE);
                            response->set_server_timestamp(current_timestamp.count() + 1);

                            response->set_return_status(ReturnStatus::SUCCESS);
                            response->set_access_status(ACCESS_GRANTED);
                        }
                        else { // UserAccountStatus::STATUS_ACTIVE

                            //NOTE: Could technically happen if a user calls the same function twice before the first
                            // one finishes.

                            const std::string error_string = "findNeededVerificationInfo() was called when user_account_keys::STATUS was STATUS_ACTIVE.";
                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    std::optional<std::string>(), error_string,
                                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                    "user_OID", user_account_oid_str
                            );

                            std::optional<std::string> user_exception_string;
                            bsoncxx::stdx::optional<bsoncxx::document::value> user_account_values;
                            try {

                                mongocxx::options::find opts;

                                opts.projection(
                                        document{}
                                                << user_account_keys::AGE_RANGE << 1
                                                << user_account_keys::MAX_DISTANCE << 1
                                                << user_account_keys::GENDERS_RANGE << 1
                                        << finalize
                                );

                                //find the user account document
                                user_account_values = user_accounts_collection.find_one(
                                        *callback_session,
                                        document{}
                                                << "_id" << user_account_oid
                                        << finalize,
                                        opts
                                );

                            } catch (const mongocxx::logic_error& e) {
                                user_exception_string = std::string(e.what());
                            }

                            //if user account document was not found when it was just found a moment ago.
                            if(!user_account_values) {
                                const std::string err_string = "findNeededVerificationInfo() was called when user_account_keys::STATUS was STATUS_ACTIVE.";
                                storeMongoDBErrorAndException(
                                        __LINE__, __FILE__,
                                        user_exception_string, err_string,
                                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                        "user_OID", user_account_oid_str,
                                        "user_account_doc_view", user_account_doc_view
                                );
                            }

                            const bsoncxx::document::view user_account_values_view = user_account_values->view();

                            int min_age_range = 0;
                            int max_age_range = 0;

                            if (!extractMinMaxAgeRange(
                                    user_account_values_view,
                                    min_age_range,
                                    max_age_range)
                                    ) {
                                response->Clear();
                                //Error already stored.
                                response->set_return_status(ReturnStatus::LG_ERROR);
                                return;
                            }

                            const int max_distance = extractFromBsoncxx_k_int32(
                                    user_account_values_view,
                                    user_account_keys::MAX_DISTANCE
                            );

                            const bsoncxx::array::view gender_range = extractFromBsoncxx_k_array(
                                    user_account_values_view,
                                    user_account_keys::GENDERS_RANGE
                            );

                            auto post_login_info = response->mutable_post_login_info();

                            for(const auto& gender : gender_range) {
                                if(gender.type() == bsoncxx::type::k_utf8) {
                                    post_login_info->add_gender_range(gender.get_string().value.to_string());
                                } else {
                                    const std::string err_string = "Invalid UserAccountStatus.";
                                    storeMongoDBErrorAndException(
                                            __LINE__, __FILE__,
                                            std::optional<std::string>(), err_string,
                                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                            "user_account_values_view", user_account_values_view,
                                            "login_document", login_document,
                                            "account_active_value", std::to_string(account_active_value)
                                    );

                                    response->set_return_status(ReturnStatus::LG_ERROR);
                                    return;
                                }
                            }

                            //Client expects min age range, max age range, gender range and max distance to be passed back.
                            post_login_info->set_user_bio("~");
                            post_login_info->set_user_city("~");
                            post_login_info->set_min_age(min_age_range);
                            post_login_info->set_max_age(max_age_range);
                            post_login_info->set_max_distance(max_distance);
                            response->set_server_timestamp(current_timestamp.count() + 1);

                            response->set_return_status(ReturnStatus::SUCCESS);
                            response->set_access_status(ACCESS_GRANTED);
                        }

                        break;
                    }
                    case STATUS_SUSPENDED:
                    case STATUS_BANNED:
                    case UserAccountStatus_INT_MIN_SENTINEL_DO_NOT_USE_:
                    case UserAccountStatus_INT_MAX_SENTINEL_DO_NOT_USE_: {
                        //NOTE: It should be impossible to get to this point because the login document requires
                        // a valid UserAccountStatus to be set inside the account. Also, if user account is suspended
                        // or banned it will be returned above.

                        const std::string error_string = "Invalid UserAccountStatus.";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "database", database_names::ACCOUNTS_DATABASE_NAME,
                                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                "document", user_account_doc_view,
                                "login_document", login_document,
                                "account_active_value", std::to_string(account_active_value)
                        );

                        response->set_return_status(ReturnStatus::LG_ERROR);
                        break;
                    }
                }

            } catch (const ErrorExtractingFromBsoncxx& e) {
                //NOTE: Error already stored.
                response->Clear();
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }
        }
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    try {
        session.with_transaction(transaction_callback);
    } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
        std::cout << "Exception calling findNeededVerificationInfoImplementation() transaction.\n" << e.what() << '\n';
#endif

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional <std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_OID", user_account_oid_str
        );

        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

}


