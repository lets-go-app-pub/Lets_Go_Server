//
// Created by jeremiah on 10/5/22.
//

#include <gtest/gtest.h>
#include "setup_login_info.h"
#include "utility_general_functions.h"

void checkLoginInfoAdminAndClient(
        const std::string& admin_name,
        const std::string& admin_password,
        const bsoncxx::oid& user_account_oid,
        const std::string& logged_in_token,
        const std::string& installation_ids,
        const std::function<ReturnStatus(const LoginToServerBasicInfo& /* login_info */)>& run_function
) {

    //check client login
    {
        LoginToServerBasicInfo login_info;

        setupUserLoginInfo(
                &login_info,
                user_account_oid,
                logged_in_token,
                installation_ids
        );

        login_info.set_lets_go_version(0);

        ReturnStatus return_status = run_function(login_info);

        EXPECT_EQ(return_status, ReturnStatus::OUTDATED_VERSION);

        login_info.Clear();

        setupUserLoginInfo(
                &login_info,
                user_account_oid,
                logged_in_token,
                installation_ids
        );

        login_info.set_current_account_id(gen_random_alpha_numeric_string(rand() % 10));

        return_status = run_function(login_info);

        EXPECT_EQ(return_status, ReturnStatus::INVALID_USER_OID);

        login_info.Clear();

        setupUserLoginInfo(
                &login_info,
                user_account_oid,
                logged_in_token,
                installation_ids
        );

        login_info.set_logged_in_token(gen_random_alpha_numeric_string(rand() % 10));

        return_status = run_function(login_info);

        EXPECT_EQ(return_status, ReturnStatus::INVALID_LOGIN_TOKEN);

        login_info.Clear();

        setupUserLoginInfo(
                &login_info,
                user_account_oid,
                logged_in_token,
                installation_ids
        );

        login_info.set_installation_id(gen_random_alpha_numeric_string(rand() % 10));

        return_status = run_function(login_info);

        EXPECT_EQ(return_status, ReturnStatus::INVALID_INSTALLATION_ID);
    }

    //check admin login
    {
        //NOTE: Admin info does not return a ReturnStatus necessarily. Instead, it should always return
        // a bool saying successful or not.
        LoginToServerBasicInfo login_info;

        for(int i = 0; i < 3; ++i) {
            setupAdminLoginInfo(
                    &login_info,
                    admin_name,
                    admin_password
            );

            ReturnStatus expected_return_status = ReturnStatus::UNKNOWN;

            switch(i) {
                case 0: {
                    login_info.set_lets_go_version(0);
                    expected_return_status = ReturnStatus::OUTDATED_VERSION;
                    break;
                }
                case 1: {
                    login_info.set_admin_name(gen_random_alpha_numeric_string(rand() % 10));
                    expected_return_status = ReturnStatus::LG_ERROR;
                    break;
                }
                case 2: {
                    login_info.set_admin_password(gen_random_alpha_numeric_string(rand() % 10));
                    expected_return_status = ReturnStatus::LG_ERROR;
                    break;
                }
                default:
                    break;
            }

            ReturnStatus return_status = run_function(login_info);

            EXPECT_EQ(expected_return_status, return_status);

            login_info.Clear();
        }
    }
}