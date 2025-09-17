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
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>
#include <account_recovery_keys.h>

#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this AccountRecoveryDoc object to a document and saves it to the passed builder
void AccountRecoveryDoc::convertToDocument(
        bsoncxx::builder::stream::document& document_result
        ) const {

    document_result
        << account_recovery_keys::VERIFICATION_CODE << verification_code
        << account_recovery_keys::PHONE_NUMBER << phone_number
        << account_recovery_keys::TIME_VERIFICATION_CODE_GENERATED << time_verification_code_generated
        << account_recovery_keys::NUMBER_ATTEMPTS << number_attempts
        << account_recovery_keys::USER_ACCOUNT_OID << user_account_oid;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
        << "_id" << current_object_oid;
    }
}

bool AccountRecoveryDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_recovery_collection = accounts_db[collection_names::ACCOUNT_RECOVERY_COLLECTION_NAME];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            user_recovery_collection.update_one(
                    document{}
                    << "_id" << current_object_oid
                    << finalize,
                    document{}
                    << "$set" << insertDocument.view()
                    << finalize,
                    updateOptions);
        } else {

            auto idVar = user_recovery_collection.insert_one(insertDocument.view());

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

bool AccountRecoveryDoc::getFromCollection(const std::string& _phone_number) {
    bsoncxx::builder::stream::document find_doc;

    find_doc
        << account_recovery_keys::PHONE_NUMBER << _phone_number;

    return getFromCollection(find_doc);
}

bool AccountRecoveryDoc::getFromCollection(const bsoncxx::oid& findOID) {
    bsoncxx::builder::stream::document find_doc;

    find_doc
            << "_id" << findOID;

    return getFromCollection(find_doc);
}

bool AccountRecoveryDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_recovery_collection = accounts_db[collection_names::ACCOUNT_RECOVERY_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = user_recovery_collection.find_one(find_doc);
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

bool AccountRecoveryDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool AccountRecoveryDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
                );

        verification_code = extractFromBsoncxx_k_utf8(
                user_account_document,
                account_recovery_keys::VERIFICATION_CODE
                );

        phone_number = extractFromBsoncxx_k_utf8(
                user_account_document,
                account_recovery_keys::PHONE_NUMBER
                );

        time_verification_code_generated = extractFromBsoncxx_k_date(
                user_account_document,
                account_recovery_keys::TIME_VERIFICATION_CODE_GENERATED
                );

        number_attempts = extractFromBsoncxx_k_int32(
                user_account_document,
                account_recovery_keys::NUMBER_ATTEMPTS
                );

        user_account_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                account_recovery_keys::USER_ACCOUNT_OID
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

bool AccountRecoveryDoc::operator==(const AccountRecoveryDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            verification_code,
            other.verification_code,
            "VERIFICATION_CODE",
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
            time_verification_code_generated.value.count(),
            other.time_verification_code_generated.value.count(),
            "TIME_VERIFICATION_CODE_GENERATED",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            number_attempts,
            other.number_attempts,
            "NUMBER_ATTEMPTS",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            user_account_oid.to_string(),
            other.user_account_oid.to_string(),
            "USER_ACCOUNT_OID",
            OBJECT_CLASS_NAME,
            return_value
            );


    return return_value;
}

std::ostream& operator<<(std::ostream& o, const AccountRecoveryDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
