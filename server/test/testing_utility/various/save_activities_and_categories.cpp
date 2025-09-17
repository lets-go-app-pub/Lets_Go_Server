//
// Created by jeremiah on 10/4/22.
//

#include <mongocxx/collection.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include "collection_names.h"
#include "activities_info_keys.h"
#include "save_activities_and_categories.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void saveActivitiesAndCategories(
        mongocxx::database& accounts_db,
        google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>* mutable_categories,
        google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>* mutable_activities
        ) {

    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

    auto activities_find_result = activities_info_collection.find_one(
            document{}
                    << "_id" << activities_info_keys::ID
                    << finalize
    );

    const bsoncxx::document::view activities_doc_view = activities_find_result->view();

    const bsoncxx::array::view categories_array = activities_doc_view[activities_info_keys::CATEGORIES].get_array().value;
    const bsoncxx::array::view activities_array = activities_doc_view[activities_info_keys::ACTIVITIES].get_array().value;

    int categories_index = 0;
    for(const auto& doc : categories_array) {
        const bsoncxx::document::view category_doc = doc.get_document().value;

        auto* category = mutable_categories->Add();

        category->set_index(categories_index);
        category->set_order_number(category_doc[activities_info_keys::categories::ORDER_NUMBER].get_double().value);
        category->set_display_name(category_doc[activities_info_keys::categories::DISPLAY_NAME].get_string().value.to_string());
        category->set_icon_display_name(category_doc[activities_info_keys::categories::ICON_DISPLAY_NAME].get_string().value.to_string());
        category->set_min_age(category_doc[activities_info_keys::categories::MIN_AGE].get_int32().value);
        category->set_color(category_doc[activities_info_keys::categories::COLOR].get_string().value.to_string());
        categories_index++;
    }

    int activities_index = 0;
    for(const auto& doc : activities_array) {
        const bsoncxx::document::view activity_doc = doc.get_document().value;

        auto* activity = mutable_activities->Add();

        activity->set_index(activities_index);
        activity->set_display_name(activity_doc[activities_info_keys::activities::DISPLAY_NAME].get_string().value.to_string());
        activity->set_icon_display_name(activity_doc[activities_info_keys::activities::ICON_DISPLAY_NAME].get_string().value.to_string());
        activity->set_min_age(activity_doc[activities_info_keys::activities::MIN_AGE].get_int32().value);
        activity->set_category_index(activity_doc[activities_info_keys::activities::CATEGORY_INDEX].get_int32().value);
        activity->set_icon_index(activity_doc[activities_info_keys::activities::ICON_INDEX].get_int32().value);

        activities_index++;
    }
}