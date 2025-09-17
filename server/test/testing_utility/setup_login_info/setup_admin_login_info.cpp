//
// Created by jeremiah on 6/6/22.
//

#include <general_values.h>
#include "setup_login_info.h"

void setupAdminLoginInfo(
        LoginToServerBasicInfo* login_info,
        const std::string& admin_name,
        const std::string& admin_password
        ) {
    login_info->set_admin_info_used(true);
    login_info->set_lets_go_version(general_values::MINIMUM_ACCEPTED_DESKTOP_INTERFACE_VERSION);

    login_info->set_admin_name(admin_name);
    login_info->set_admin_password(admin_password);
}