//proto file is SMSVerification.proto

#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/result/insert_one.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/collection.hpp>
#include <global_bsoncxx_docs.h>
#include <handle_function_operation_exception.h>

#include "sMS_verification.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "pending_account_keys.h"
#include "info_stored_after_deletion_keys.h"
#include "matching_algorithm.h"
#include "general_values.h"
#include "extract_data_from_bsoncxx.h"
#include "create_user_account.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void sMSVerificationImplementation(
        const sms_verification::SMSVerificationRequest* request,
        sms_verification::SMSVerificationResponse* response
);

//runs an upsert on the pending collection to put the document that was removed back in
bool savePendingDocumentBackToCollection(
        const bsoncxx::document::view& pending_account_document_view,
        mongocxx::collection& pending_collection,
        mongocxx::client_session* session
);

void sMSVerification(
        const sms_verification::SMSVerificationRequest* request,
        sms_verification::SMSVerificationResponse* response
) {
    handleFunctionOperationException(
        [&] {
            sMSVerificationImplementation(request, response);
        },
        [&] {
            response->set_return_status(sms_verification::SMSVerificationResponse_Status_DATABASE_DOWN);
        },
        [&] {
            response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
        },
        __LINE__, __FILE__, request
    );
}

void sMSVerificationImplementation(
        const sms_verification::SMSVerificationRequest* request,
        sms_verification::SMSVerificationResponse* response
) {

    if (isInvalidLetsGoAndroidVersion(request->lets_go_version())) {
        response->set_return_status(sms_verification::SMSVerificationResponse_Status_OUTDATED_VERSION);
        return;
    }

    const std::string& verification_code = request->verification_code();

    if (verification_code.size() != general_values::VERIFICATION_CODE_NUMBER_OF_DIGITS) {
        response->set_return_status(sms_verification::SMSVerificationResponse_Status_INVALID_VERIFICATION_CODE);
        return;
    }

    const std::string& installation_id = request->installation_id();

    if (isInvalidUUID(installation_id)) {
        response->set_return_status(sms_verification::SMSVerificationResponse_Status_INVALID_INSTALLATION_ID);

        return;
    }

    std::string pending_first_index_key;
    std::string phone_number;
    std::string account_id;

    switch (request->account_type()) {
        case AccountLoginType::GOOGLE_ACCOUNT:
        case AccountLoginType::FACEBOOK_ACCOUNT: {

            account_id = errorCheckAccountIDAndGenerateUniqueAccountID(
                    request->account_type(),
                    request->phone_number_or_account_id()
            );

            if (account_id.empty() || account_id == "~") {
                response->set_return_status(
                        sms_verification::SMSVerificationResponse_Status_INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);
                return;
            }

            pending_first_index_key = account_id;
            break;
        }
        case AccountLoginType::PHONE_ACCOUNT: {

            phone_number = request->phone_number_or_account_id(); //validated below

            if (!isValidPhoneNumber(phone_number)) { //check if phone number is valid
                response->set_return_status(
                        sms_verification::SMSVerificationResponse_Status_INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);
                return;
            }

            pending_first_index_key = phone_number;
            break;
        }
        default: {
            response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
            return;
        }
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection pending_collection = accounts_db[collection_names::PENDING_ACCOUNT_COLLECTION_NAME];
    mongocxx::collection user_account_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection info_stored_after_deletion_collection = accounts_db[collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    response->set_return_status(sms_verification::SMSVerificationResponse_Status_UNKNOWN);

    mongocxx::client_session::with_transaction_cb transaction_callback = [&](
            mongocxx::client_session* callback_session
    ) {

        try {

            bsoncxx::stdx::optional<bsoncxx::document::value> find_pending_document;
            try {

                //extract matching pending document
                //deleting this as I find it so only 1 'instance' can exist at a time, this prevents
                // from the document being double accessed

                //NOTE: another way of doing this is to find the document, then check it to see if it
                // matches, then delete it if the info does match, however the user should almost always complete this
                // function successfully (get the correct verification code, enter the correct birthday if mandatory etc...)
                // so find one and delete should end more efficient overall also it allows the situation to be more atomic
                // because only 1 thread can view the pending document
                find_pending_document = pending_collection.find_one_and_delete(
                        *callback_session,
                        document{}
                                << pending_account_keys::INDEXING << pending_first_index_key
                                << pending_account_keys::ID << installation_id
                                //<< PHONE_NUMBER << phoneNumber
                                << finalize
                );
            }
            catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME
                );
                response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                return;
            }

            if (request->account_type() != AccountLoginType::PHONE_ACCOUNT) {
                if (!find_pending_document) { //if pending account not found
                    response->set_return_status(sms_verification::SMSVerificationResponse_Status_PENDING_ACCOUNT_NOT_FOUND);
                    return;
                }

                //Set phone number for Google and Facebook accounts.
                phone_number = extractFromBsoncxx_k_utf8(
                        find_pending_document->view(),
                        pending_account_keys::PHONE_NUMBER
                );
            }

            bsoncxx::stdx::optional<bsoncxx::document::value> find_info_stored_after_deletion;
            try {

                const long long time_sms_verification_updated_completed = current_timestamp.count() / general_values::TIME_BETWEEN_VERIFICATION_ATTEMPTS.count();

                mongocxx::pipeline update_sms_verification_failed;

                const bsoncxx::document::value condition_document = document{}
                        << "$gt" << open_array
                            << bsoncxx::types::b_int64{time_sms_verification_updated_completed}
                            << "$" + info_stored_after_deletion_keys::FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME
                        << close_array
                    << finalize;

                update_sms_verification_failed.add_fields(
                    document{}

                        << info_stored_after_deletion_keys::FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME << open_document
                            << "$cond" << open_document
                                << "if" << condition_document
                                << "then" << bsoncxx::types::b_int64{time_sms_verification_updated_completed}
                                << "else" << "$" + info_stored_after_deletion_keys::FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME
                            << close_document
                        << close_document

                        << info_stored_after_deletion_keys::NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS << open_document
                            << "$cond" << open_document
                                << "if" << condition_document
                                << "then" << 0
                                << "else" << "$" + info_stored_after_deletion_keys::NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS
                            << close_document
                        << close_document

                    << finalize
                );

                mongocxx::options::find_one_and_update find_info_stored_after_deletion_options;

                find_info_stored_after_deletion_options.projection(
                    document{}
                        << info_stored_after_deletion_keys::TIME_SMS_CAN_BE_SENT_AGAIN << 1
                        << info_stored_after_deletion_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << 1
                        << info_stored_after_deletion_keys::NUMBER_SWIPES_REMAINING << 1
                        << info_stored_after_deletion_keys::SWIPES_LAST_UPDATED_TIME << 1
                        << info_stored_after_deletion_keys::NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS << 1
                    << finalize
                );

                //Return after to get updated NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS.
                find_info_stored_after_deletion_options.return_document(
                        mongocxx::options::return_document::k_after
                );

                find_info_stored_after_deletion = info_stored_after_deletion_collection.find_one_and_update(
                    *callback_session,
                    document{}
                        << info_stored_after_deletion_keys::PHONE_NUMBER << phone_number
                    << finalize,
                    update_sms_verification_failed,
                    find_info_stored_after_deletion_options
                );

            }
            catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME
                );

                response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                return;
            }

            if(!find_info_stored_after_deletion) {

                const std::string error_string = "Failed to find an account inside of INFO_STORED_AFTER_DELETION_COLLECTION. These accounts "
                                                 "are never deleted and so this should never be sent from the client.";

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME,
                        "phone_number", phone_number,
                        "request", request->DebugString()
                );

                //Not technically a correct return value, but the client can handle it in the same way as a pending account that
                // no longer exists.
                response->set_return_status(sms_verification::SMSVerificationResponse_Status_PENDING_ACCOUNT_NOT_FOUND);
                return;
            }

            const bsoncxx::document::view info_stored_after_deletion_doc_view = find_info_stored_after_deletion->view();

            const int number_failed_sms_verification_attempts = extractFromBsoncxx_k_int32(
                    info_stored_after_deletion_doc_view,
                    info_stored_after_deletion_keys::NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS
            );

            //Must be greater than or equal to be equal to MAXIMUM_NUMBER_FAILED_VERIFICATION_ATTEMPTS because
            // NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS starts at zero.
            if(number_failed_sms_verification_attempts >= general_values::MAXIMUM_NUMBER_FAILED_VERIFICATION_ATTEMPTS) {
                const std::string error_string = "Maximum number of failed sms verification attempts were reached when a user was logging in.";

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME,
                        "failed_attempts", std::to_string(number_failed_sms_verification_attempts),
                        "phone_number", phone_number
                );

                if (!find_pending_document
                      || savePendingDocumentBackToCollection(
                      find_pending_document->view(),
                      pending_collection,
                      callback_session)
                ) {
                    response->set_return_status(sms_verification::SMSVerificationResponse_Status_VERIFICATION_ON_COOLDOWN);
                } else { //error occurred
                    response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                }
                return;
            }

            //This check should go AFTER the info_stored_after_deletion collection is checked. Otherwise, users can fish for
            // phone numbers that have outstanding pending accounts.
            if (!find_pending_document) { //if pending account not found
                response->set_return_status(sms_verification::SMSVerificationResponse_Status_PENDING_ACCOUNT_NOT_FOUND);
                return;
            }

            const bsoncxx::document::view pending_account_document_view = find_pending_document->view();
            const std::string extracted_verification_code = extractFromBsoncxx_k_utf8(
                    pending_account_document_view,
                    pending_account_keys::VERIFICATION_CODE
            );

            if (verification_code != extracted_verification_code) { //if verification code does not match

                bsoncxx::stdx::optional<mongocxx::result::update> update_info_stored_after_deletion;
                try {
                    update_info_stored_after_deletion = info_stored_after_deletion_collection.update_one(
                        *callback_session,
                        document{}
                            << info_stored_after_deletion_keys::PHONE_NUMBER << phone_number
                        << finalize,
                        document{}
                            << "$inc" << open_document
                                << info_stored_after_deletion_keys::NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS << 1
                            << close_document
                        << finalize
                    );
                }
                catch (const mongocxx::logic_error& e) {

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), std::string(e.what()),
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME
                    );

                    response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                    return;
                }

                if(!update_info_stored_after_deletion
                    || update_info_stored_after_deletion->matched_count() < 1
                    || update_info_stored_after_deletion->modified_count() < 1
                ) {
                    const std::string error_message = "Maximum number of failed sms verification attempts were reached when a user was logging in."
                                                      "After than the update to the INFO_STORED_AFTER_DELETION_COLLECTION_NAME failed.";

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_message,
                            "failed_attempts", std::to_string(number_failed_sms_verification_attempts),
                            "phone_number", phone_number,
                            "matched_count", update_info_stored_after_deletion ? std::to_string(update_info_stored_after_deletion->matched_count()) : "-1",
                            "modified_count", update_info_stored_after_deletion ? std::to_string(update_info_stored_after_deletion->modified_count()) : "-1"
                    );
                }

                //Need to insert the pending account back into the collection if the user sent the wrong code.
                if (savePendingDocumentBackToCollection(
                        pending_account_document_view,
                        pending_collection,
                        callback_session)
                        ) {
                    if((number_failed_sms_verification_attempts + 1) >= general_values::MAXIMUM_NUMBER_FAILED_VERIFICATION_ATTEMPTS) {
                        response->set_return_status(sms_verification::SMSVerificationResponse_Status_VERIFICATION_ON_COOLDOWN);
                    } else {
                        response->set_return_status(sms_verification::SMSVerificationResponse_Status_INVALID_VERIFICATION_CODE);
                    }
                } else { //error occurred
                    response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                }

                return;
            }

            const std::chrono::milliseconds start_time_value = extractFromBsoncxx_k_date(
                    pending_account_document_view,
                    pending_account_keys::TIME_VERIFICATION_CODE_WAS_SENT
            ).value;

            if ((current_timestamp - start_time_value) >=
                general_values::TIME_UNTIL_VERIFICATION_CODE_EXPIRES) { //time has expired on verification code
                response->set_return_status(
                        sms_verification::SMSVerificationResponse_Status_VERIFICATION_CODE_EXPIRED);
                return;
            }

            const auto account_login_type(
                AccountLoginType(
                    extractFromBsoncxx_k_int32(
                        pending_account_document_view,
                        pending_account_keys::TYPE
                    )
                )
            );

            bsoncxx::stdx::optional<bsoncxx::document::value> user_account_document_value;
            try {

                //NOTE: The request->update_account_method() is used inside the below pipeline, and it is a bit odd.
                // When the user attempts to run loginFunction() with an installation id that is NOT inside the
                // INSTALLATION_IDS array, a signal will be sent back to the client. The client will then send in either
                // update_account_method==UPDATE_ACCOUNT or update_account_method==CREATE_NEW_ACCOUNT. However, just
                // because the user account was found does not mean the installation id was missing. This means there
                // is no hard guarantee that update_account_method will be set.
                const bsoncxx::builder::stream::document update_user_pipeline_document = buildSmsVerificationFindAndUpdateOneDoc(
                    installation_id,
                    current_timestamp,
                    account_login_type,
                    account_id,
                    request
                );

                mongocxx::pipeline update_user_account_pipeline;

                update_user_account_pipeline.add_fields(update_user_pipeline_document.view());

                mongocxx::options::find_one_and_update update_user_account_options;
                update_user_account_options.return_document(mongocxx::options::return_document::k_after);
                update_user_account_options.projection(
                    document{}
                        << "_id" << 1 //don't need to do this, but want to be explicit
                        << user_account_keys::INSTALLATION_IDS << 1
                    << finalize
                );

                //find if there is a user account associated with this pending account and update it if it is
                user_account_document_value = user_account_collection.find_one_and_update(
                        *callback_session,
                        document{}
                                << user_account_keys::PHONE_NUMBER << phone_number
                        << finalize,
                        update_user_account_pipeline,
                        update_user_account_options
                );
            }
            catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );
                response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                return;
            }

            if (user_account_document_value) { //user account existed and was updated

                const bsoncxx::document::view user_account_doc = user_account_document_value->view();
                const bsoncxx::array::view installation_ids = extractFromBsoncxx_k_array(
                        user_account_doc,
                        user_account_keys::INSTALLATION_IDS
                );

                //installation Ids was cleared to signal deleting this account
                if (installation_ids.empty()) {

                    const bsoncxx::oid user_account_oid = extractFromBsoncxx_k_oid(
                            user_account_doc,
                            "_id"
                    );

                    //this means the account is flagged for deletion and recreation
                    if (serverInternalDeleteAccount(
                            mongo_cpp_client,
                            accounts_db,
                            user_account_collection,
                            callback_session,
                            user_account_oid,
                            current_timestamp)
                    ) { //if delete was successful

                        int number_swipes_remaining = extractFromBsoncxx_k_int32(
                                info_stored_after_deletion_doc_view,
                                info_stored_after_deletion_keys::NUMBER_SWIPES_REMAINING
                        );

                        long long swipes_last_updated_time = extractFromBsoncxx_k_int64(
                                info_stored_after_deletion_doc_view,
                                info_stored_after_deletion_keys::SWIPES_LAST_UPDATED_TIME
                        );

                        const std::chrono::milliseconds time_sms_can_be_sent_again = extractFromBsoncxx_k_date(
                                info_stored_after_deletion_doc_view,
                                info_stored_after_deletion_keys::TIME_SMS_CAN_BE_SENT_AGAIN
                        ).value;

                        const std::chrono::milliseconds time_email_can_be_sent_again = extractFromBsoncxx_k_date(
                                info_stored_after_deletion_doc_view,
                                info_stored_after_deletion_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN
                        ).value;

                        const AccountCreationParameterPack parameter_pack{
                                current_timestamp,
                                installation_id,
                                account_id,
                                phone_number,
                                number_swipes_remaining,
                                swipes_last_updated_time,
                                time_sms_can_be_sent_again,
                                time_email_can_be_sent_again
                        };

                        if (createUserAccount(
                                accounts_db,
                                user_account_collection,
                                callback_session,
                                current_timestamp,
                                account_login_type,
                                parameter_pack)
                        ) {
                            response->set_return_status(sms_verification::SMSVerificationResponse_Status_SUCCESS);
                        } else {
                            response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                        }
                    } else { //if delete failed
                        //NOTE: if delete failed the error should be logged inside the function
                        response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                    }

                    return;
                }
                else { //installation Ids array is not empty

                    bool installation_id_exists_inside_array = false;

                    for (const auto& msg: installation_ids) {
                        if (msg.type() == bsoncxx::type::k_utf8) {
                            if (installation_id == msg.get_string().value.to_string()) {
                                installation_id_exists_inside_array = true;
                                break;
                            }
                        } else {
                            logElementError(
                                    __LINE__, __FILE__,
                                    bsoncxx::document::element(), user_account_doc,
                                    bsoncxx::type::k_array, user_account_keys::INSTALLATION_IDS,
                                    database_names::ACCOUNTS_DATABASE_NAME,
                                    collection_names::USER_ACCOUNTS_COLLECTION_NAME
                            );
                            continue;
                        }
                    }

                    if (!installation_id_exists_inside_array) {

                        if (request->installation_id_added_command() !=
                            sms_verification::SMSVerificationRequest_InstallationIdAddedCommand_UPDATE_ACCOUNT) {

                            //this means that
                            // 1) the installationId did not exist inside the array
                            // 2) CREATE_NEW_ACCOUNT was not passed (was checked for inside the aggregation pipeline and returns [] if it was)
                            // 3) UPDATE_ACCOUNT was also not passed
                            const std::string error_string = "If a new installation Id being added to the account, it should be passed with either UPDATE_ACCOUNT or CREATE_NEW_ACCOUNT.";
                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    std::optional<std::string>(), error_string,
                                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                    "Install_Id_array", makePrettyJson(installation_ids),
                                    "InstallId", installation_id
                            );
                            response->set_return_status(
                                    sms_verification::SMSVerificationResponse_Status_INVALID_UPDATE_ACCOUNT_METHOD_PASSED);
                            return;
                        }

                        //need to upsert the pending account back into the collection
                        if (savePendingDocumentBackToCollection(
                                pending_account_document_view,
                                pending_collection,
                                callback_session)
                        ) {
                            response->set_return_status(
                                    sms_verification::SMSVerificationResponse_Status_INCORRECT_BIRTHDAY);
                        } else { //error occurred
                            response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                        }

                        return;
                    }

                    response->set_return_status(sms_verification::SMSVerificationResponse_Status_SUCCESS);
                    return;
                }

            }
            else { //user account does not exist

                int number_swipes_remaining = extractFromBsoncxx_k_int32(
                        info_stored_after_deletion_doc_view,
                        info_stored_after_deletion_keys::NUMBER_SWIPES_REMAINING
                );

                long long swipes_last_updated_time = extractFromBsoncxx_k_int64(
                        info_stored_after_deletion_doc_view,
                        info_stored_after_deletion_keys::SWIPES_LAST_UPDATED_TIME
                );

                const std::chrono::milliseconds time_sms_can_be_sent_again = extractFromBsoncxx_k_date(
                        info_stored_after_deletion_doc_view,
                        info_stored_after_deletion_keys::TIME_SMS_CAN_BE_SENT_AGAIN
                ).value;

                const std::chrono::milliseconds time_email_can_be_sent_again = extractFromBsoncxx_k_date(
                        info_stored_after_deletion_doc_view,
                        info_stored_after_deletion_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN
                ).value;

                const AccountCreationParameterPack parameter_pack{
                        current_timestamp,
                        installation_id,
                        account_id,
                        phone_number,
                        number_swipes_remaining,
                        swipes_last_updated_time,
                        time_sms_can_be_sent_again,
                        time_email_can_be_sent_again
                };

                bsoncxx::oid extracted_user_account_oid;

                if (createUserAccount(
                        accounts_db,
                        user_account_collection,
                        callback_session,
                        current_timestamp,
                        account_login_type,
                        parameter_pack)
                        ) {
                    response->set_return_status(sms_verification::SMSVerificationResponse_Status_SUCCESS);
                } else {
                    response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
                }

                return;
            }
        } catch (const ErrorExtractingFromBsoncxx& e) {
            //NOTE: Error already stored here
            response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
            return;
        }
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        session.with_transaction(transaction_callback);
    } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
        std::cout << "Exception calling sMSVerificationImplementation() transaction.\n" << e.what() << '\n';
#endif
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_phone_number", phone_number,
                "user_account_id", account_id
        );
        response->set_return_status(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
        return;
    }

}

bool savePendingDocumentBackToCollection(
        const bsoncxx::document::view& pending_account_document_view,
        mongocxx::collection& pending_collection,
        mongocxx::client_session* session
) {
    if(session == nullptr) {
        const std::string error_string = "savePendingDocumentBackToCollection() was called when session was set to nullptr.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "pendingAccountDocumentView", pending_account_document_view
        );
        return false;
    }

    //need to upsert the pending account back into the collection if the user sent the
    // wrong code

    //NOTE: another way of doing this is to find the document, then check it to see if it
    // matches, then delete it if the info does match, however the user should almost always
    // get the correct verification code so find one and delete should end more efficient overall
    // also it allows the situation to be more atomic because only 1 thread can view the pending
    // document at a time

    bsoncxx::stdx::optional<mongocxx::result::insert_one> insert_pending_document;
    try {
        insert_pending_document = pending_collection.insert_one(
                *session,
                pending_account_document_view
        );
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME,
                "pending_doc", pending_account_document_view
        );
        return false;
    }

    if (!insert_pending_document || insert_pending_document->result().inserted_count() != 1) {
        const std::string error_string = "Failed to upsert pending document after document was just deleted from collection.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME,
                "pending_doc", pending_account_document_view
        );
        return false;
    }

    return true;
}