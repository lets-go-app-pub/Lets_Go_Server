//
// Created by jeremiah on 8/16/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <store_mongoDB_error_and_exception.h>
#include <helper_functions/helper_functions.h>

#include "email_sending_messages.h"

#include "global_bsoncxx_docs.h"

#include "connection_pool_global_variable.h"

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "email_verification_keys.h"
#include "general_values.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"


//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void emailVerificationImplementation(
        const email_sending_messages::EmailVerificationRequest* request,
        email_sending_messages::EmailVerificationResponse* response,
        SendEmailInterface* send_email_object
);

//primary function for accountRecovery, called from gRPC server implementation
void emailVerification(
        const email_sending_messages::EmailVerificationRequest* request,
        email_sending_messages::EmailVerificationResponse* response,
        SendEmailInterface* send_email_object
) {

    handleFunctionOperationException(
            [&] {
                emailVerificationImplementation(request, response, send_email_object);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            },
            __LINE__, __FILE__, request);
}

void emailVerificationImplementation(
        const email_sending_messages::EmailVerificationRequest* request,
        email_sending_messages::EmailVerificationResponse* response,
        SendEmailInterface* send_email_object
) {

    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationID;

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            userAccountOIDStr,
            loginTokenStr,
            installationID
    );

    if (basicInfoReturnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(basicInfoReturnStatus);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection userAccountsCollection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default
    response->set_timestamp(-1L); // -1 means not set

    const bsoncxx::oid userAccountOID{userAccountOIDStr};

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();
    const bsoncxx::types::b_date mongo_db_date_time_before_email = bsoncxx::types::b_date{currentTimestamp + general_values::TIME_BETWEEN_SENDING_EMAILS};

    bsoncxx::document::value mergeDocument =
            document{}
                << user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << open_document
                    << "$cond" << open_document
                        << "if" << open_document
                            << "$gt" << open_array
                                << mongo_db_date_time_before_email
                                << "$" + user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN
                            << close_array
                        << close_document
                        << "then" << mongo_db_date_time_before_email
                        << "else" << "$" + user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN
                    << close_document
                << close_document
            << finalize;

    std::shared_ptr<std::vector<bsoncxx::document::value>> appendToAndStatementDoc = std::make_shared<std::vector<bsoncxx::document::value>>();

    //checks if email is on cool down
    appendToAndStatementDoc->emplace_back(
     document{}
              << "$cond" << open_document

                  //if email has come off cool down
                  << "if" << open_document
                      << "$lte" << open_array
                            << "$" + user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << bsoncxx::types::b_date{currentTimestamp}
                      << close_array
                  << close_document

                  << "then" << true
                  << "else" << false
              << close_document
          << finalize
    );

    //project verified pictures key to extract thumbnail
    bsoncxx::document::value projectionDocument = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << user_account_keys::EMAIL_ADDRESS << 1
            << user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION << 1
            << finalize;

    bsoncxx::document::value loginDocument = getLoginDocument<false>(
            loginTokenStr,
            installationID,
            mergeDocument,
            currentTimestamp,
            appendToAndStatementDoc
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> findUserAccount;

    if (!runInitialLoginOperation(
            findUserAccount,
            userAccountsCollection,
            userAccountOID,
            loginDocument,
            projectionDocument)
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    ReturnStatus returnStatus = checkForValidLoginToken(findUserAccount, userAccountOIDStr);

    if (returnStatus == ReturnStatus::UNKNOWN) {
        //this means email was on cool down
        response->set_return_status(ReturnStatus::SUCCESS);
        response->set_email_sent_status(email_sending_messages::EmailSentStatus::EMAIL_ON_COOL_DOWN);
        response->set_email_address_is_already_verified(false);
        response->set_timestamp(currentTimestamp.count());
    } else if (returnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(returnStatus);
    } else { //ReturnStatus::SUCCESS

        bsoncxx::document::view userAccountDocView = findUserAccount->view();
        std::string email_address;
        bool email_address_requires_verification;

        auto email_address_requires_verification_element = userAccountDocView[user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION];
        if (email_address_requires_verification_element && email_address_requires_verification_element.type() ==
                                                           bsoncxx::type::k_bool) { //if element exists and is type bool
            email_address_requires_verification = email_address_requires_verification_element.get_bool().value;
        } else { //if element does not exist or is not type bool
            logElementError(__LINE__, __FILE__, email_address_requires_verification_element,
                            userAccountDocView, bsoncxx::type::k_bool, user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        if (!email_address_requires_verification) { //if email does NOT require Verification
            //this means email was on cool down
            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_email_sent_status(email_sending_messages::EmailSentStatus::EMAIL_VALUE_NOT_SET);
            response->set_email_address_is_already_verified(true);
            response->set_timestamp(currentTimestamp.count());
            return;
        }

        auto email_address_element = userAccountDocView[user_account_keys::EMAIL_ADDRESS];
        if (email_address_element &&
            email_address_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
            email_address = email_address_element.get_string().value.to_string();
        } else { //if element does not exist or is not type utf8
            logElementError(__LINE__, __FILE__, email_address_element,
                            userAccountDocView, bsoncxx::type::k_utf8, user_account_keys::EMAIL_ADDRESS,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        std::string randomly_generated_code;

        mongocxx::collection email_verification_collection = accounts_db[collection_names::EMAIL_VERIFICATION_COLLECTION_NAME];

        /** The find_document must ONLY match things that already match the _id field, that is because
         * USER_ACCOUNT_REFERENCE is set to "_id" and the _id field is
         * immutable, the upsert will still work so long as the _id field does not change however
         * if for example an 'or' statement is used as part of the query then it could find something
         * w/o a matching _id field and throw an exception when it attempts to modify it.
         **/
        bsoncxx::document::value find_document = document{}
            << email_verification_keys::USER_ACCOUNT_REFERENCE << userAccountOID
        << finalize;

        auto generate_update_document = [&](const std::string& passed_code) -> bsoncxx::document::value {
            return document{}
                << "$set" << open_document
                    << email_verification_keys::USER_ACCOUNT_REFERENCE << userAccountOID
                    << email_verification_keys::VERIFICATION_CODE << passed_code
                    << email_verification_keys::TIME_VERIFICATION_CODE_GENERATED << bsoncxx::types::b_date{currentTimestamp}
                    << email_verification_keys::ADDRESS_BEING_VERIFIED << email_address
                << close_document
            << finalize;
        };

        if (!attempt_to_generate_unique_code_twice(
                randomly_generated_code,
                email_verification_collection,
                find_document,
                generate_update_document)
        ) { //if upsert failed
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        const std::string generated_link =
                general_values::WEB_SERVER_HOME_PAGE + "user-services/email-verification/?emailVerificationCode=" + randomly_generated_code;
        const std::string email_prefix = "LetsGoEmailVerification";
        const std::string email_verification_subject = general_values::APP_NAME + " Email Verification";
        const std::string email_verification_content =
                "Click the link below to verify your email address.\n\n\n<a href='" + generated_link +
                "'><h3>Verification Link</h3></a>\n\n\nIf this action was not initiated by you, please contact support@letsgoapp.site.\n\n\n<strong>DO NOT REPLY TO THIS EMAIL.</strong>";

        if(send_email_helper(
                email_prefix,
                email_address,
                email_verification_subject,
                email_verification_content,
                userAccountOID,
                currentTimestamp,
                send_email_object)
        ) { //successfully sent email
            response->set_email_sent_status(email_sending_messages::EmailSentStatus::EMAIL_SUCCESS);
        } else { //failed to send email
            response->set_email_sent_status(email_sending_messages::EmailSentStatus::EMAIL_FAILED_TO_BE_SENT);
        }

        response->set_return_status(ReturnStatus::SUCCESS);
        response->set_email_address_is_already_verified(false);
        response->set_timestamp(currentTimestamp.count());
    }

}
