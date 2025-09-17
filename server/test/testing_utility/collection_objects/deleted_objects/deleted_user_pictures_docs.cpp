//
// Created by jeremiah on 6/1/22.
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
#include <user_account_statistics_documents_completed_keys.h>
#include <deleted_user_pictures_keys.h>

#include "deleted_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this UserAccountStatisticsDoc object to a document and saves it to the passed builder
void DeletedUserPictureDoc::convertToDocument(
        bsoncxx::builder::stream::document& document_result,
        bool skip_long_strings
) const {
    UserPictureDoc::convertToDocument(document_result, skip_long_strings);

    if (references_removed_after_delete != nullptr) {
        document_result
                << deleted_user_pictures_keys::REFERENCES_REMOVED_AFTER_DELETE << *references_removed_after_delete;
    }

    document_result
            << deleted_user_pictures_keys::TIMESTAMP_REMOVED << timestamp_removed
            << deleted_user_pictures_keys::REASON_REMOVED << reason_removed;

    if (admin_name != nullptr) {
        document_result
                << deleted_user_pictures_keys::ADMIN_NAME << *admin_name;
    }
}

bool DeletedUserPictureDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
    mongocxx::collection deleted_user_pictures_collection = deleted_db[collection_names::DELETED_USER_PICTURES_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument, false);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            deleted_user_pictures_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = deleted_user_pictures_collection.insert_one(insertDocument.view());

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

bool DeletedUserPictureDoc::getFromCollection() {
    return getFromCollection(document{} << finalize);
}

bool DeletedUserPictureDoc::getFromCollection(const bsoncxx::oid& findOID) {
    return getFromCollection(
            document{}
                    << "_id" << findOID
            << finalize
    );
}

bool DeletedUserPictureDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
    mongocxx::collection deleted_user_pictures_collection = deleted_db[collection_names::DELETED_USER_PICTURES_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = deleted_user_pictures_collection.find_one(find_doc);
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
DeletedUserPictureDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}


bool DeletedUserPictureDoc::convertDocumentToClass(const bsoncxx::document::view& user_account_document) {

    UserPictureDoc::convertDocumentToClass(user_account_document);

    try {

        auto references_removed_after_delete_ele = user_account_document[deleted_user_pictures_keys::REFERENCES_REMOVED_AFTER_DELETE];

        if (references_removed_after_delete_ele) {
            references_removed_after_delete = std::make_unique<bool>(
                    references_removed_after_delete_ele.get_bool().value);
        } else {
            references_removed_after_delete = nullptr;
        }

        timestamp_removed = extractFromBsoncxx_k_date(
                user_account_document,
                deleted_user_pictures_keys::TIMESTAMP_REMOVED
        );

        reason_removed = ReasonPictureDeleted(
                extractFromBsoncxx_k_int32(
                        user_account_document,
                        deleted_user_pictures_keys::REASON_REMOVED
                )
        );

        auto admin_name_ele = user_account_document[deleted_user_pictures_keys::ADMIN_NAME];

        if (admin_name_ele) {
            admin_name = std::make_unique<std::string>(admin_name_ele.get_string().value.to_string());
        } else {
            admin_name = nullptr;
        }

    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in UserAccountDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
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

bool DeletedUserPictureDoc::operator==(const DeletedUserPictureDoc& other) const {
    bool return_value = UserPictureDoc::operator==(other);

    if (references_removed_after_delete == nullptr
        || other.references_removed_after_delete == nullptr) {

        checkForEquality(
                references_removed_after_delete,
                other.references_removed_after_delete,
                "REFERENCES_REMOVED_AFTER_DELETE(ptr)",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                *references_removed_after_delete,
                *other.references_removed_after_delete,
                "REFERENCES_REMOVED_AFTER_DELETE",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    checkForEquality(
            timestamp_removed.value.count(),
            other.timestamp_removed.value.count(),
            "TIMESTAMP_REMOVED",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            reason_removed,
            other.reason_removed,
            "REASON_REMOVED",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (admin_name == nullptr
        || other.admin_name == nullptr) {

        checkForEquality(
                admin_name,
                other.admin_name,
                "ADMIN_NAME(ptr)",
                OBJECT_CLASS_NAME,
                return_value
        );
    } else {
        checkForEquality(
                *admin_name,
                *other.admin_name,
                "ADMIN_NAME",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const DeletedUserPictureDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result, true);
    o << makePrettyJson(document_result.view());
    return o;
}
