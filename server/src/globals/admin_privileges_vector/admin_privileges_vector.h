//
// Created by jeremiah on 8/28/21.
//

#pragma once

#include <AdminLevelEnum.grpc.pb.h>
#include <store_mongoDB_error_and_exception.h>
#include "database_names.h"
#include "collection_names.h"

class AdminPrivilegesHolder {
    //AdminPrivileges is a .proto message inside AdminLevelEnum.proto
    std::vector<AdminPrivileges> admin_privileges = std::vector<AdminPrivileges>{AdminLevelEnum_ARRAYSIZE};

public:
    AdminPrivilegesHolder() {
        //NO_ADMIN_ACCESS; this is returned if an invalid value is sent to []
        admin_privileges[NO_ADMIN_ACCESS];

        //PRIMARY_DEVELOPER
        admin_privileges[PRIMARY_DEVELOPER].set_view_matching_activity_statistics(true);
        admin_privileges[PRIMARY_DEVELOPER].set_view_age_gender_statistics(true);
        admin_privileges[PRIMARY_DEVELOPER].set_view_activities_and_categories(true);
        admin_privileges[PRIMARY_DEVELOPER].set_update_activities_and_categories(true);
        admin_privileges[PRIMARY_DEVELOPER].set_update_icons(true);
        admin_privileges[PRIMARY_DEVELOPER].set_view_reports(true);
        admin_privileges[PRIMARY_DEVELOPER].set_handle_reports(true);
        admin_privileges[PRIMARY_DEVELOPER].set_find_single_users(true);
        admin_privileges[PRIMARY_DEVELOPER].set_update_single_users(true);
        admin_privileges[PRIMARY_DEVELOPER].set_view_activity_feedback(true);
        admin_privileges[PRIMARY_DEVELOPER].set_view_other_feedback(true);
        admin_privileges[PRIMARY_DEVELOPER].set_view_bugs_feedback(true);
        admin_privileges[PRIMARY_DEVELOPER].set_setup_default_values(true);
        admin_privileges[PRIMARY_DEVELOPER].set_request_chat_messages(true);
        admin_privileges[PRIMARY_DEVELOPER].set_request_error_messages(true);
        admin_privileges[PRIMARY_DEVELOPER].set_handle_error_messages(true);
        admin_privileges[PRIMARY_DEVELOPER].set_run_server_shutdowns(true);
        admin_privileges[PRIMARY_DEVELOPER].set_add_events(true);
        admin_privileges[PRIMARY_DEVELOPER].set_cancel_events(true);
        admin_privileges[PRIMARY_DEVELOPER].set_find_single_event(true);
        admin_privileges[PRIMARY_DEVELOPER].set_request_events(true);

        //STANDARD_DEVELOPER
        admin_privileges[STANDARD_DEVELOPER].set_view_bugs_feedback(true);
        admin_privileges[STANDARD_DEVELOPER].set_request_error_messages(true);
        admin_privileges[STANDARD_DEVELOPER].set_handle_error_messages(true);

        //FULL_ACCESS_ADMIN
        admin_privileges[FULL_ACCESS_ADMIN].set_view_matching_activity_statistics(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_view_age_gender_statistics(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_view_activities_and_categories(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_update_activities_and_categories(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_update_icons(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_view_reports(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_handle_reports(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_find_single_users(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_update_single_users(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_view_activity_feedback(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_view_other_feedback(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_view_bugs_feedback(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_request_chat_messages(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_add_events(true);
        admin_privileges[FULL_ACCESS_ADMIN].set_cancel_events(true);
        admin_privileges[PRIMARY_DEVELOPER].set_find_single_event(true);
        admin_privileges[PRIMARY_DEVELOPER].set_request_events(true);

        //HANDLE_REPORTS_ONLY
        admin_privileges[HANDLE_REPORTS_ONLY].set_view_reports(true);
        admin_privileges[HANDLE_REPORTS_ONLY].set_handle_reports(true);
        admin_privileges[HANDLE_REPORTS_ONLY].set_request_chat_messages(true);
    }

    //called with the AdminLevelEnum to get the privileges of that specific admin level
    // NOTE: this is const so AdminPrivileges cannot be changed
    const AdminPrivileges& operator [] (AdminLevelEnum i) const {
        if(0 <= i && i < (int)admin_privileges.size()) {
            return admin_privileges[i];
        } else {
            std::optional<std::string> dummy_string;
            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                          dummy_string,
                                          std::string("Invalid value used to access admin_privileges vector."),
                                          "database", database_names::ACCOUNTS_DATABASE_NAME,
                                          "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME,
                                          "value", i
            );
            return admin_privileges[0];
        }
    };
};

inline AdminPrivilegesHolder admin_privileges;