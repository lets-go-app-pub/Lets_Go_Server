//
// Created by jeremiah on 6/3/22.
//

#pragma once

#include <functional>

#include <bsoncxx/oid.hpp>
#include "LoginToServerBasicInfo.grpc.pb.h"
#include "StatusEnum.grpc.pb.h"
void setupUserLoginInfo(
        LoginToServerBasicInfo* login_info,
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_ids
        );

void setupAdminLoginInfo(
        LoginToServerBasicInfo* login_info,
        const std::string& admin_name,
        const std::string& admin_password
        );

void checkLoginInfoClientOnly(
        const bsoncxx::oid& user_account_oid,
        const std::string& logged_in_token,
        const std::string& installation_ids,
        const std::function<ReturnStatus(const LoginToServerBasicInfo& /* login_info */)>& run_function
);

void checkLoginInfoAdminOnly(
        const std::string& admin_name,
        const std::string& admin_password,
        const std::function<bool(const LoginToServerBasicInfo& /* login_info */)>& run_function
);

void checkLoginInfoAdminAndClient(
        const std::string& admin_name,
        const std::string& admin_password,
        const bsoncxx::oid& user_account_oid,
        const std::string& logged_in_token,
        const std::string& installation_ids,
        const std::function<ReturnStatus(const LoginToServerBasicInfo& /* login_info */)>& run_function
);