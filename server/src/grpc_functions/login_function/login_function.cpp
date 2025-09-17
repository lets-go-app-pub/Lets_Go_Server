
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/result/insert_one.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/collection.hpp>

#include <UserAccountStatusEnum.grpc.pb.h>

#include <get_user_info_timestamps.h>
#include <global_bsoncxx_docs.h>
#include <sstream>

#include <store_info_to_user_statistics.h>
#include <server_values.h>
#include <accepted_mime_types.h>
#include <time_frame_struct.h>
#include <AccountCategoryEnum.pb.h>
#include <save_activity_time_frames.h>
#include <save_category_time_frames.h>
#include <set_fields_helper_functions/set_fields_helper_functions.h>
#include <grpc_values.h>
#include <random>

#include "login_function.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "request_helper_functions.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"
#include "pending_account_keys.h"
#include "info_stored_after_deletion_keys.h"
#include "server_parameter_restrictions.h"
#include "matching_algorithm.h"
#include "chat_stream_container.h"
#include "chat_room_values.h"
#include "android_specific_values.h"
#include "general_values.h"
#include "chat_room_header_keys.h"
#include "pre_login_checkers_keys.h"
#include "event_admin_values.h"

#ifdef _ACCEPT_SEND_SMS_COMMANDS
#include <Python.h>
#include <python_send_sms_module.h>

#include "thread_pool_global_variable.h"
#include "python_handle_gil_state.h"
#endif

//proto file is PhoneLogin.proto

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

inline const std::string SMS_VERIFICATION_PHONE_NUMBER = "+14806902095";

//handles the creation (or modification) of pending account and sends SMS
bool handleVerification(
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        const std::string& phone_number,
        const std::string& installation_id,
        const AccountLoginType& account_type,
        const std::string& account_id,
        std::string& return_val_verification_code,
        const bsoncxx::oid* user_account_oid
);

//send an SMS with the verification code and phone#, returns false if failed and saves the error to errorMessage
void sendSMS(
        const std::string& verification_code,
        const std::string& phone_number,
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::oid* user_account_oid
);

//checks account info, returns timestamps for each account info item
//also returns all simple info to the server
//pictures timestamps will be returned only
//for icons an array of index number that need updated will be returned
bool checkOutdatedAccountInfoAndSetToResponse(
        mongocxx::database& accounts_db,
        const std::string& phone_number,
        mongocxx::client& mongo_cpp_client,
        mongocxx::collection& user_account_collection,
        const bsoncxx::document::view& query_for_user_account_doc_view,
        const loginfunction::LoginRequest* request,
        const bsoncxx::document::view& user_account_view,
        loginfunction::LoginResponse* response,
        const std::chrono::milliseconds& current_timestamp
);

std::string loginFunctionMongoDbImplementation(
        const loginfunction::LoginRequest* request,
        loginfunction::LoginResponse* response,
        const std::string& caller_uri
);

std::string handleSmsCoolDownAndVerification(
        loginfunction::LoginResponse* response,
        const AccountLoginType& account_type,
        const std::string& installation_id,
        const std::string& phone_number,
        const std::string& passed_account_id,
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        const std::chrono::milliseconds& current_timestamp,
        std::string& return_val_verification_code,
        const bsoncxx::document::view& sms_doc_view,
        const std::string& cool_down_on_sms_return_message_key,
        const bsoncxx::oid* user_account_oid = nullptr
);

//This function will check for any activity index that are invalid OR any activities that the user is not the correct
// age for. It will then update them and save the newly updated categories to the response.
//This function should almost never be actually called, so it was left a bit 'heavy'.
bool updateInvalidActivities(
        mongocxx::client& mongo_cpp_client,
        mongocxx::collection& user_account_collection,
        const bsoncxx::document::view& find_user_account_doc,
        const bsoncxx::document::view& user_account_doc_view,
        loginfunction::LoginResponse* response,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::oid& user_account_oid,
        int current_user_age
);

void handleOperationExceptionPendingAccount(
        const mongocxx::operation_exception& e,
        mongocxx::collection& phone_pending_collection,
        const bsoncxx::document::view& update_pending_accounts_doc,
        const std::string& phone_number,
        const std::string& indexing_string,
        const std::string& installation_id,
        bool upsert,
        bsoncxx::stdx::optional<mongocxx::result::update>& update_pending_account_success,
        std::optional<std::string>& update_pending_account_exception_string
);

inline std::string getPendingAccountIndex(
        const AccountLoginType& account_type,
        const std::string& phone_number,
        const std::string& passed_account_id
        ) {
    std::string account_id_to_store;

    if(account_type == GOOGLE_ACCOUNT || account_type == FACEBOOK_ACCOUNT) { //google or facebook account
        account_id_to_store = passed_account_id;
    } else { //phone type account
        account_id_to_store = phone_number;
    }

    return account_id_to_store;
}

inline bsoncxx::document::value getPendingAccountFilter(
        const std::string& phone_number,
        const std::string& indexing_string,
        const std::string& installation_id
        ) {
    return document{}
        << "$or" << open_array
            << open_document
                << pending_account_keys::PHONE_NUMBER << phone_number
            << close_document
            << open_document
                << pending_account_keys::INDEXING << indexing_string
            << close_document
            << open_document
                << pending_account_keys::ID << installation_id
            << close_document
        << close_array
    << finalize;
}

std::string loginFunctionMongoDb(
        const loginfunction::LoginRequest* request,
        loginfunction::LoginResponse* response,
        const std::string& caller_uri
) {
    std::string return_string;

    handleFunctionOperationException(
            [&] {
                return_string = loginFunctionMongoDbImplementation(request, response, caller_uri);
            },
            [&] {
                response->set_return_status(LoginValuesToReturnToClient_LoginAccountStatus_DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(LoginValuesToReturnToClient_LoginAccountStatus_LG_ERROR);
            },
            __LINE__, __FILE__, request);

    //Set the timestamp no matter what happens, get a new timestamp to ignore the time the function took to run.
    //This is set immediately before the return because it is used by the client to calculate time until the
    // next login occurs.
    response->set_server_timestamp(getCurrentTimestamp().count() + 1);

    return return_string;
}

std::string loginFunctionMongoDbImplementation(
        const loginfunction::LoginRequest* request,
        loginfunction::LoginResponse* response,
        const std::string& caller_uri
) {

    if (isInvalidLetsGoAndroidVersion(request->lets_go_version())) { //check if meets minimum version requirement
        response->set_return_status(LoginValuesToReturnToClient::OUTDATED_VERSION);
        return "";
    }

    const AccountLoginType& account_type = request->account_type();

    //check for valid type of account
    if (!AccountLoginType_IsValid(account_type)
        || account_type == AccountLoginType::LOGIN_TYPE_VALUE_NOT_SET) {
        response->set_return_status(LoginValuesToReturnToClient::INVALID_ACCOUNT_TYPE);
        return "";
    }

    const std::string& installation_id = request->installation_id(); //this is verified below

    if (isInvalidUUID(installation_id)) { //check for valid installationId
        response->set_return_status(LoginValuesToReturnToClient::INVALID_INSTALLATION_ID);
        return "";
    }

    //NOTE: the phone number can be updated later, do not make it const or an alias
    //NOTE: the phone number will be checked ideally, so it should either be set to empty or a valid phone number (no partials)
    std::string phone_number;
    std::string account_id;

    if (account_type == AccountLoginType::PHONE_ACCOUNT) {
        //make sure to update the phone number inside this condition, it will be checked for empty string
        // later
        phone_number = request->phone_number();

        if(!isValidPhoneNumber(phone_number)) { //check if phone number is valid

            //this is more for people that are calling these functions from something separate from android
            //because android will also have a filter before sending numbers in
            response->set_return_status(LoginValuesToReturnToClient::INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);

            return "";
        }
    } else { //facebook or google account type
        //NOTE: will check if phone number is valid below if the verified account does not exist, however if existing google or facebook account
        //the user will be able to log in with just the accountID and extract the phone number

        account_id = errorCheckAccountIDAndGenerateUniqueAccountID(account_type, request->account_id());

        if (account_id == "~") {
            response->set_return_status(LoginValuesToReturnToClient::INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);

            return "";
        }

        if(isValidPhoneNumber(request->phone_number())) { //check if phone number is valid
            phone_number = request->phone_number();
        }
        //else {} //ok if invalid phone number, just the account id is enough if account already exists
    }

    std::string device_name;
    if(request->device_name().size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES) {
        device_name = "ERROR: too long " + std::to_string(request->device_name().size());
    } else {
        device_name = request->device_name();
    }

    //setting values so default gRPC values don't get returned
    response->set_return_status(LoginValuesToReturnToClient::VALUE_NOT_SET);
    response->set_login_token("~");
    response->set_sms_cool_down(-1);
    response->set_access_status(AccessStatus::STATUS_NOT_SET);
    response->set_birthday_not_needed(true);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_account_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::string return_verification_code = "~";

    mongocxx::pipeline update_login_info;

    const auto age_calculator_document = buildDocumentToCalculateAge(current_timestamp);

    update_login_info.add_fields(
            document{}
                    << user_account_keys::AGE << age_calculator_document.view()
            << finalize
    );

    //requires LOGGED_IN_RETURN_MESSAGE to be projected out
    update_login_info.replace_root(
            getLoginFunctionDocument(
                    account_id,
                    installation_id,
                    current_timestamp
            )
    );

    std::string search_field_key;
    std::string search_field_value;

    //checked above for validity
    if(!phone_number.empty()) { //search by the phone number if possible
        search_field_key = user_account_keys::PHONE_NUMBER;
        search_field_value = phone_number;
    } else { //search by account id if no phone number available
        search_field_key = user_account_keys::ACCOUNT_ID_LIST;
        search_field_value = account_id;
    }

    mongocxx::options::find_one_and_update find_user_account_options;
    find_user_account_options.return_document(mongocxx::options::return_document::k_after);

    //Projecting in or out both have a large number of fields, decided to project in for extendability
    // otherwise this list needs to be added to whenever a new field is added to USER_ACCOUNTS_COLLECTION_NAME.
    find_user_account_options.projection(
            document{}
                    << user_account_keys::STATUS << 1
                    << user_account_keys::INACTIVE_MESSAGE << 1
                    << user_account_keys::INACTIVE_END_TIME << 1
                    << user_account_keys::ACCOUNT_ID_LIST << 1
                    << user_account_keys::SUBSCRIPTION_STATUS << 1
                    << user_account_keys::SUBSCRIPTION_EXPIRATION_TIME << 1
                    << user_account_keys::PHONE_NUMBER << 1
                    << user_account_keys::FIRST_NAME << 1
                    << user_account_keys::BIO << 1
                    << user_account_keys::CITY << 1
                    << user_account_keys::GENDER << 1
                    << user_account_keys::BIRTH_YEAR << 1
                    << user_account_keys::BIRTH_MONTH << 1
                    << user_account_keys::BIRTH_DAY_OF_MONTH << 1
                    << user_account_keys::BIRTH_DAY_OF_YEAR << 1
                    << user_account_keys::AGE << 1
                    << user_account_keys::EMAIL_ADDRESS << 1
                    << user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION << 1
                    << user_account_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL << 1
                    << user_account_keys::PICTURES << 1
                    << user_account_keys::SEARCH_BY_OPTIONS << 1
                    << user_account_keys::CATEGORIES << 1
                    << user_account_keys::AGE_RANGE << 1
                    << user_account_keys::GENDERS_RANGE << 1
                    << user_account_keys::MAX_DISTANCE << 1
                    << user_account_keys::TIME_SMS_CAN_BE_SENT_AGAIN << 1
                    << user_account_keys::NUMBER_SWIPES_REMAINING << 1
                    << user_account_keys::SWIPES_LAST_UPDATED_TIME << 1
                    << user_account_keys::LOGGED_IN_TOKEN << 1
                    << user_account_keys::LOGGED_IN_TOKEN_EXPIRATION << 1
                    << user_account_keys::LOGGED_IN_INSTALLATION_ID << 1
                    << user_account_keys::COOL_DOWN_ON_SMS_RETURN_MESSAGE << 1
                    << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                    << user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << 1
                    << user_account_keys::BIRTHDAY_TIMESTAMP << 1
                    << user_account_keys::GENDER_TIMESTAMP << 1
                    << user_account_keys::FIRST_NAME_TIMESTAMP << 1
                    << user_account_keys::BIO_TIMESTAMP << 1
                    << user_account_keys::CITY_NAME_TIMESTAMP << 1
                    << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << 1
                    << user_account_keys::EMAIL_TIMESTAMP << 1
                    << user_account_keys::CATEGORIES_TIMESTAMP << 1
                    << user_account_keys::OTHER_USERS_BLOCKED << 1
            << finalize
    );

    const bsoncxx::document::value find_user_account =
            document{}
                    << search_field_key << search_field_value
            << finalize;

    bsoncxx::stdx::optional<bsoncxx::document::value> user_account_find_result;
    try {
        user_account_find_result = user_account_collection.find_one_and_update(
                find_user_account.view(),
                update_login_info,
                find_user_account_options
        );
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(),std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
    }

    if (user_account_find_result) { //if number or accountID has a user account

        const bsoncxx::document::view user_account_doc_view = *user_account_find_result;
        LoginFunctionResultValuesEnum login_function_result_value;

        //extract the response saved by the login pipeline
        auto return_message_element = user_account_doc_view[user_account_keys::LOGGED_IN_RETURN_MESSAGE];
        if (return_message_element
            && return_message_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            login_function_result_value = LoginFunctionResultValuesEnum(return_message_element.get_int32().value);
        } else { //if element does not exist or is not type int32
            logElementError(
                    __LINE__, __FILE__,
                    return_message_element,user_account_doc_view,
                    bsoncxx::type::k_int32, user_account_keys::LOGGED_IN_RETURN_MESSAGE,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
            return "";
        }

        //If logged in by account_id extract phone number from user account document to make sure it matches.
        if (account_type == AccountLoginType::GOOGLE_ACCOUNT || account_type == AccountLoginType::FACEBOOK_ACCOUNT) {

            auto phone_number_element = user_account_doc_view[user_account_keys::PHONE_NUMBER];

            if (phone_number_element && phone_number_element.type() ==
                                        bsoncxx::type::k_utf8) { //if element exists and is type utf8

                phone_number = phone_number_element.get_string().value.to_string();
            } else { //if element does not exist or is not type utf8
                logElementError(
                        __LINE__, __FILE__,
                        phone_number_element, user_account_doc_view,
                        bsoncxx::type::k_utf8, user_account_keys::PHONE_NUMBER,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );
                response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                return "";
            }
        }

        switch (login_function_result_value) {
            case LOGIN_ERROR_UNKNOWN_VALUE: {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(),std::string("Error with pipeline logic when logging in."),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "update_pipeline", makePrettyJson(update_login_info.view_array())
                );
                response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                return "";
            }
            case LOGIN_BANNED:
            case LOGIN_SUSPENDED: {

                std::string account_inactive_message;

                //extract inactivity message
                auto account_inactive_message_element = user_account_doc_view[user_account_keys::INACTIVE_MESSAGE];
                if (account_inactive_message_element && account_inactive_message_element.type() ==
                                                        bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    account_inactive_message = account_inactive_message_element.get_string().value.to_string();

                    if (login_function_result_value == LoginFunctionResultValuesEnum::LOGIN_SUSPENDED) {

                        std::chrono::milliseconds account_inactive_duration;

                        auto account_inactive_duration_element = user_account_doc_view[user_account_keys::INACTIVE_END_TIME];
                        if (account_inactive_duration_element
                            && account_inactive_duration_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
                            account_inactive_duration = account_inactive_duration_element.get_date().value;
                        } else { //if element does not exist or is not type date
                            logElementError(
                                    __LINE__, __FILE__,
                                    account_inactive_duration_element, user_account_doc_view,
                                    bsoncxx::type::k_date, user_account_keys::INACTIVE_END_TIME,
                                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                            );
                            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                            return "";
                        }

                        response->set_time_out_duration_remaining((account_inactive_duration - current_timestamp).count());

                        response->set_access_status(AccessStatus::SUSPENDED);
                    } else {
                        response->set_access_status(AccessStatus::BANNED);
                    }

                    response->set_time_out_message(account_inactive_message);

                    response->set_return_status(LoginValuesToReturnToClient::ACCOUNT_CLOSED);
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            account_inactive_message_element, user_account_doc_view,
                            bsoncxx::type::k_utf8, user_account_keys::INACTIVE_MESSAGE,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );
                    response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                }
                return "";
            }
            case LOGIN_INSTALLATION_DOES_NOT_EXIST: {
#ifndef _RELEASE
                std::cout << "Failed to find Installation ID! (Not a testing error)\n";
#endif
                //This means the device ID is not stored inside the array of saved device IDs

                //this is set to prevent a data leak
                //if someone gets a new phone number and their old phone number is still saved as an account
                //in the database, then someone randomly (or not so randomly) gets their old number, then they
                //can log into the old account, so if a new device attempts to log in it will ask for the birthday
                //the new user will then be allowed to OVERWRITE the old account however they can not access it
                response->set_birthday_not_needed(false);
                [[fallthrough]];
            }
            case LOGIN_ACCOUNT_ID_DOES_NOT_EXIST:
            case LOGIN_VERIFICATION_TIME_EXPIRED: {
                bsoncxx::oid user_account_oid;
                auto user_account_oid_element = user_account_doc_view["_id"];
                if (user_account_oid_element
                    && user_account_oid_element.type() == bsoncxx::type::k_oid) { //if successfully extracted id value
                    user_account_oid = user_account_oid_element.get_oid().value;
                } else {
                    logElementError(
                            __LINE__, __FILE__,
                            user_account_oid_element, user_account_doc_view,
                            bsoncxx::type::k_oid, "_id",
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );

                    return "";
                }

                std::string sms_verification_code = handleSmsCoolDownAndVerification(
                        response,
                        account_type,
                        installation_id,
                        phone_number,
                        account_id,
                        mongo_cpp_client,
                        accounts_db,
                        current_timestamp,
                        return_verification_code,
                        user_account_doc_view,
                        user_account_keys::COOL_DOWN_ON_SMS_RETURN_MESSAGE,
                        &user_account_oid
                );

                //access_status ACCESS_GRANTED along with return_status REQUIRES_AUTHENTICATION will allow client to
                // determine that the account already existed. If ACCESS_GRANTED is NOT set, it will delete the account
                // clearing all existing data from the database when REQUIRES_AUTHENTICATION is returned.
                response->set_access_status(AccessStatus::ACCESS_GRANTED);
                return sms_verification_code;
            }
            case LOGIN_INSTALLATION_ID_DOES_NOT_MATCH:
            case LOGIN_LOGIN_TOKEN_EXPIRED:
            case LOGIN_USER_LOGGED_IN: {

                //Login token was refreshed for these results
                if (!checkOutdatedAccountInfoAndSetToResponse(
                        accounts_db,
                        phone_number,
                        mongo_cpp_client,
                        user_account_collection,
                        find_user_account,
                        request,
                        user_account_doc_view,
                        response,
                        current_timestamp)
                ) { //if failed to extract values
                    response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                    return "";
                } else { //if properly extracted values

                    auto login_token_element = user_account_doc_view[user_account_keys::LOGGED_IN_TOKEN];
                    if (login_token_element
                        && login_token_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                        response->set_login_token(login_token_element.get_string().value.to_string());
                    } else { //if element does not exist or is not type utf8
                        logElementError(
                                __LINE__, __FILE__,
                                login_token_element, user_account_doc_view,
                                bsoncxx::type::k_utf8, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                        );
                        response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                        return "";
                    }

                    //Success!
                    response->set_return_status(LoginValuesToReturnToClient::LOGGED_IN);
                }

                auto user_account_oid_element = user_account_doc_view["_id"];
                if (user_account_oid_element
                    && user_account_oid_element.type() == bsoncxx::type::k_oid) { //if successfully extracted id value
                    auto push_update_doc = document{}
                            << user_account_statistics_keys::LOGIN_TIMES << open_document
                                << user_account_statistics_keys::login_times::INSTALLATION_ID << installation_id
                                << user_account_statistics_keys::login_times::DEVICE_NAME << device_name
                                << user_account_statistics_keys::login_times::API_NUMBER << request->api_number()
                                << user_account_statistics_keys::login_times::LETS_GO_VERSION << (int)request->lets_go_version()
                                << user_account_statistics_keys::login_times::CALLER_URI << caller_uri
                                << user_account_statistics_keys::login_times::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
                            << close_document
                        << finalize;

                    storeInfoToUserStatistics(
                            mongo_cpp_client,
                            accounts_db,
                            user_account_oid_element.get_oid().value,
                            push_update_doc,
                            current_timestamp
                    );
                } else {
                    logElementError(
                            __LINE__, __FILE__,
                            user_account_oid_element, user_account_doc_view,
                            bsoncxx::type::k_oid, "_id",
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );
                    //No need to error here, failed to set statistics docs
                    return "";
                }
            }
        }
    }
    else { //if number does NOT have a user account

        //if it is a Google or Facebook account then need to validate phone number
        if((account_type == AccountLoginType::GOOGLE_ACCOUNT || account_type == AccountLoginType::FACEBOOK_ACCOUNT)
            && !isValidPhoneNumber(phone_number)
        ) { //check if phone number is valid
            response->set_return_status(LoginValuesToReturnToClient::REQUIRES_PHONE_NUMBER_TO_CREATE_ACCOUNT);
            return "";
        }

        const bsoncxx::types::b_date current_date_mongo = bsoncxx::types::b_date{current_timestamp};

        mongocxx::collection info_stored_after_deletion_collection = accounts_db[collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME];

        mongocxx::pipeline separate_info_pipeline;

        const long long time_swipes_last_updated_completed = current_timestamp.count() / matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED.count();
        const long long time_sms_verification_updated_completed = current_timestamp.count() / general_values::TIME_BETWEEN_VERIFICATION_ATTEMPTS.count();

        const bsoncxx::document::value sms_on_cool_down_doc = getResetSmsIfOffCoolDown(
                current_date_mongo,
                info_stored_after_deletion_keys::COOL_DOWN_ON_SMS,
                info_stored_after_deletion_keys::TIME_SMS_CAN_BE_SENT_AGAIN
        );

        separate_info_pipeline.replace_root(
            document{}
                << "newRoot" << open_document

                    << "$cond" << open_document

                        //if field does not exist
                        << "if" << open_document
                            << "$eq" << open_array
                                << "$" + info_stored_after_deletion_keys::TIME_SMS_CAN_BE_SENT_AGAIN << bsoncxx::types::b_undefined{}
                            << close_array
                        << close_document

                        //then create document
                        << "then" << open_document
                            << info_stored_after_deletion_keys::PHONE_NUMBER << phone_number
                            << info_stored_after_deletion_keys::COOL_DOWN_ON_SMS << -1
                            << info_stored_after_deletion_keys::TIME_SMS_CAN_BE_SENT_AGAIN << bsoncxx::types::b_date{current_timestamp + general_values::TIME_BETWEEN_SENDING_SMS}
                            << info_stored_after_deletion_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
                            << info_stored_after_deletion_keys::NUMBER_SWIPES_REMAINING << matching_algorithm::MAXIMUM_NUMBER_SWIPES
                            << info_stored_after_deletion_keys::SWIPES_LAST_UPDATED_TIME << bsoncxx::types::b_int64{time_swipes_last_updated_completed}
                            << info_stored_after_deletion_keys::NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS << 0
                            << info_stored_after_deletion_keys::FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME << bsoncxx::types::b_int64{time_sms_verification_updated_completed}
                        << close_document

                        //if field does exist, check if sms on cool down
                        << "else" << open_document
                            << "$mergeObjects" << open_array
                                << "$$ROOT"
                                << sms_on_cool_down_doc
                            << close_array
                        << close_document

                    << close_document

                << close_document
            << finalize
        );

        mongocxx::options::find_one_and_update separate_info_opts;

        separate_info_opts.upsert(true);
        separate_info_opts.return_document(mongocxx::options::return_document::k_after);

        bsoncxx::stdx::optional<bsoncxx::document::value> separate_info_find_and_update;
        std::optional<std::string> separate_info_find_and_update_exception_string;
        try {
            separate_info_find_and_update = info_stored_after_deletion_collection.find_one_and_update(
                    document{}
                            << info_stored_after_deletion_keys::PHONE_NUMBER << phone_number
                    << finalize,
                    separate_info_pipeline,
                    separate_info_opts
            );
        } catch (const mongocxx::logic_error& e) {
            separate_info_find_and_update_exception_string = e.what();
        }

        if (separate_info_find_and_update) { //upsert succeeded

            const bsoncxx::document::view separate_info_doc_view = *separate_info_find_and_update;
            return handleSmsCoolDownAndVerification(
                    response,
                    account_type,
                    installation_id,
                    phone_number,
                    account_id,
                    mongo_cpp_client,
                    accounts_db,
                    current_timestamp,
                    return_verification_code,
                    separate_info_doc_view,
                    info_stored_after_deletion_keys::COOL_DOWN_ON_SMS
            );
        } else { //separate info failed to upsert properly

            const std::string error_string = std::string("Separate info document was failed to upsert.\n")
                    .append("Phone Number: ")
                    .append(phone_number)
                    .append(" InstallationId: ")
                    .append(installation_id);

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    separate_info_find_and_update_exception_string, error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME
            );
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);

            return "";

        }

    }

    return return_verification_code;
}

std::string handleSmsCoolDownAndVerification(
       loginfunction::LoginResponse* response,
       const AccountLoginType& account_type,
       const std::string& installation_id,
       const std::string& phone_number,
       const std::string& passed_account_id,
       mongocxx::client& mongo_cpp_client,
       mongocxx::database& accounts_db,
       const std::chrono::milliseconds& current_timestamp,
       std::string& return_val_verification_code,
       const bsoncxx::document::view& sms_doc_view,
       const std::string& cool_down_on_sms_return_message_key,
       const bsoncxx::oid* user_account_oid
) {

    //if sms is on cool down this will be a number, otherwise will be -1
    int cool_down_on_user_sms;

    auto cool_down_remaining_on_user_sms_element = sms_doc_view[cool_down_on_sms_return_message_key];
    if (cool_down_remaining_on_user_sms_element
        && cool_down_remaining_on_user_sms_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        cool_down_on_user_sms = cool_down_remaining_on_user_sms_element.get_int32().value;
    } else { //if element does not exist or is not type int32
        logElementError(
                __LINE__, __FILE__,
                cool_down_remaining_on_user_sms_element, sms_doc_view,
                bsoncxx::type::k_int32, cool_down_on_sms_return_message_key,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
        return "";
    }

    if (cool_down_on_user_sms != -1) {
        //NOTE: The lowest this could be is 1 (never 0) because the formula used to calculate it is
        // COOL_DOWN_ON_SMS_RETURN_MESSAGE <= currentTime.

        //There are two possibilities here.
        // 1) This was called after a user account was found. In this case it means one of three things
        //    i. Passed installation_id does not exist inside the account.
        //   ii. Passed account_id does not exist inside the account.
        //  iii. Account requires verification based on LAST_VERIFIED_TIME.
        // In general the pending account should exist. The only exception is if say the user adds an installation_id
        // then adds another installation_id in the next 30 seconds.
        // 2) No user account was found and the cooldown was extracted (or newly inserted) into the separate_info
        // collection. In this case if a cooldown exists (cool_down_on_user_sms != -1) then the pending account will
        // have already existed because it was created in the last ~30 seconds (the cooldown time).

        // TLDR: In order for the sms to be on cooldown a pending account will almost always exist, and if it doesn't
        // the update is harmless.

        const std::string indexing_string = getPendingAccountIndex(
                account_type,
                phone_number,
                passed_account_id
        );

        mongocxx::collection phone_pending_collection = accounts_db[collection_names::PENDING_ACCOUNT_COLLECTION_NAME];

        const bsoncxx::document::value update_pending_account_doc = document{}
                    << "$set" << open_document
                        << pending_account_keys::PHONE_NUMBER << phone_number
                        << pending_account_keys::INDEXING << indexing_string
                        << pending_account_keys::TYPE << account_type
                        << pending_account_keys::ID << installation_id
                    << close_document
                << finalize;

        try {

            //update the pending account
            phone_pending_collection.update_one(
                getPendingAccountFilter(
                    phone_number,
                    indexing_string,
                    installation_id
                ),
                update_pending_account_doc.view()
            );

        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME,
                    pending_account_keys::INDEXING, indexing_string,
                    pending_account_keys::PHONE_NUMBER, phone_number
            );

            //NOTE: OK to continue here
        }
        catch (const mongocxx::operation_exception& e) {
            mongocxx::stdx::optional<mongocxx::result::update> update_pending_account_success;
            std::optional<std::string> update_pending_account_exception_string;

            handleOperationExceptionPendingAccount(
                    e,
                    phone_pending_collection,
                    update_pending_account_doc.view(),
                    phone_number,
                    indexing_string,
                    installation_id,
                    false,
                    update_pending_account_success,
                    update_pending_account_exception_string
            );

            //It is possible for nothing to be updated or modified in the case of two unique account, one with
            // a matching phone number field and one with a matching indexing field. In this case
            // handleOperationExceptionPendingAccount() will delete both pending accounts and NOT update anything
            // because upsert is disabled (because the update document does not contain all fields).
            if(!update_pending_account_success) {
                const std::string error_string = std::string("Pending document was not updated successfully.\n")
                        .append("Phone Number: ")
                        .append(phone_number)
                        .append(" DeviceID: ")
                        .append(installation_id);

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        update_pending_account_exception_string, error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME,
                        "failedDocument", update_pending_account_doc.view()
                );

                response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                return "";
            }
        }

        response->set_sms_cool_down(cool_down_on_user_sms);
        response->set_return_status(LoginValuesToReturnToClient::SMS_ON_COOL_DOWN);
    }
    else {

        const long long time_verification_sent_last_updated = current_timestamp.count() / general_values::TIME_BETWEEN_SMS_VERIFICATION_MESSAGES_BY_INSTALLATION_ID.count();

        mongocxx::collection pre_login_checkers_collection = accounts_db[collection_names::PRE_LOGIN_CHECKERS_COLLECTION_NAME];

        mongocxx::pipeline pre_login_checkers_pipeline;

        //Check if installation ID has sent 'too many' sms verification messages.
        pre_login_checkers_pipeline.replace_root(
            document{}
                << "newRoot" << open_document

                    << "$cond" << open_document

                        //if field does not exist
                        << "if" << open_document
                            << "$eq" << open_array
                                << "$" + pre_login_checkers_keys::INSTALLATION_ID << bsoncxx::types::b_undefined{}
                            << close_array
                        << close_document

                        //then create document
                        << "then" << open_document
                            << pre_login_checkers_keys::INSTALLATION_ID << installation_id
                            << pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT << 1
                            << pre_login_checkers_keys::SMS_VERIFICATION_MESSAGES_LAST_UPDATE_TIME << bsoncxx::types::b_int64{time_verification_sent_last_updated}
                        << close_document

                        //if field does exist, check if sms on cool down
                        << "else" << open_document
                            << "$mergeObjects" << open_array
                                << "$$ROOT"

                                << open_document

                                    << "$cond" << open_document

                                        //if last updated time has passed
                                        << "if" << open_document
                                            << "$gt" << open_array
                                                << bsoncxx::types::b_int64{time_verification_sent_last_updated}
                                                << "$" + pre_login_checkers_keys::SMS_VERIFICATION_MESSAGES_LAST_UPDATE_TIME
                                            << close_array
                                        << close_document

                                        //then reset values to defaults
                                        << "then" << open_document
                                            << pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT << 1
                                            << pre_login_checkers_keys::SMS_VERIFICATION_MESSAGES_LAST_UPDATE_TIME << bsoncxx::types::b_int64{time_verification_sent_last_updated}
                                        << close_document

                                        //else increment messages sent
                                        << "else" << open_document
                                            << pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT << open_document

                                                //This condition is here in order to prevent the case where enough attempts are made
                                                // to roll the 32-bit integer around in a circle (unlikely, but shouldn't have an
                                                // effect on performance).
                                                << "$cond" << open_document

                                                    //if the number is less or equal to the max
                                                    << "if" << open_document
                                                        << "$gte" << open_array
                                                            << general_values::MAXIMUM_NUMBER_SMS_MESSAGES_SENT_BY_INSTALLATION_ID
                                                            << "$" + pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT
                                                        << close_array
                                                    << close_document

                                                    //then increment it
                                                    << "then" << open_document
                                                        << "$add" << open_array
                                                            << "$" + pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT
                                                            << 1
                                                        << close_array
                                                    << close_document

                                                    //else return the number
                                                    << "else" << "$" + pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT

                                                << close_document

                                            << close_document
                                        << close_document

                                    << close_document

                                << close_document

                            << close_array
                        << close_document

                    << close_document

                << close_document
            << finalize
        );

        mongocxx::options::find_one_and_update pre_login_info_checkers_opts;

        pre_login_info_checkers_opts.upsert(true);
        pre_login_info_checkers_opts.return_document(mongocxx::options::return_document::k_after);
        pre_login_info_checkers_opts.projection(
                document{}
                    << pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT << 1
                << finalize
        );

        bsoncxx::stdx::optional<bsoncxx::document::value> pre_login_checkers_find_and_update;
        std::optional<std::string> pre_login_info_find_and_update_exception_string;
        try {
            pre_login_checkers_find_and_update = pre_login_checkers_collection.find_one_and_update(
                    document{}
                            << pre_login_checkers_keys::INSTALLATION_ID << installation_id
                    << finalize,
                    pre_login_checkers_pipeline,
                    pre_login_info_checkers_opts
            );
        } catch (const mongocxx::logic_error& e) {
            pre_login_info_find_and_update_exception_string = e.what();
        }

        if(!pre_login_checkers_find_and_update) {

            const std::string error_string = "Failed to find a document when checking for pre login checkers collection."
                                             " This document should be created if it did not exist, this should never happen.";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    pre_login_info_find_and_update_exception_string, error_string,
                    "time_verification_sent_last_updated", std::to_string(time_verification_sent_last_updated),
                    "installation_id", installation_id
            );

            //NOTE: Continue here, if a problem occurred, it is a problem on our end, so still send the sms message.

        } else {

            const bsoncxx::document::view pre_login_checkers_find_and_update_view = pre_login_checkers_find_and_update->view();

            int number_sms_messages_sent;
            const auto number_sms_messages_sent_element = pre_login_checkers_find_and_update_view[pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT];
            if (number_sms_messages_sent_element
                && number_sms_messages_sent_element.type() == bsoncxx::type::k_int32) {
                number_sms_messages_sent = number_sms_messages_sent_element.get_int32().value;
            } else {
                logElementError(
                        __LINE__, __FILE__,
                        number_sms_messages_sent_element, pre_login_checkers_find_and_update_view,
                        bsoncxx::type::k_int32, pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::PRE_LOGIN_CHECKERS_COLLECTION_NAME
                );
                response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                return return_val_verification_code;
            }

            if (number_sms_messages_sent > general_values::MAXIMUM_NUMBER_SMS_MESSAGES_SENT_BY_INSTALLATION_ID) {
                response->set_return_status(LoginValuesToReturnToClient::VERIFICATION_ON_COOL_DOWN);
                return return_val_verification_code;
            }
        }

        const bool successful = handleVerification(
                current_timestamp,
                mongo_cpp_client,
                accounts_db,
                phone_number,
                installation_id,
                account_type,
                passed_account_id,
                return_val_verification_code,
                user_account_oid
        );

        if (successful) {
            response->set_return_status(LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION);
        } else {
            //for this to happen an error had to occur in handleVerification, and it should already be logged
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
        }

    }

    return return_val_verification_code;
}

//handles the creation (or modification) of pending account and sends SMS
bool handleVerification(
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        const std::string& phone_number,
        const std::string& installation_id,
        const AccountLoginType& account_type,
        const std::string& account_id,
        std::string& return_val_verification_code,
        const bsoncxx::oid* user_account_oid
) {

    std::string verification_code;

    mongocxx::collection phone_pending_collection = accounts_db[collection_names::PENDING_ACCOUNT_COLLECTION_NAME];

    std::string random_seed_str = bsoncxx::oid{}.to_string();
    std::seed_seq seed(random_seed_str.begin(),random_seed_str.end());
    std::mt19937 rng(seed);

    //NOTE: Using only arabic digits for verification code because most (if not all) language keyboards have access to
    // arabic digits.
    //this must be size-2 it is a C style string and so the final char will be null terminator
    std::uniform_int_distribution<std::mt19937::result_type> distribution(0, 9);

    //NOTE: Branch prediction and speculative execution should make this zero overhead (except when the account is actually used
    // for testing).
    if(phone_number == SMS_VERIFICATION_PHONE_NUMBER) { //If demo account for Google Play
        //return 777777 as verification code
        verification_code = std::string(general_values::VERIFICATION_CODE_NUMBER_OF_DIGITS, '7');
    } else {
        //generate random verification code
        for (int i = 0; i < general_values::VERIFICATION_CODE_NUMBER_OF_DIGITS; i++) {
            verification_code += std::to_string(distribution(rng));
        }
    }

    return_val_verification_code = verification_code;

    const std::string indexing_string = getPendingAccountIndex(
            account_type,
            phone_number,
            account_id
    );

    const bsoncxx::document::value update_pending_accounts_doc =
        document{}
            << "$set" << open_document
                << pending_account_keys::TYPE << account_type
                << pending_account_keys::PHONE_NUMBER << phone_number
                << pending_account_keys::INDEXING << indexing_string
                << pending_account_keys::ID << installation_id
                << pending_account_keys::VERIFICATION_CODE << verification_code
                << pending_account_keys::TIME_VERIFICATION_CODE_WAS_SENT << bsoncxx::types::b_date{current_timestamp}
            << close_document
        << finalize;

    mongocxx::options::update update_pending_accounts_options;
    update_pending_accounts_options.upsert(true);

    bsoncxx::stdx::optional<mongocxx::result::update> update_pending_account_success;
    std::optional<std::string> update_pending_account_exception_string;
    try {
        //upsert the pending account
        update_pending_account_success = phone_pending_collection.update_one(
            getPendingAccountFilter(
                phone_number,
                indexing_string,
                installation_id
            ),
            update_pending_accounts_doc.view(),
            update_pending_accounts_options
        );
    }
    catch (const mongocxx::logic_error& e) {
        update_pending_account_exception_string = e.what();
    }
    catch (const mongocxx::operation_exception& e) {
        handleOperationExceptionPendingAccount(
                e,
                phone_pending_collection,
                update_pending_accounts_doc.view(),
                phone_number,
                indexing_string,
                installation_id,
                true,
                update_pending_account_success,
                update_pending_account_exception_string
        );
    }

    int num_docs_modified = 0;
    if (update_pending_account_success) { //if pending update successful

        //get the number of documents modified
        int num_upserted = update_pending_account_success->result().upserted_count();
        int num_matched = update_pending_account_success->result().matched_count();

        num_docs_modified = num_upserted + num_matched;
    }

    if (num_docs_modified == 1) { //if pending document was upserted

#ifndef _RELEASE
        std::cout << std::string("Sending code " + verification_code + " to phone number " + phone_number + " ...\n");
#endif
        sendSMS(
                verification_code,
                phone_number,
                mongo_cpp_client,
                accounts_db,
                current_timestamp,
                user_account_oid
        );

    } else {  //if pending document error occurred
        const std::string error_string = std::string("Pending document was not updated successfully.\n")
                .append("Phone Number: ")
                .append(phone_number)
                .append(" DeviceID: ")
                .append(installation_id);

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                update_pending_account_exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME,
                "failedDocument", update_pending_accounts_doc.view(),
                "numDocsUpserted", std::to_string(num_docs_modified)
        );
        return false;
    }

    return true;

}

void sendSMS(
        const std::string& verification_code,
        const std::string& phone_number [[maybe_unused]],
        mongocxx::client& mongo_cpp_client [[maybe_unused]],
        mongocxx::database& accounts_db [[maybe_unused]],
        const std::chrono::milliseconds& current_timestamp [[maybe_unused]],
        const bsoncxx::oid* user_account_oid [[maybe_unused]]
        ) {

    //Hash strings
    // Og4fM6BSxjb - Release
    // MqVAVu9kpIp - Debug
    const std::string sms_body = verification_code + " is your " + general_values::APP_NAME + " verification code!\nOg4fM6BSxjb";

    //NOTE: Branch prediction and speculative execution should make this zero overhead (except when the account is actually used
    // for testing).
    if(phone_number == SMS_VERIFICATION_PHONE_NUMBER) { //If demo account for Google Play, no need to send sms
        return;
    }

#ifdef _ACCEPT_SEND_SMS_COMMANDS
    //locks this thread for python instance
    PythonHandleGilState gil_state;

    PyObject* pFunc = PyObject_GetAttrString(send_sms_module, (char*) "send_sms_func");
    gil_state.automaticallyDecrementObjectReference(pFunc);

    if (pFunc && PyCallable_Check(pFunc)) {

        PyObject* pArgs = Py_BuildValue(
                "(sss)",
                sms_body.c_str(),
                SMS_VERIFICATION_PHONE_NUMBER.c_str(),
                phone_number.c_str()
        );
        gil_state.automaticallyDecrementObjectReference(pArgs);

        PyObject* pReturnValue = PyObject_CallObject(pFunc, pArgs);
        gil_state.automaticallyDecrementObjectReference(pReturnValue);

        if (pReturnValue && PyTuple_Check(pReturnValue)) { //if the return value was a tuple

            bool successful;
            char* message;

            bool ok = PyArg_ParseTuple(pReturnValue, "ps", &successful, &message);

            if (!ok) {

                const std::string error_string = "Error occurred when sending SMS.\n";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME,
                        "returned_exception", std::string(message));

            } else if (user_account_oid) {

                thread_pool.submit([
                                           currentTimestamp = current_timestamp,
                                           user_account_oid = user_account_oid
                                   ] {

                    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

                    //NOTE: Do not want database access running when the GIL for the python process is also locked. This is
                    // essentially a mutex lock, and it prevents other threads from running python scrips when it is locked.

                    auto push_update_doc = document{}
                            << user_account_statistics_keys::SMS_SENT_TIMES << bsoncxx::types::b_date{currentTimestamp}
                            << finalize;

                    storeInfoToUserStatistics(
                            mongo_cpp_client,
                            accounts_db,
                            *user_account_oid,
                            push_update_doc,
                            currentTimestamp
                    );

                });
            }
        }
        else { //return value was NOT a tuple
            const std::string error_string = "Return type from send_email inside send_email.py was NOT type Tuple or did not exist.\n";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "pReturnValue", pReturnValue?"true":"false"
            );
        }

    }
    else {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string("Error occurred when sending SMS. pFunc did not return as callable or did not exist."),
                "pFunc", (pFunc?"true":"false")
        );
    }

#endif

}

void handleOperationExceptionPendingAccount(
        const mongocxx::operation_exception& e,
        mongocxx::collection& phone_pending_collection,
        const bsoncxx::document::view& update_pending_accounts_doc,
        const std::string& phone_number,
        const std::string& indexing_string,
        const std::string& installation_id,
        const bool upsert,
        mongocxx::stdx::optional<mongocxx::result::update>& update_pending_account_success,
        std::optional<std::string>& update_pending_account_exception_string
) {

    if (e.code().value() == 11000) { //duplicate key error

        //This situation is possible if a user does something similar to
        //1) Create a pending account with phone_number1, installation_id1.
        //2) Create a different pending account with phone_number2, installation_id2.
        //3) Attempt to log in with phone_number1, installation_id2.
        // This will attempt to update the installation_id2 to installation_id1 as it finds phone_number1
        // however because installation_id2 already exists it will fail.
        //The situation can happen with any combination of the 3 unique index values on the pending collection, not
        // just phone_number and installation_id

        //Remove all pending accounts.
        bsoncxx::stdx::optional<mongocxx::result::delete_result> remove_pending_accounts;
        try {
            //NOTE: Do NOT delete the installation id match so that the verification code can be preserved. This
            // should be unique anyway.
            remove_pending_accounts = phone_pending_collection.delete_many(
                document{}
                    << "$or" << open_array
                        << open_document
                            << pending_account_keys::PHONE_NUMBER << phone_number
                        << close_document
                        << open_document
                            << pending_account_keys::INDEXING << indexing_string
                        << close_document
                    << close_array
                << finalize
            );
        } catch (const mongocxx::logic_error& e) {
            update_pending_account_exception_string = e.what();
        }

        if(remove_pending_accounts && remove_pending_accounts->deleted_count() > 0) {
            try {

                mongocxx::options::update opts;
                opts.upsert(upsert);

                //Attempt to update the pending account again after accounts were removed (outside the
                // installation Id).
                update_pending_account_success = phone_pending_collection.update_one(
                        document{}
                                << pending_account_keys::ID << installation_id
                                << finalize,
                        update_pending_accounts_doc,
                        opts
                );
            }
            catch (const mongocxx::logic_error& e) {
                update_pending_account_exception_string = e.what();
            }
        } // else {} //Allow the error to be stored below.

    } else {
        if (e.raw_server_error()) { //raw_server_error exists
            throw mongocxx::operation_exception(
                    e.code(),
                    bsoncxx::document::value(e.raw_server_error().value()),
                    e.what()
            );
        } else { //raw_server_error does not exist
            throw mongocxx::operation_exception(
                    e.code(),
                    document{} << finalize,
                    e.what()
            );
        }
    }
}

bool checkOutdatedAccountInfoAndSetToResponse(
        mongocxx::database& accounts_db,
        const std::string& phone_number,
        mongocxx::client& mongo_cpp_client,
        mongocxx::collection& user_account_collection,
        const bsoncxx::document::view& query_for_user_account_doc_view,
        const loginfunction::LoginRequest* request,
        const bsoncxx::document::view& user_account_view,
        loginfunction::LoginResponse* response,
        const std::chrono::milliseconds& current_timestamp
) {

    bsoncxx::oid user_account_oid;
    auto user_account_oid_element = user_account_view["_id"];
    if (user_account_oid_element
        && user_account_oid_element.type() == bsoncxx::type::k_oid) { //if successfully extracted id value
        user_account_oid = user_account_oid_element.get_oid().value;
    } else {
        logElementError(
                __LINE__, __FILE__,
                user_account_oid_element, user_account_view,
                bsoncxx::type::k_oid, "_id",
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
        return false;
    }

    UserAccountStatus account_status;
    auto account_active_message_element = user_account_view[user_account_keys::STATUS];
    if (account_active_message_element
        && account_active_message_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        account_status = UserAccountStatus(account_active_message_element.get_int32().value);
    } else { //if accountActive does not exist or is not type int32
        logElementError(
                __LINE__, __FILE__,
                account_active_message_element, user_account_view,
                bsoncxx::type::k_int32, user_account_keys::STATUS,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
        return false;
    }

    AlgorithmSearchOptions algorithm_search_by_options;
    auto algorithm_search_by_options_element = user_account_view[user_account_keys::SEARCH_BY_OPTIONS];
    if (algorithm_search_by_options_element &&
        algorithm_search_by_options_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        algorithm_search_by_options = AlgorithmSearchOptions(algorithm_search_by_options_element.get_int32().value);
    } else { //if accountActive does not exist or is not type int32
        logElementError(__LINE__, __FILE__, algorithm_search_by_options_element,
                        user_account_view, bsoncxx::type::k_int32, user_account_keys::STATUS,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
        return false;
    }

    response->set_phone_number(phone_number);
    response->set_account_oid(user_account_oid.to_string());
    response->set_algorithm_search_options(algorithm_search_by_options);

    //set global variables
    GlobalConstantsMessage* global_constants = response->mutable_login_values_to_return_to_client()->mutable_global_constant_values();

    global_constants->set_time_between_login_token_verification(general_values::TIME_BETWEEN_TOKEN_VERIFICATION.count());

    global_constants->set_time_available_to_select_time_frames(general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count());
    global_constants->set_maximum_picture_size_in_bytes(server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES);
    global_constants->set_maximum_picture_thumbnail_size_in_bytes(
            server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES);
    global_constants->set_time_between_sending_sms(general_values::TIME_BETWEEN_SENDING_SMS.count());

    global_constants->set_max_between_time(static_cast<int>(matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count()));
    global_constants->set_activity_match_weight(matching_algorithm::ACTIVITY_MATCH_WEIGHT);
    global_constants->set_categories_match_weight(matching_algorithm::CATEGORIES_MATCH_WEIGHT);
    global_constants->set_overlapping_activity_times_weight(matching_algorithm::OVERLAPPING_ACTIVITY_TIMES_WEIGHT);
    global_constants->set_overlapping_category_times_weight(matching_algorithm::OVERLAPPING_CATEGORY_TIMES_WEIGHT);
    global_constants->set_between_activity_times_weight(matching_algorithm::BETWEEN_ACTIVITY_TIMES_WEIGHT);
    global_constants->set_between_category_times_weight(matching_algorithm::BETWEEN_CATEGORY_TIMES_WEIGHT);

    global_constants->set_number_pictures_stored_per_account(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT);
    global_constants->set_number_activities_stored_per_account(
            server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT);
    global_constants->set_number_time_frames_stored_per_account(
            server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY);
    global_constants->set_number_gender_user_can_match_with(
            server_parameter_restrictions::NUMBER_GENDER_USER_CAN_MATCH_WITH);
    global_constants->set_time_before_expired_time_match_will_be_returned(
            matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());

    global_constants->set_maximum_number_bytes_trimmed_text_message(server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE);
    global_constants->set_maximum_number_allowed_bytes(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES);
    global_constants->set_maximum_number_allowed_bytes_user_bio(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_BIO);
    global_constants->set_maximum_number_allowed_bytes_first_name(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_FIRST_NAME);
    global_constants->set_maximum_number_allowed_bytes_user_feedback(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_FEEDBACK);
    global_constants->set_maximum_number_allowed_bytes_error_message(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE);
    global_constants->set_maximum_number_allowed_bytes_text_message(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_MESSAGE);

    global_constants->set_highest_displayed_age(android_specific_values::HIGHEST_DISPLAYED_AGE);
    global_constants->set_highest_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);
    global_constants->set_lowest_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
    global_constants->set_maximum_time_match_will_stay_on_device(
            matching_algorithm::MAXIMUM_TIME_MATCH_WILL_STAY_ON_DEVICE.count());
    global_constants->set_minimum_allowed_distance(server_parameter_restrictions::MINIMUM_ALLOWED_DISTANCE);
    global_constants->set_maximum_allowed_distance(server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE);

    global_constants->set_maximum_number_response_messages(matching_algorithm::MAXIMUM_NUMBER_RESPONSE_MESSAGES);

    global_constants->set_maximum_number_chat_room_id_chars(chat_room_values::CHAT_ROOM_ID_NUMBER_OF_DIGITS);
    global_constants->set_maximum_chat_message_size_in_bytes(
            server_parameter_restrictions::MAXIMUM_CHAT_MESSAGE_PICTURE_SIZE_IN_BYTES);
    global_constants->set_maximum_chat_message_thumbnail_size_in_bytes(
            server_parameter_restrictions::MAXIMUM_CHAT_MESSAGE_THUMBNAIL_SIZE_IN_BYTES);
    global_constants->set_time_between_chat_message_invite_expiration(
            android_specific_values::TIME_BETWEEN_CHAT_MESSAGE_INVITE_EXPIRATION.count());

    global_constants->set_chat_room_stream_number_times_to_refresh_before_restart(android_specific_values::NUMBER_TIMES_CHAT_STREAM_REFRESHES_BEFORE_RESTART);
    global_constants->set_chat_room_stream_deadline_time(android_specific_values::CHAT_ROOM_STREAM_DEADLINE_TIME.count());
    global_constants->set_max_number_messages_to_request(chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST);
    global_constants->set_time_info_has_not_been_observed_before_cleaned(
            android_specific_values::INFO_HAS_NOT_BEEN_OBSERVED_BEFORE_CLEANED.count());

    global_constants->set_activity_icon_width_in_pixels(server_parameter_restrictions::ACTIVITY_ICON_WIDTH_IN_PIXELS);
    global_constants->set_activity_icon_height_in_pixels(server_parameter_restrictions::ACTIVITY_ICON_HEIGHT_IN_PIXELS);
    global_constants->set_activity_icon_border_width(android_specific_values::ACTIVITY_ICON_BORDER_WIDTH);
    global_constants->set_activity_icon_padding(android_specific_values::ACTIVITY_ICON_PADDING);
    global_constants->set_activity_icon_color(android_specific_values::ACTIVITY_ICON_COLOR);
    global_constants->set_activity_icon_background_color(android_specific_values::ACTIVITY_ICON_BACKGROUND_COLOR);

    global_constants->set_image_quality_value(android_specific_values::IMAGE_QUALITY_VALUE);
    global_constants->set_picture_maximum_cropped_size_px(android_specific_values::PICTURE_MAXIMUM_CROPPED_SIZE_PX);
    global_constants->set_picture_thumbnail_maximum_cropped_size_px(android_specific_values::PICTURE_THUMBNAIL_MAXIMUM_CROPPED_SIZE_PX);

    global_constants->set_time_between_updating_single_user(android_specific_values::TIME_BETWEEN_UPDATING_SINGLE_USER);
    global_constants->set_time_between_updating_single_user_function_running(android_specific_values::TIME_BETWEEN_UPDATING_SINGLE_USER_FUNCTION_RUNNING);

    global_constants->set_chat_message_image_thumbnail_width(android_specific_values::CHAT_MESSAGE_IMAGE_THUMBNAIL_WIDTH);
    global_constants->set_chat_message_image_thumbnail_height(android_specific_values::CHAT_MESSAGE_IMAGE_THUMBNAIL_HEIGHT);

    global_constants->set_connection_idle_timeout_in_ms(grpc_values::CONNECTION_IDLE_TIMEOUT_IN_MS);

    for(const std::string& mime_type : accepted_mime_types) {
        global_constants->add_mime_types_accepted_by_server(mime_type);
    }

    for(const auto& address : GRPC_SERVER_ADDRESSES_URI) {
        auto server_info = global_constants->add_server_info();
        server_info->set_address(address.ADDRESS);
        server_info->set_port(address.PORT);
    }

    global_constants->set_app_url_for_sharing(android_specific_values::APP_URL_STRING_FOR_SHARING);

    global_constants->set_max_server_outbound_message_size_in_bytes(grpc_values::MAX_SEND_MESSAGE_LENGTH);

    global_constants->set_time_to_request_previous_messages(chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count());

    global_constants->set_maximum_meta_data_size_to_send_from_client(grpc_values::MAXIMUM_META_DATA_SIZE_TO_SEND_FROM_CLIENT);

    global_constants->set_time_after_expired_match_removed_from_device(general_values::TIME_AFTER_EXPIRED_MATCH_REMOVED_FROM_DEVICE.count());

    global_constants->set_event_id_default(chat_room_values::EVENT_ID_DEFAULT);
    global_constants->set_pinned_location_default_longitude(chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE);
    global_constants->set_pinned_location_default_latitude(chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE);
    global_constants->set_qr_code_default(chat_room_values::QR_CODE_DEFAULT);
    global_constants->set_qr_code_message_default(chat_room_values::QR_CODE_MESSAGE_DEFAULT);
    global_constants->set_qr_code_time_updated_default(chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT);
    global_constants->set_event_user_last_activity_time_default(chat_room_values::EVENT_USER_LAST_ACTIVITY_TIME_DEFAULT.count());

    global_constants->set_maximum_number_allowed_bytes_event_title(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_EVENT_TITLE);

    global_constants->set_event_gender_value(general_values::EVENT_GENDER_VALUE);
    global_constants->set_event_age_value(general_values::EVENT_AGE_VALUE);

    global_constants->set_admin_first_name(event_admin_values::FIRST_NAME);

    //request icon indexes that require updating from database
    if (!requestOutOfDateIconIndexValues(
            request->icon_timestamps(),
            response->mutable_login_values_to_return_to_client()->mutable_icons_index(),
            accounts_db)
    ) {
        response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
        return false;
    }

    //request categories and activities from database
    if (!requestAllServerCategoriesActivitiesHelper(
            response->mutable_login_values_to_return_to_client()->mutable_server_categories(),
            response->mutable_login_values_to_return_to_client()->mutable_server_activities(),
            accounts_db)
    ) {
        response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
        return false;
    }

    bool account_needs_more_info =
            getUserInfoTimestamps(
                    user_account_view,
                    response->mutable_pre_login_timestamps(),
                    response->mutable_pictures_timestamps()
            );

    //store blocked accounts
    auto accounts_in_chat_room_element = user_account_view[user_account_keys::OTHER_USERS_BLOCKED];
    if (accounts_in_chat_room_element
        && accounts_in_chat_room_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
        const bsoncxx::array::view blocked_accounts_in_chat_room = accounts_in_chat_room_element.get_array().value;

        for (const auto& blocked_account : blocked_accounts_in_chat_room) {
            if (blocked_account.type() == bsoncxx::type::k_document) { //if this array element is a document

                const bsoncxx::document::view blocked_account_view = blocked_account.get_document().value;

                auto other_user_oid_element = blocked_account_view[user_account_keys::other_users_blocked::OID_STRING];
                if (other_user_oid_element &&
                    other_user_oid_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    response->add_blocked_accounts(other_user_oid_element.get_string().value.to_string());
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            other_user_oid_element, blocked_account_view,
                            bsoncxx::type::k_utf8, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );
                    continue;
                }

            } else { //if this array element is not a document
                logElementError(
                        __LINE__, __FILE__,
                        accounts_in_chat_room_element, user_account_view,
                        bsoncxx::type::k_document, user_account_keys::OTHER_USERS_BLOCKED,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );
                //NOTE: ok to continue here
            }
        }
    }
    else { //if element does not exist or is not type array
        logElementError(
                __LINE__, __FILE__,
                accounts_in_chat_room_element, user_account_view,
                bsoncxx::type::k_array, user_account_keys::OTHER_USERS_BLOCKED,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        return false;
    }

    auto subscription_status_element = user_account_view[user_account_keys::SUBSCRIPTION_STATUS];
    if (subscription_status_element
        && subscription_status_element.type() == bsoncxx::type::k_int32) { //if element exists and is type array
        response->set_subscription_status(
            UserSubscriptionStatus(
                    subscription_status_element.get_int32().value
                )
        );
    } else if(!subscription_status_element) { //if element does not exist
        //TODO: Remove this branch after database is updated.
        response->set_subscription_status(UserSubscriptionStatus::NO_SUBSCRIPTION);
    } else { //element exists but is invalid type
        logElementError(
                __LINE__, __FILE__,
                subscription_status_element, user_account_view,
                bsoncxx::type::k_int32, user_account_keys::SUBSCRIPTION_STATUS,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        return false;
    }

    auto subscription_expiration_time_element = user_account_view[user_account_keys::SUBSCRIPTION_EXPIRATION_TIME];
    if (subscription_expiration_time_element
        && subscription_expiration_time_element.type() == bsoncxx::type::k_date) { //if element exists and is type array
        response->set_subscription_expiration_time(
                subscription_expiration_time_element.get_date().value.count()
        );
    } else if(!subscription_status_element) { //if element does not exist
        //TODO: Remove this branch after database is updated.
        response->set_subscription_expiration_time(-1);
    } else { //element exists but is invalid type
        logElementError(
                __LINE__, __FILE__,
                subscription_status_element, user_account_view,
                bsoncxx::type::k_date, user_account_keys::SUBSCRIPTION_EXPIRATION_TIME,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        return false;
    }

    auto opted_in_to_promotional_email_element = user_account_view[user_account_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL];
    if (opted_in_to_promotional_email_element
        && opted_in_to_promotional_email_element.type() == bsoncxx::type::k_bool) { //if element exists and is type array
        response->set_opted_in_to_promotional_email(
                opted_in_to_promotional_email_element.get_bool().value
                );
    } else if(!opted_in_to_promotional_email_element) { //if element does not exist
        //TODO: Remove this branch after database is updated.
        response->set_opted_in_to_promotional_email(false);
    } else { //element exists but is invalid type
        logElementError(
                __LINE__, __FILE__,
                opted_in_to_promotional_email_element, user_account_view,
                bsoncxx::type::k_bool, user_account_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        return false;
    }

    //Variable declared for shorter names.
    auto response_times = response->mutable_pre_login_timestamps();

    //get birthday if it needs to be updated
    if (response_times->birthday_timestamp() != -1
        && request->birthday_timestamp() < response_times->birthday_timestamp()) { //if birthday info has been set on server and is outdated on device

        if (!requestBirthdayHelper(
                user_account_view,
                response->mutable_birthday_info()
        )) { //if function failed
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
            return false;
        }
    }

    //get email if it needs updating
    if (response_times->email_timestamp() != -1
        && request->email_timestamp() <
           response_times->email_timestamp()) { //if email info has been set on server and is outdated on device

        if (!requestEmailHelper(
                user_account_view,
                response->mutable_email_info())) { //if function failed
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
            return false;
        }
    }

    //get gender if it needs updating
    if (response_times->gender_timestamp() != -1
        && request->gender_timestamp() <
           response_times->gender_timestamp()) { //if gender info has been set on server and is outdated on device

        if (!requestGenderHelper(
                user_account_view, response->mutable_gender()
        )) { //if function failed
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
            return false;
        }

    }

    //get name if it needs updating
    if (response_times->name_timestamp() != -1
        && request->name_timestamp() <
           response_times->name_timestamp()) { //if name info has been set on server and is outdated on device

        if (!requestNameHelper(
                user_account_view, response->mutable_name()
        )) { //if function failed
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
            return false;
        }
    }

    //get categories if possible
    if (response_times->categories_timestamp() != -1
        && response_times->birthday_timestamp() != -1) { //if categories info & birthday info (for age) have been set on server
        if (!requestCategoriesHelper(
                user_account_view,
                response->mutable_categories_array())
        ) { //if function failed
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
            return false;
        }

        int current_user_age;

        auto stored_age_element = user_account_view[user_account_keys::AGE];
        if (stored_age_element
            && stored_age_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            current_user_age = stored_age_element.get_int32().value;
        } else { //if element does not exist or is not type int32
            logElementError(
                    __LINE__, __FILE__,
                    stored_age_element,user_account_view,
                    bsoncxx::type::k_int32, user_account_keys::AGE,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
            return false;
        }

        bool activities_require_updating = false;

        for(const auto& activity_or_category : response->categories_array()) {
            int activity_index = activity_or_category.activity_index();
            //NOTE: Not checking min_age on categories only on activities, this is because it is much more
            // difficult to do this inside of set_categories.cpp. So following here for consistency.
            //If activity does not exist OR user is too young for activity run CATEGORIES update.
            //NOTE: activity_index is checked inside setCategories(), it should never be larger than the array, the
            // check is just to avoid the potential exception from grpc.
            if(activity_index >= response->login_values_to_return_to_client().server_activities().size()
                || current_user_age < response->login_values_to_return_to_client().server_activities()[activity_index].min_age()
            ) {
                activities_require_updating = true;
                break;
            }
        }

        //This check is here specifically for the case if an activity or category min age is changed at some point by an admin.
        // However, it can also be considered a failsafe too.
        if(activities_require_updating) {
            if(!updateInvalidActivities(
                    mongo_cpp_client,
                    user_account_collection,
                    query_for_user_account_doc_view,
                    user_account_view,
                    response,
                    current_timestamp,
                    user_account_oid,
                    current_user_age)
            ) {
                //error has already been stored
                response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                return false;
            }
        } else if(request->categories_timestamp() >= response_times->categories_timestamp()) { // time is NOT outdated on device
            //no update needed, clear array
            response->clear_categories_array();
        }
    }

    if (account_status == UserAccountStatus::STATUS_REQUIRES_MORE_INFO
        ) { //if the account is still in the 'requires login process to be complete' state
        account_needs_more_info = true;
    }

    if (account_needs_more_info) { //if the account needs more info
        response->set_access_status(NEEDS_MORE_INFO);
    }
    else { //if the account has all pre-login info

        auto post_login_info_timestamp_element = user_account_view[user_account_keys::POST_LOGIN_INFO_TIMESTAMP];
        if (post_login_info_timestamp_element
            && post_login_info_timestamp_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
            response->set_post_login_timestamp(post_login_info_timestamp_element.get_date().value.count());
        } else { //if element does not exist or is not type date
            logElementError(
                    __LINE__, __FILE__,
                    post_login_info_timestamp_element, user_account_view,
                    bsoncxx::type::k_date, user_account_keys::POST_LOGIN_INFO_TIMESTAMP,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
            return false;
        }

        //get post login info if it needs updating
        if (response->post_login_timestamp() != -1
            && request->post_login_info_timestamp() < response->post_login_timestamp()
        ) { //if post login info has been set on server and is outdated on device
            if (!requestPostLoginInfoHelper(
                    user_account_view, response->mutable_post_login_info())
                    ) { //if function failed
                response->set_return_status(LoginValuesToReturnToClient::LG_ERROR);
                return false;
            }
        }

        response->set_access_status(ACCESS_GRANTED);
    }

    return true;
}

bool updateInvalidActivities(
        mongocxx::client& mongo_cpp_client,
        mongocxx::collection& user_account_collection,
        const bsoncxx::document::view& find_user_account_doc,
        const bsoncxx::document::view& user_account_doc_view,
        loginfunction::LoginResponse* response,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::oid& user_account_oid,
        int current_user_age
) {

    bool transaction_successful = true;

    //A check must be run again because there is the possibility that the categories could have
    // changed. This must be run inside a transaction to guarantee that multiple steps are well...
    // transactional.
    mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {
        bsoncxx::stdx::optional<bsoncxx::document::value> user_account_find_result;
        std::optional<std::string> user_account_find_exception_string;
        try {
            mongocxx::options::find opts;

            opts.projection(
                    document{}
                            << user_account_keys::CATEGORIES << 1
                    << finalize
            );

            user_account_find_result = user_account_collection.find_one(
                *callback_session,
                find_user_account_doc,
                opts
            );
        } catch (const mongocxx::logic_error& e) {
            user_account_find_exception_string = e.what();
        }

        if(!user_account_find_result) {
            const std::string error_string = "Failed to find user account when invalid matching activities found AND account was just found.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    user_account_find_exception_string, error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "userAccountOID", user_account_oid,
                    "age", current_user_age,
                    "userAccountDocumentView", user_account_doc_view
            );

            transaction_successful = false;
            return;
        }

        const bsoncxx::document::view transaction_user_account_doc = user_account_find_result->view();

        google::protobuf::RepeatedPtrField<CategoryActivityMessage> extracted_activities;
        if (!requestCategoriesHelper(
                transaction_user_account_doc,
                &extracted_activities)
        ) { //if function failed
            transaction_successful = false;
            return;
        }

        bsoncxx::builder::basic::array account_activities_and_categories;
        std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
        for(const auto& activity : extracted_activities) {

            int activity_index = activity.activity_index();
            int category_index;

            if(activity_index >= response->login_values_to_return_to_client().server_activities().size()) {
                const std::string error_string = "User activity index did not exist inside server activities array.";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        user_account_find_exception_string, error_string,
                        "userAccountOID", user_account_oid,
                        "age", current_user_age,
                        "user_account_doc", transaction_user_account_doc,
                        "size", std::to_string(response->login_values_to_return_to_client().server_activities().size()),
                        "index", std::to_string(activity_index)
                        );

                continue;
            } else if(current_user_age < response->login_values_to_return_to_client().server_activities()[activity_index].min_age()) {
                continue;
            }

            category_index = response->login_values_to_return_to_client().server_activities()[activity_index].category_index();

            save_activity_time_frames(
                category_index_to_timeframes_map,
                account_activities_and_categories,
                activity,
                activity_index,
                category_index,
                current_timestamp
            );
        }

        if (account_activities_and_categories.view().empty()) {

            //If the final activity was removed, set to default activity only (at least 1 activity
            // should always exist).
            bsoncxx::array::value default_array = buildDefaultCategoriesArray();
            for(const auto& val : default_array.view()) {
                account_activities_and_categories.append(val.get_value());
            }
        } else {
            save_category_time_frames(
                    category_index_to_timeframes_map,
                    account_activities_and_categories
                    );
        }

        bsoncxx::builder::stream::document update_categories_document;

        update_categories_document
            << "$set" << open_document
                << user_account_keys::CATEGORIES << account_activities_and_categories
                << user_account_keys::CATEGORIES_TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
            << close_document;

        appendDocumentToClearMatchingInfoUpdate<true>(update_categories_document);

        bsoncxx::stdx::optional<bsoncxx::document::value> update_user_account_result;
        std::optional<std::string> update_categories_exception_string;
        try {

            mongocxx::options::find_one_and_update opts;
            opts.return_document(mongocxx::options::return_document::k_after);

            update_user_account_result = user_account_collection.find_one_and_update(
                    *callback_session,
                    find_user_account_doc,
                    update_categories_document.view(),
                    opts
                    );
        } catch (const mongocxx::logic_error& e) {
            update_categories_exception_string = e.what();
        }

        if(!update_user_account_result) {
            const std::string error_string = "Failed to update user account when invalid matching activities found.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    update_categories_exception_string, error_string,
                    "update_categories_document", update_categories_document.view(),
                    "age", current_user_age,
                    "activities", transaction_user_account_doc,
                    "userAccountOID", user_account_oid
            );

            transaction_successful = false;
            return;
        }

        response->clear_categories_array();
        response->mutable_pre_login_timestamps()->set_categories_timestamp(current_timestamp.count());

        // Save the newly updated document to the response.
        // This way of doing it is a little redundant because all activities and timeframes still
        // exist inside account_activities_and_categories. However, this will allow for fewer functions
        // to be used in transforming from one data structure to another.
        if (!requestCategoriesHelper(
                update_user_account_result->view(),
                response->mutable_categories_array())
                ) { //if function failed
            transaction_successful = false;
            return;
        }

    };

    mongocxx::client_session transaction_session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        transaction_session.with_transaction(transaction_callback);
    } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
        std::cout << "Exception calling loginFunction() transaction.\n" << e.what() << '\n';
#endif
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "userAccountOID", user_account_oid,
                "userAccountDocumentView", user_account_doc_view
        );
        transaction_successful = false;
    }

    if(!transaction_successful) {
        return false;
    }

    return true;
}
