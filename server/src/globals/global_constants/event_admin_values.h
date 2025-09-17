//
// Created by jeremiah on 3/8/23.
//

#pragma once

#include <string>
#include <chrono>
#include <bsoncxx/oid.hpp>
#include "global_bsoncxx_docs.h"

namespace event_admin_values {

    inline const bsoncxx::oid OID{"6408b04146de04a5d7f65982"};

    inline const auto STATUS = UserAccountStatus::STATUS_ACTIVE;
    inline const std::string INSTALLATION_ID = "287e6609-9dce-4240-b1c9-2eb78a2a5efc";

    inline const std::string PHONE_NUMBER = "6408b04146de04a5d7f65982"; //the same as oid
    inline const std::string FIRST_NAME = "EventAdmin"; //string;
    inline const std::string GENDER = "LetsGo";
    inline const std::string BIO = "Official LetsGo event admin.";
    inline const int BIRTH_YEAR = 1903;
    inline const int BIRTH_MONTH = 3;
    inline const int BIRTH_DAY_OF_MONTH = 7;
    inline const int BIRTH_DAY_OF_YEAR = 66;
    inline const int AGE = 120;
    inline const std::string EMAIL_ADDRESS = "suppore@letsgoapp.site";
    inline const bool EMAIL_ADDRESS_REQUIRES_VERIFICATION = false;
};