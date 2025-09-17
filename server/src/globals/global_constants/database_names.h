//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

namespace database_names {

#ifdef LG_TESTING
    inline const std::string ACCOUNTS_DATABASE_NAME = "TestingAccounts";
#else
    inline const std::string ACCOUNTS_DATABASE_NAME = "Accounts";
#endif
#ifdef LG_TESTING
    inline const std::string ERRORS_DATABASE_NAME = "TestingERRORS";
#else
    inline const std::string ERRORS_DATABASE_NAME = "ERRORS";
#endif

#ifdef LG_TESTING
    inline const std::string DELETED_DATABASE_NAME = "TestingDeletedInfo";
#else
    inline const std::string DELETED_DATABASE_NAME = "DeletedInfo";
#endif

#ifdef LG_TESTING
    inline const std::string INFO_FOR_STATISTICS_DATABASE_NAME = "TestingInfoForStatistics";
#else
    inline const std::string INFO_FOR_STATISTICS_DATABASE_NAME = "InfoForStatistics";
#endif

#ifdef LG_TESTING
    inline const std::string FEEDBACK_DATABASE_NAME = "TestingFeedback";
#else
    inline const std::string FEEDBACK_DATABASE_NAME = "Feedback";
#endif

#ifdef LG_TESTING
    inline const std::string REPORTS_DATABASE_NAME = "TestingReports";
#else
    inline const std::string REPORTS_DATABASE_NAME = "Reports";
#endif

#ifdef LG_TESTING
    inline const std::string CHAT_ROOMS_DATABASE_NAME = "TestingChatRooms";
#else
    inline const std::string CHAT_ROOMS_DATABASE_NAME = "ChatRooms";
#endif

}