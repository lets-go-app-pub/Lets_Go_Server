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
#include <email_verification_keys.h>

#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this UserPictureDoc object to a document and saves it to the passed builder
void EmailVerificationDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    document_result
            << email_verification_keys::USER_ACCOUNT_REFERENCE << user_account_reference
            << email_verification_keys::VERIFICATION_CODE << verification_code
            << email_verification_keys::TIME_VERIFICATION_CODE_GENERATED << time_verification_code_generated
            << email_verification_keys::ADDRESS_BEING_VERIFIED << address_being_verified;

    //email_verification_keys::USER_ACCOUNT_REFERENCE is _id, so current_object_oid is not needed
}

bool EmailVerificationDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection email_verification_collection = accounts_db[collection_names::EMAIL_VERIFICATION_COLLECTION_NAME];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (user_account_reference.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            email_verification_collection.update_one(
                    document{}
                    << "_id" << user_account_reference
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);

            current_object_oid = user_account_reference;
        } else {

            auto idVar = email_verification_collection.insert_one(insertDocument.view());

            user_account_reference = idVar->inserted_id().get_oid().value;
            current_object_oid = user_account_reference;
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

bool EmailVerificationDoc::getFromCollection(const bsoncxx::oid& findOID) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection email_verification_collection = accounts_db[collection_names::EMAIL_VERIFICATION_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = email_verification_collection.find_one(document{}
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

bool
EmailVerificationDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool EmailVerificationDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {

        user_account_reference = extractFromBsoncxx_k_oid(
                user_account_document,
                email_verification_keys::USER_ACCOUNT_REFERENCE
        );

        current_object_oid = user_account_reference;

        verification_code = extractFromBsoncxx_k_utf8(
                user_account_document,
                email_verification_keys::VERIFICATION_CODE
        );

        time_verification_code_generated = extractFromBsoncxx_k_date(
                user_account_document,
                email_verification_keys::TIME_VERIFICATION_CODE_GENERATED
        );

        address_being_verified = extractFromBsoncxx_k_utf8(
                user_account_document,
                email_verification_keys::ADDRESS_BEING_VERIFIED
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

bool EmailVerificationDoc::operator==(const EmailVerificationDoc& other) const {
    bool return_value = true;

    checkForEquality(
            user_account_reference.to_string(),
            other.user_account_reference.to_string(),
            "USER_ACCOUNT_REFERENCE",
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
            time_verification_code_generated.value.count(),
            other.time_verification_code_generated.value.count(),
            "TIME_VERIFICATION_CODE_GENERATED",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            address_being_verified,
            other.address_being_verified,
            "ADDRESS_BEING_VERIFIED",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const EmailVerificationDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
