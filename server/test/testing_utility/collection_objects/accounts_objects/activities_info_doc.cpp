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
#include <activities_info_keys.h>

#include "account_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this ActivitiesInfoDoc object to a document and saves it to the passed builder
void ActivitiesInfoDoc::convertToDocument(
        bsoncxx::builder::stream::document& document_result
) const {

    bsoncxx::builder::basic::array categories_doc;
    bsoncxx::builder::basic::array activities_doc;

    for (const CategoryDoc& category : categories) {
        categories_doc.append(
                document{}
                        << activities_info_keys::categories::DISPLAY_NAME << category.display_name
                        << activities_info_keys::categories::ICON_DISPLAY_NAME << category.icon_display_name
                        << activities_info_keys::categories::ORDER_NUMBER << category.order_number
                        << activities_info_keys::categories::MIN_AGE << category.min_age
                        << activities_info_keys::categories::STORED_NAME << category.stored_name
                        << activities_info_keys::categories::COLOR << category.color
                        << finalize
        );
    }

    for (const ActivityDoc& activity : activities) {
        activities_doc.append(
                document{}
                        << activities_info_keys::activities::DISPLAY_NAME << activity.display_name
                        << activities_info_keys::activities::ICON_DISPLAY_NAME << activity.icon_display_name
                        << activities_info_keys::activities::MIN_AGE << activity.min_age
                        << activities_info_keys::activities::STORED_NAME << activity.stored_name
                        << activities_info_keys::activities::CATEGORY_INDEX << activity.category_index
                        << activities_info_keys::activities::ICON_INDEX << activity.icon_index
                        << finalize
        );
    }

    document_result
            << "_id" << activities_info_keys::ID
            << activities_info_keys::CATEGORIES << categories_doc
            << activities_info_keys::ACTIVITIES << activities_doc;
}

bool ActivitiesInfoDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {
        if (!id.empty()) {
            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            activities_info_collection.update_one(
                    document{}
                            << "_id" << activities_info_keys::ID
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions
            );
        } else {
            activities_info_collection.insert_one(insertDocument.view());

            id = activities_info_keys::ID;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in ActivitiesInfoDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in ActivitiesInfoDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool ActivitiesInfoDoc::getFromCollection() {
    return getFromCollection(
            document{}
                    << "_id" << activities_info_keys::ID
            << finalize
    );
}

bool ActivitiesInfoDoc::getFromCollection(const bsoncxx::oid& find_oid) {
    return getFromCollection(
            document{}
                    << "_id" << find_oid
            << finalize
    );
}

bool ActivitiesInfoDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = activities_info_collection.find_one(find_doc);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in ActivitiesInfoDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in ActivitiesInfoDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool ActivitiesInfoDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        id = activities_info_keys::ID;

        return convertDocumentToClass(user_doc_view);
    } else {
        id.clear();
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function ActivitiesInfoDoc::saveInfoToDocument\n";
        return false;
    }
}

bool ActivitiesInfoDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& user_account_document) {
    try {
        bsoncxx::array::view categories_doc = extractFromBsoncxx_k_array(
                user_account_document,
                activities_info_keys::CATEGORIES
        );

        bsoncxx::array::view activities_doc = extractFromBsoncxx_k_array(
                user_account_document,
                activities_info_keys::ACTIVITIES
        );

        categories.clear();
        for (const auto& ele : categories_doc) {
            bsoncxx::document::view category_doc = ele.get_document().value;

            categories.emplace_back(
                    CategoryDoc(
                            category_doc[activities_info_keys::categories::DISPLAY_NAME].get_string().value.to_string(),
                            category_doc[activities_info_keys::categories::ICON_DISPLAY_NAME].get_string().value.to_string(),
                            category_doc[activities_info_keys::categories::ORDER_NUMBER].get_double().value,
                            category_doc[activities_info_keys::categories::MIN_AGE].get_int32().value,
                            category_doc[activities_info_keys::categories::STORED_NAME].get_string().value.to_string(),
                            category_doc[activities_info_keys::categories::COLOR].get_string().value.to_string()
                    )
            );
        }

        activities.clear();
        for (const auto& ele : activities_doc) {
            bsoncxx::document::view activity_doc = ele.get_document().value;

            activities.emplace_back(
                    ActivityDoc(
                            activity_doc[activities_info_keys::activities::DISPLAY_NAME].get_string().value.to_string(),
                            activity_doc[activities_info_keys::activities::ICON_DISPLAY_NAME].get_string().value.to_string(),
                            activity_doc[activities_info_keys::activities::MIN_AGE].get_int32().value,
                            activity_doc[activities_info_keys::activities::STORED_NAME].get_string().value.to_string(),
                            activity_doc[activities_info_keys::activities::CATEGORY_INDEX].get_int32().value,
                            activity_doc[activities_info_keys::activities::ICON_INDEX].get_int32().value
                    )
            );
        }
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in ActivitiesInfoDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool ActivitiesInfoDoc::operator==(const ActivitiesInfoDoc& other) const {
    bool return_value = true;

    if (other.categories.size() == categories.size()) {
        for (int i = 0; i < (int)categories.size(); i++) {

            checkForEquality(
                    categories[i].display_name,
                    other.categories[i].display_name,
                    "CATEGORIES.display_name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    categories[i].icon_display_name,
                    other.categories[i].icon_display_name,
                    "CATEGORIES.icon_display_name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    categories[i].order_number,
                    other.categories[i].order_number,
                    "CATEGORIES.order_number",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    categories[i].min_age,
                    other.categories[i].min_age,
                    "CATEGORIES.min_age",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    categories[i].stored_name,
                    other.categories[i].stored_name,
                    "CATEGORIES.stored_name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    categories[i].color,
                    other.categories[i].color,
                    "CATEGORIES.color",
                    OBJECT_CLASS_NAME,
                    return_value
            );
        }
    } else {
        checkForEquality(
                categories.size(),
                other.categories.size(),
                "CATEGORIES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (other.activities.size() == activities.size()) {
        for (int i = 0; i < (int)activities.size(); i++) {

            checkForEquality(
                    activities[i].display_name,
                    other.activities[i].display_name,
                    "ACTIVITIES.display_name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    activities[i].icon_display_name,
                    other.activities[i].icon_display_name,
                    "ACTIVITIES.icon_display_name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    activities[i].min_age,
                    other.activities[i].min_age,
                    "ACTIVITIES.min_age",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    activities[i].stored_name,
                    other.activities[i].stored_name,
                    "ACTIVITIES.stored_name",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    activities[i].category_index,
                    other.activities[i].category_index,
                    "ACTIVITIES.category_index",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    activities[i].icon_index,
                    other.activities[i].icon_index,
                    "ACTIVITIES.icon_index",
                    OBJECT_CLASS_NAME,
                    return_value
            );

        }
    } else {
        checkForEquality(
                activities.size(),
                other.activities.size(),
                "ACTIVITIES.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const ActivitiesInfoDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
