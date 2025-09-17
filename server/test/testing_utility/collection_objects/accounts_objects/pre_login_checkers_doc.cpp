//
// Created by jeremiah on 3/21/23.
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

#include "account_objects.h"
#include "pre_login_checkers_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this PreLoginCheckersDoc object to a document and saves it to the passed builder
void PreLoginCheckersDoc::convertToDocument(
        bsoncxx::v_noabi::builder::stream::document& document_result
) const {
    document_result
            << pre_login_checkers_keys::INSTALLATION_ID << installation_id
            << pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT << number_sms_verification_messages_sent
            << pre_login_checkers_keys::SMS_VERIFICATION_MESSAGES_LAST_UPDATE_TIME << sms_verification_messages_last_update_time;
}

bool PreLoginCheckersDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::PRE_LOGIN_CHECKERS_COLLECTION_NAME];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        mongocxx::options::update updateOptions;
        updateOptions.upsert(true);

        user_pictures_collection.update_one(
                document{}
                        << pre_login_checkers_keys::INSTALLATION_ID << installation_id
                        << finalize,
                document{}
                        << "$set" << insertDocument.view()
                        << finalize,
                updateOptions);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in PreLoginCheckersDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in PreLoginCheckersDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool PreLoginCheckersDoc::getFromCollection() {
    return getFromCollection(document{} << finalize);
}

bool PreLoginCheckersDoc::getFromCollection(const std::string& _installation_id) {
    return getFromCollection(
        document{}
            << pre_login_checkers_keys::INSTALLATION_ID << _installation_id
        << finalize
    );
}

bool PreLoginCheckersDoc::getFromCollection(const bsoncxx::oid& find_oid [[maybe_unused]]) {
    return false;
}

bool PreLoginCheckersDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::PRE_LOGIN_CHECKERS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = user_pictures_collection.find_one(find_doc);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in PreLoginCheckersDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in PreLoginCheckersDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool PreLoginCheckersDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function PreLoginCheckersDoc::saveInfoToDocument\n";
        return false;
    }
}

bool PreLoginCheckersDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        installation_id = extractFromBsoncxx_k_utf8(
                user_account_document,
                pre_login_checkers_keys::INSTALLATION_ID
        );

        number_sms_verification_messages_sent = extractFromBsoncxx_k_int32(
                user_account_document,
                pre_login_checkers_keys::NUMBER_SMS_VERIFICATION_MESSAGES_SENT
        );

        sms_verification_messages_last_update_time = extractFromBsoncxx_k_int64(
                user_account_document,
                pre_login_checkers_keys::SMS_VERIFICATION_MESSAGES_LAST_UPDATE_TIME
        );
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in PreLoginCheckersDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool PreLoginCheckersDoc::operator==(const PreLoginCheckersDoc& other) const {
    bool return_value = true;

    checkForEquality(
            installation_id,
            other.installation_id,
            "INSTALLATION_ID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            number_sms_verification_messages_sent,
            other.number_sms_verification_messages_sent,
            "NUMBER_SMS_VERIFICATION_MESSAGES_SENT",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            sms_verification_messages_last_update_time,
            other.sms_verification_messages_last_update_time,
            "SMS_VERIFICATION_MESSAGES_LAST_UPDATE_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const PreLoginCheckersDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
