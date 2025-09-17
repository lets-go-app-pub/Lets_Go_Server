//
// Created by jeremiah on 7/28/22.
//

#pragma once

#include <string>
#include "AdminLevelEnum.grpc.pb.h"

#include "account_objects.h"

inline const std::string TEMP_ADMIN_ACCOUNT_NAME = "1234_temp_admin";
inline const std::string TEMP_ADMIN_ACCOUNT_PASSWORD = "temp_admin_account_password";

//This is set up to upsert the account. This way it can be called several times in a row with
// the same information during testing.
std::string createTempAdminAccount(AdminLevelEnum adminPrivilegeLevel);
