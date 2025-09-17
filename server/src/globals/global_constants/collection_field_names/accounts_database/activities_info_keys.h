//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (ACTIVITIES_INFO_COLLECTION_NAME)
namespace activities_info_keys {
    //A single document exists in this collection with the _id field set to ID. The arrays CATEGORIES and ACTIVITIES
    // contain each category and activity.
    inline const std::string ID = "singled_iD"; //string; this will be the _id inside the collection of this document
    inline const std::string CATEGORIES = "all_categories"; //array of documents; each element represents a single category, uses SHARED_MIN_AGE_KEY & COLOR
    inline const std::string ACTIVITIES = "all_activities"; //array of documents; each element represents a single activity, uses SHARED_MIN_AGE_KEY, CATEGORY_INDEX & ICON_INDEX

    inline const std::string SHARED_DISPLAY_NAME_KEY = "display_name"; //string; name set up for displaying as text NOT for the icon.
    inline const std::string SHARED_ICON_DISPLAY_NAME_KEY = "icon_display_name"; //string; name formatted for displaying below the icon
    inline const std::string SHARED_STORED_NAME_KEY = "store_name"; //string; name with some conversions done for comparability purposes
    inline const std::string SHARED_MIN_AGE_KEY = "min_age"; //int32; minimum age allowed to access this category inclusive (ex: 18 would mean 18+ can see it); this is also used to 'delete' an activity by setting it over HIGHEST_ALLOWED_AGE

    //namespace contains keys for ACTIVITIES documents
    namespace activities {
        inline const std::string DISPLAY_NAME = SHARED_DISPLAY_NAME_KEY; //string; name of the activity
        inline const std::string ICON_DISPLAY_NAME = SHARED_ICON_DISPLAY_NAME_KEY; //string; name of the activity
        inline const std::string MIN_AGE = SHARED_MIN_AGE_KEY; //int32; minimum age allowed to access this category inclusive (ex: 18 would mean 18+ can see it); this is also used to 'delete' an activity by setting it over HIGHEST_ALLOWED_AGE
        inline const std::string STORED_NAME = SHARED_STORED_NAME_KEY ; //string; (read SHARED_STORED_NAME_KEY comment)
        inline const std::string CATEGORY_INDEX = "category_index"; //int32; index of the category this activity 'belongs' to
        inline const std::string ICON_INDEX = "icon_index"; //int32; index of the icon this uses
    }

    //namespace contains keys for CATEGORIES documents
    namespace categories {
        inline const std::string DISPLAY_NAME = SHARED_DISPLAY_NAME_KEY; //string; name of the category
        inline const std::string ICON_DISPLAY_NAME = SHARED_ICON_DISPLAY_NAME_KEY; //string; name of the category
        inline const std::string ORDER_NUMBER = "order_num"; //double; number that will be used for sorting categories on the client (sorted by ascending)
        inline const std::string MIN_AGE = SHARED_MIN_AGE_KEY; //int32; minimum age allowed to access this category inclusive (ex: 18 would mean 18+ can see it); this is also used to 'delete' an activity by setting it over HIGHEST_ALLOWED_AGE
        inline const std::string STORED_NAME = SHARED_STORED_NAME_KEY; //string; (read SHARED_STORED_NAME_KEY comment)
        inline const std::string COLOR = "color"; //string; hex color code for this category
    }

}