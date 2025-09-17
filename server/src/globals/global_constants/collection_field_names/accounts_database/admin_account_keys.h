//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (ADMIN_ACCOUNTS_COLLECTION_NAME)
namespace admin_account_key {
    inline const std::string NAME = "name"; //string; name for this admin
    inline const std::string PASSWORD = "pass"; //string; password for this admin
    inline const std::string PRIVILEGE_LEVEL = "privilege_level"; //int32; follows AdminLevelEnum inside AdminLevelEnum.proto

    inline const std::string LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS = "lst_age_gender"; //MongoDB Date; last time this user extracted age gender statistics

    inline const std::string LAST_TIME_EXTRACTED_REPORTS = "lst_reports"; //MongoDB Date; last time this user extracted reports
    inline const std::string LAST_TIME_EXTRACTED_BLOCKS = "lst_blocks"; //MongoDB Date; last time this user extracted blocks

    inline const std::string LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY = "lst_feedback_activity"; //MongoDB Date; last time this user extracted activity type feedback
    inline const std::string LAST_TIME_EXTRACTED_FEEDBACK_OTHER = "lst_feedback_other"; //MongoDB Date; last time this user extracted other type feedback
    inline const std::string LAST_TIME_EXTRACTED_FEEDBACK_BUG = "lst_feedback_bug"; //MongoDB Date; last time this user extracted bug report type feedback

    inline const std::string NUMBER_FEEDBACK_MARKED_AS_SPAM = "num_feedback_marked_spam"; //int64; number of times the admin pushed 'mark as spam' button for feedback
    inline const std::string NUMBER_REPORTS_MARKED_AS_SPAM = "num_reports_marked_spam"; //int64; number of times the admin pushed 'mark as spam' button for reports
}