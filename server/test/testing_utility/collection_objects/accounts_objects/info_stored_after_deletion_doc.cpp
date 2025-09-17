//
// Created by jeremiah on 6/3/22.
//

#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <user_pictures_keys.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>
#include <info_stored_after_deletion_keys.h>

#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this InfoStoredAfterDeletionDoc object to a document and saves it to the passed builder
void InfoStoredAfterDeletionDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    document_result
            << info_stored_after_deletion_keys::PHONE_NUMBER << phone_number
            << info_stored_after_deletion_keys::TIME_SMS_CAN_BE_SENT_AGAIN << time_sms_can_be_sent_again
            << info_stored_after_deletion_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << time_email_can_be_sent_again
            << info_stored_after_deletion_keys::COOL_DOWN_ON_SMS << cool_down_on_sms
            << info_stored_after_deletion_keys::NUMBER_SWIPES_REMAINING << number_swipes_remaining
            << info_stored_after_deletion_keys::SWIPES_LAST_UPDATED_TIME << swipes_last_updated_time
            << info_stored_after_deletion_keys::NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS << number_failed_sms_verification_attempts
            << info_stored_after_deletion_keys::FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME << failed_sms_verification_last_update_time;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool InfoStoredAfterDeletionDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection info_stored_after_delete_collection = accounts_db[collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            info_stored_after_delete_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = info_stored_after_delete_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool InfoStoredAfterDeletionDoc::getFromCollection(const std::string& phone_number) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection info_stored_after_delete_collection = accounts_db[collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = info_stored_after_delete_collection.find_one(document{}
            << info_stored_after_deletion_keys::PHONE_NUMBER << phone_number
        << finalize);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
        << "bsoncxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
        << e.what() << '\n'
        << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
        << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
        << e.what() << '\n'
        << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool InfoStoredAfterDeletionDoc::getFromCollection(const bsoncxx::oid& findOID) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection info_stored_after_delete_collection = accounts_db[collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = info_stored_after_delete_collection.find_one(document{}
                                                                               << "_id" << findOID
                                                                               << finalize);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool InfoStoredAfterDeletionDoc::saveInfoToDocument(
        const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool InfoStoredAfterDeletionDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        phone_number = extractFromBsoncxx_k_utf8(
                user_account_document,
                info_stored_after_deletion_keys::PHONE_NUMBER
        );

        time_sms_can_be_sent_again = extractFromBsoncxx_k_date(
                user_account_document,
                info_stored_after_deletion_keys::TIME_SMS_CAN_BE_SENT_AGAIN
        );

        time_email_can_be_sent_again = extractFromBsoncxx_k_date(
                user_account_document,
                info_stored_after_deletion_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN
        );

        cool_down_on_sms = extractFromBsoncxx_k_int32(
                user_account_document,
                info_stored_after_deletion_keys::COOL_DOWN_ON_SMS
        );

        number_swipes_remaining = extractFromBsoncxx_k_int32(
                user_account_document,
                info_stored_after_deletion_keys::NUMBER_SWIPES_REMAINING
        );

        swipes_last_updated_time = extractFromBsoncxx_k_int64(
                user_account_document,
                info_stored_after_deletion_keys::SWIPES_LAST_UPDATED_TIME
        );

        number_failed_sms_verification_attempts = extractFromBsoncxx_k_int32(
                user_account_document,
                info_stored_after_deletion_keys::NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS
        );

        failed_sms_verification_last_update_time = extractFromBsoncxx_k_int64(
                user_account_document,
                info_stored_after_deletion_keys::FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME
        );

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool InfoStoredAfterDeletionDoc::operator==(const InfoStoredAfterDeletionDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            phone_number,
            other.phone_number,
            "PHONE_NUMBER",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            time_sms_can_be_sent_again.value.count(),
            other.time_sms_can_be_sent_again.value.count(),
            "PHONE_NUMBER",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            time_email_can_be_sent_again.value.count(),
            other.time_email_can_be_sent_again.value.count(),
            "TIME_EMAIL_CAN_BE_SENT_AGAIN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            cool_down_on_sms,
            other.cool_down_on_sms,
            "COOL_DOWN_ON_SMS",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_swipes_remaining,
            other.number_swipes_remaining,
            "NUMBER_SWIPES_REMAINING",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            swipes_last_updated_time,
            other.swipes_last_updated_time,
            "SWIPES_LAST_UPDATED_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_failed_sms_verification_attempts,
            other.number_failed_sms_verification_attempts,
            "NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            failed_sms_verification_last_update_time,
            other.failed_sms_verification_last_update_time,
            "FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const InfoStoredAfterDeletionDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
