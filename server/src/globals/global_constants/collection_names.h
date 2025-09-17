//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

namespace collection_names {

    //mongoDB (ACCOUNTS_DATABASE_NAME) collections
    inline const std::string ADMIN_ACCOUNTS_COLLECTION_NAME = "admin_accounts";
    inline const std::string USER_ACCOUNTS_COLLECTION_NAME = "user_accounts";
    inline const std::string USER_ACCOUNT_STATISTICS_COLLECTION_NAME = "user_account_stats";
    inline const std::string PENDING_ACCOUNT_COLLECTION_NAME = "pending_account";
    inline const std::string USER_PICTURES_COLLECTION_NAME = "user_pictures";
    inline const std::string INFO_STORED_AFTER_DELETION_COLLECTION_NAME = "info_stored_after_delete";
    inline const std::string EMAIL_VERIFICATION_COLLECTION_NAME = "VerificationEmailInfo"; //putting this in Accounts database in case want to use $lookup
    inline const std::string ACCOUNT_RECOVERY_COLLECTION_NAME = "AccountRecoveryInfo"; //putting this in Accounts database in case want to use $lookup
    inline const std::string PRE_LOGIN_CHECKERS_COLLECTION_NAME = "pre_login_checkers";

#ifdef LG_TESTING
    inline std::string ACTIVITIES_INFO_COLLECTION_NAME = "activities_info";
    inline std::string ICONS_INFO_COLLECTION_NAME = "icons_info";
#else
    inline const std::string ACTIVITIES_INFO_COLLECTION_NAME = "activities_info";
    inline const std::string ICONS_INFO_COLLECTION_NAME = "icons_info";
#endif

    //mongoDB (ERRORS_DATABASE_NAME) collections
    //NOTE: loosely organizing these by function, so each function will be a collection
    //NOTE: not doing rigorously defined abbreviated names for error databases
    // because the names don't take up much memory
    inline const std::string FRESH_ERRORS_COLLECTION_NAME = "FreshErrors";
    inline const std::string HANDLED_ERRORS_COLLECTION_NAME = "HandledErrors";
    inline const std::string HANDLED_ERRORS_LIST_COLLECTION_NAME = "HandledErrorsList";

    //mongoDB (DELETED_DATABASE_NAME) collections
    inline const std::string DELETED_DELETED_ACCOUNTS_COLLECTION_NAME = "deleted_accounts";
    inline const std::string DELETED_USER_PICTURES_COLLECTION_NAME = "deleted_pictures";
    inline const std::string DELETED_CHAT_MESSAGE_PICTURES_COLLECTION_NAME = "deleted_corrupted_chat_room_pictures"; //this stored corrupted pictures

    //mongoDB (INFO_FOR_STATISTICS_DATABASE_NAME) collections
    inline const std::string MATCH_ALGORITHM_STATISTICS_COLLECTION_NAME = "MatchingAlgorithmStatistics"; //The raw results of an algorithm run.
    inline const std::string MATCH_ALGORITHM_RESULTS_COLLECTION_NAME = "MatchingAlgorithmResults"; //The organized results of an algorithm run.
    inline const std::string INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME = "IndividualMatchStatistic"; //Each document is the results of a match.
    inline const std::string USER_ACCOUNT_STATISTICS_DOCUMENTS_COMPLETED_COLLECTION_NAME = "UserAccountStatisticsDoc"; //Each document is specific to the user.

    //mongoDB (FEEDBACK_DATABASE_NAME) collections
    inline const std::string ACTIVITY_SUGGESTION_FEEDBACK_COLLECTION_NAME = "ActivitySuggestion";
    inline const std::string BUG_REPORT_FEEDBACK_COLLECTION_NAME = "BugReport";
    inline const std::string OTHER_SUGGESTION_FEEDBACK_COLLECTION_NAME = "OtherSuggestion";

    //mongoDB (REPORTS_DATABASE_NAME) collections
    inline const std::string OUTSTANDING_REPORTS_COLLECTION_NAME = "OutstandingReports";
    inline const std::string HANDLED_REPORTS_COLLECTION_NAME = "HandledReports";

    //mongoDB (CHAT_ROOMS_DATABASE_NAME) collections
    inline const std::string CHAT_ROOM_ID_ = "chat_room_"; //this will be followed by the chat room id
    inline const std::string CHAT_ROOM_INFO = "chat_room_info"; //all info for the chat rooms
    inline const std::string CHAT_MESSAGE_PICTURES_COLLECTION_NAME = "chat_message_pictures"; //store pictures for collection
}