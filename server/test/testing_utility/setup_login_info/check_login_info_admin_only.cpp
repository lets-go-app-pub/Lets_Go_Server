//
// Created by jeremiah on 8/22/22.
//

#include <gtest/gtest.h>
#include "setup_login_info.h"
#include "utility_general_functions.h"

void checkLoginInfoAdminOnly(
        const std::string& admin_name,
        const std::string& admin_password,
        const std::function<bool(const LoginToServerBasicInfo& /* login_info */)>& run_function
) {
    //NOTE: Admin info does not return a ReturnStatus necessarily. Instead, it should always return
    // a bool saying successful or not.
    LoginToServerBasicInfo login_info;

    for(int i = 0; i < 4; ++i) {
        setupAdminLoginInfo(
                &login_info,
                admin_name,
                admin_password
        );

        switch(i) {
            case 0:
                login_info.set_lets_go_version(0);
                break;
            case 1:
                login_info.set_admin_name(gen_random_alpha_numeric_string(rand() % 10));
                break;
            case 2:
                login_info.set_admin_password(gen_random_alpha_numeric_string(rand() % 10));
                break;
            case 3:
                login_info.set_admin_info_used(false);
                break;
            default:
                break;
        }

        bool successful = run_function(login_info);

        EXPECT_EQ(successful, false);

        login_info.Clear();
    }
}