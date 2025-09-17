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
#include <admin_account_keys.h>

#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this AdminAccountDoc object to a document and saves it to the passed builder
void AdminAccountDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    document_result
            << admin_account_key::NAME << name
            << admin_account_key::PASSWORD << password
            << admin_account_key::PRIVILEGE_LEVEL << privilege_level
            << admin_account_key::LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS << last_time_extracted_age_gender_statistics
            << admin_account_key::LAST_TIME_EXTRACTED_REPORTS << last_time_extracted_reports
            << admin_account_key::LAST_TIME_EXTRACTED_BLOCKS << last_time_extracted_blocks
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY << last_time_extracted_feedback_activity
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER << last_time_extracted_feedback_other
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG << last_time_extracted_feedback_bug
            << admin_account_key::NUMBER_FEEDBACK_MARKED_AS_SPAM << number_feedback_marked_as_spam
            << admin_account_key::NUMBER_REPORTS_MARKED_AS_SPAM << number_reports_marked_as_spam;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool AdminAccountDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection admin_accounts_collection = accounts_db[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            admin_accounts_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = admin_accounts_collection.insert_one(insertDocument.view());

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

bool AdminAccountDoc::extractDocument(bsoncxx::document::value&& find_doc) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection admin_accounts_collection = accounts_db[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = admin_accounts_collection.find_one(find_doc.view());
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

bool AdminAccountDoc::getFromCollection(const std::string& admin_name, const std::string& admin_password) {
    return extractDocument(
            document{}
                    << admin_account_key::NAME << admin_name
                    << admin_account_key::PASSWORD << admin_password
                << finalize
    );
}

bool AdminAccountDoc::getFromCollection(const bsoncxx::oid& findOID) {
    return extractDocument(
            document{}
                    << "_id" << findOID
                    << finalize
    );
}


bool AdminAccountDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

bool AdminAccountDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                user_account_document,
                "_id"
        );

        name = extractFromBsoncxx_k_utf8(
                user_account_document,
                admin_account_key::NAME
        );

        password = extractFromBsoncxx_k_utf8(
                user_account_document,
                admin_account_key::PASSWORD
        );

        privilege_level = AdminLevelEnum(
                extractFromBsoncxx_k_int32(
                        user_account_document,
                        admin_account_key::PRIVILEGE_LEVEL
                )
        );

        last_time_extracted_age_gender_statistics = extractFromBsoncxx_k_date(
                user_account_document,
                admin_account_key::LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS
        );


        last_time_extracted_reports = extractFromBsoncxx_k_date(
                user_account_document,
                admin_account_key::LAST_TIME_EXTRACTED_REPORTS
        );

        last_time_extracted_blocks = extractFromBsoncxx_k_date(
                user_account_document,
                admin_account_key::LAST_TIME_EXTRACTED_BLOCKS
        );

        last_time_extracted_feedback_activity = extractFromBsoncxx_k_date(
                user_account_document,
                admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY
        );

        last_time_extracted_feedback_other = extractFromBsoncxx_k_date(
                user_account_document,
                admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER
        );

        last_time_extracted_feedback_bug = extractFromBsoncxx_k_date(
                user_account_document,
                admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG
        );

        number_feedback_marked_as_spam = extractFromBsoncxx_k_int64(
                user_account_document,
                admin_account_key::NUMBER_FEEDBACK_MARKED_AS_SPAM
        );

        number_reports_marked_as_spam = extractFromBsoncxx_k_int64(
                user_account_document,
                admin_account_key::NUMBER_REPORTS_MARKED_AS_SPAM
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

bool AdminAccountDoc::operator==(const AdminAccountDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            name,
            other.name,
            "NAME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            password,
            other.password,
            "PASSWORD",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            privilege_level,
            other.privilege_level,
            "PRIVILEGE_LEVEL",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            last_time_extracted_age_gender_statistics.value.count(),
            other.last_time_extracted_age_gender_statistics.value.count(),
            "LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            last_time_extracted_reports.value.count(),
            other.last_time_extracted_reports.value.count(),
            "LAST_TIME_EXTRACTED_REPORTS",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            last_time_extracted_blocks.value.count(),
            other.last_time_extracted_blocks.value.count(),
            "LAST_TIME_EXTRACTED_BLOCKS",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            last_time_extracted_feedback_activity.value.count(),
            other.last_time_extracted_feedback_activity.value.count(),
            "LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            last_time_extracted_feedback_other.value.count(),
            other.last_time_extracted_feedback_other.value.count(),
            "LAST_TIME_EXTRACTED_FEEDBACK_OTHER",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            last_time_extracted_feedback_bug.value.count(),
            other.last_time_extracted_feedback_bug.value.count(),
            "LAST_TIME_EXTRACTED_FEEDBACK_BUG",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            number_feedback_marked_as_spam,
            other.number_feedback_marked_as_spam,
            "NUMBER_FEEDBACK_MARKED_AS_SPAM",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkForEquality(
            number_reports_marked_as_spam,
            other.number_reports_marked_as_spam,
            "NUMBER_REPORTS_MARKED_AS_SPAM",
            OBJECT_CLASS_NAME,
            return_value
            );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const AdminAccountDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
