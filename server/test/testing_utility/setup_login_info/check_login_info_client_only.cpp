//
// Created by jeremiah on 8/22/22.
//

#include <gtest/gtest.h>
#include "setup_login_info.h"
#include "utility_general_functions.h"

void checkLoginInfoClientOnly(
        const bsoncxx::oid& user_account_oid,
        const std::string& logged_in_token,
        const std::string& installation_ids,
        const std::function<ReturnStatus(const LoginToServerBasicInfo& /* login_info */)>& run_function
) {

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

    login_info.Clear();

    setupUserLoginInfo(
            &login_info,
            user_account_oid,
            logged_in_token,
            installation_ids
    );

    login_info.set_admin_info_used(true);

    return_status = run_function(login_info);

    EXPECT_EQ(return_status, ReturnStatus::INVALID_PARAMETER_PASSED);

}
