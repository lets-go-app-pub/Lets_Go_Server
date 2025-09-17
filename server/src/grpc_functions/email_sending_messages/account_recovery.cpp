//
// Created by jeremiah on 8/16/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>

#include <UserAccountStatusEnum.grpc.pb.h>

#include <connection_pool_global_variable.h>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <store_mongoDB_error_and_exception.h>
#include <helper_functions/helper_functions.h>

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"

#include "email_sending_messages.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "account_recovery_keys.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void accountRecoveryImplementation(
        const email_sending_messages::AccountRecoveryRequest* request,
        email_sending_messages::AccountRecoveryResponse* response,
        SendEmailInterface* send_email_object
);

//primary function for accountRecovery, called from gRPC server implementation
void accountRecovery(
        const email_sending_messages::AccountRecoveryRequest* request,
        email_sending_messages::AccountRecoveryResponse* response,
        SendEmailInterface* send_email_object
) {

    handleFunctionOperationException(
            [&] {
                accountRecoveryImplementation(request, response, send_email_object);
            },
            [&] {
                response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::DATABASE_DOWN);
            },
            [&] {
                response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void accountRecoveryImplementation(
        const email_sending_messages::AccountRecoveryRequest* request,
        email_sending_messages::AccountRecoveryResponse* response,
        SendEmailInterface* send_email_object
) {
    const std::string& phone_number = request->phone_number();

    if (!isValidPhoneNumber(phone_number)) { //check if phone number is valid
        response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::INVALID_PHONE_NUMBER);
        return;
    }

    if (isInvalidLetsGoAndroidVersion(request->lets_go_version())) { //check if meets minimum version requirement
        response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::OUTDATED_VERSION);
        return;
    }

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    //NOTE: Only returning EmailSentStatus::EMAIL_SUCCESS (unless invalid parameters are passed) in order to avoid
    // people fishing for valid phone numbers. This could happen by sending in phone numbers over and over to the
    // server to see the error message returned to the client. The upside to sending having more verbosity in the error
    // messages for a standard user is that if they have a typo in their phone number, the system can catch it for them
    // a lot of the time (unless the type phone number is a real account). However, the users' security is more
    // important. For example, if they sign up for the app then get spam phone calls it will not only inconvenience the
    // user but look bad for the app as well.
    auto set_failed_to_be_sent = [&] {
        response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::SUCCESS);
        response->set_email_sent_status(email_sending_messages::EmailSentStatus::EMAIL_SUCCESS);
        response->set_timestamp(currentTimestamp.count());
    };

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection userAccountsCollection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::UNKNOWN);
    response->set_timestamp(-1L); // -1 means not set

    bsoncxx::stdx::optional<bsoncxx::document::value> found_user_account;
    try {
        mongocxx::options::find_one_and_update opts;
        opts.projection(
            document{}
                << "_id" << 1
                << user_account_keys::EMAIL_ADDRESS << 1
            << finalize
        );

        found_user_account = userAccountsCollection.find_one_and_update(
                document{}
                    << "$and" << open_array
                        << open_document
                            << user_account_keys::PHONE_NUMBER << phone_number
                        << close_document
                        << open_document
                            << user_account_keys::STATUS << open_document
                                << "$nin" << open_array
                                    << UserAccountStatus::STATUS_SUSPENDED
                                    << UserAccountStatus::STATUS_BANNED
                                << close_array
                            << close_document
                        << close_document
                        << open_document
                            << user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << open_document
                                << "$lte" << bsoncxx::types::b_date{currentTimestamp}
                            << close_document
                        << close_document
                    << close_array
                << finalize,
                document{}
                    << "$max" << open_document
                        << user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << bsoncxx::types::b_date{currentTimestamp + general_values::TIME_BETWEEN_SENDING_EMAILS}
                    << close_document
                << finalize,
            opts
        );
    } catch (const mongocxx::logic_error& e) {
        std::string errorString = "Error when attempting to find user account by phone number for account recovery.\n";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__, e.what(), errorString,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME,
                "failed_phone_number", phone_number);

        response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::LG_ERROR);
        return;
    }

    if (!found_user_account) { //if account phone number does not exist OR email is on cool down OR account is suspended/banned.
        set_failed_to_be_sent();
        return;
    }

    std::string email_address;
    bsoncxx::document::view userAccountDocView = found_user_account->view();

    bsoncxx::oid user_account_oid;

    auto userAccountOIDElement = userAccountDocView["_id"];
    if (userAccountOIDElement &&
        userAccountOIDElement.type() == bsoncxx::type::k_oid) { //if successfully extracted id value
        user_account_oid = userAccountOIDElement.get_oid().value;
    } else {
        logElementError(__LINE__, __FILE__, userAccountOIDElement,
                        userAccountDocView, bsoncxx::type::k_oid, "_id",
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::LG_ERROR);
        return;
    }

    auto email_address_element = userAccountDocView[user_account_keys::EMAIL_ADDRESS];
    if (email_address_element
        && email_address_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        email_address = email_address_element.get_string().value.to_string();
    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, email_address_element,
                        userAccountDocView, bsoncxx::type::k_utf8, user_account_keys::EMAIL_ADDRESS,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);
        response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::LG_ERROR);
        return;
    }

    std::string randomly_generated_code;

    mongocxx::collection account_recovery_collection = accounts_db[collection_names::ACCOUNT_RECOVERY_COLLECTION_NAME];

    bsoncxx::document::value find_document = document{}
                << account_recovery_keys::PHONE_NUMBER << phone_number
            << finalize;

    auto generate_update_document = [&](const std::string& passed_code) -> bsoncxx::document::value {
        return document{}
            << "$set" << open_document
                << account_recovery_keys::PHONE_NUMBER << phone_number
                << account_recovery_keys::VERIFICATION_CODE << passed_code
                << account_recovery_keys::TIME_VERIFICATION_CODE_GENERATED << bsoncxx::types::b_date{currentTimestamp}
                << account_recovery_keys::NUMBER_ATTEMPTS << 0
                << account_recovery_keys::USER_ACCOUNT_OID << user_account_oid
            << close_document
        << finalize;
    };

    if (!attempt_to_generate_unique_code_twice(
            randomly_generated_code,
            account_recovery_collection,
            find_document,
            generate_update_document)
    ) { //if upsert failed
        response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::LG_ERROR);
        return;
    }

    const std::string generated_link =
            general_values::WEB_SERVER_HOME_PAGE + "user-services/account-recovery/?accountRecoveryCode=" + randomly_generated_code;
    const std::string email_prefix = "LetsGoAccountRecovery";
    const std::string email_verification_subject = general_values::APP_NAME + " Account Recovery";
    const std::string email_verification_content =
            "Click the link below to recover your " + general_values::APP_NAME + " account.\n\n\n<a href='" + generated_link +
            "'><h3>Account Recovery Link</h3></a>\n\n\nIf this action was not initiated by you, please contact support@letsgoapp.site.\n\n\n<strong>DO NOT REPLY TO THIS EMAIL.</strong>";

    send_email_helper(
        email_prefix,
        email_address,
        email_verification_subject,
        email_verification_content,
        user_account_oid,
        currentTimestamp,
        send_email_object
    );

    //See NOTE on set_failed_to_be_sent() for why send_email_helper result does not matter.
    response->set_account_recovery_status(email_sending_messages::AccountRecoveryResponse::SUCCESS);
    response->set_email_sent_status(email_sending_messages::EmailSentStatus::EMAIL_SUCCESS);
    response->set_timestamp(currentTimestamp.count());
}