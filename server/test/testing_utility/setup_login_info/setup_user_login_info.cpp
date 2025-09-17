//
// Created by jeremiah on 6/3/22.
//

#include <general_values.h>
#include "setup_login_info.h"

void setupUserLoginInfo(
        LoginToServerBasicInfo* login_info,
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_ids
        ) {
    login_info->set_admin_info_used(false);
    login_info->set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);

    login_info->set_current_account_id(account_oid.to_string());
    login_info->set_logged_in_token(logged_in_token);
    login_info->set_installation_id(installation_ids);
}