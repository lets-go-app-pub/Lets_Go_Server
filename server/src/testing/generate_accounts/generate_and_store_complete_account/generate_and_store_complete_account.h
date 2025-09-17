//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include <string>
#include <vector>
#include <activity_struct.h>

//simulates generating a complete account by calling functions login->smsVerification->login etc
std::string generateAndStoreCompleteAccount(
        const std::string& phoneNumber, const std::string& email, int birthYear, int birthMonth,
        int birthDayOfMonth, const std::string& firstName, const std::string& gender,
        const std::vector<std::tuple<const int, const std::string, const std::string>>& pictureAndIndexVector,
        const std::vector<ActivityStruct>& categories, const double& longitude,
        const double& latitude, int minAgeRange, int maxAgeRange,
        const std::vector<std::string>& genderRange, int maxDistance, const std::string& city,
        const std::string& bio, const std::string& user_account_oid = "~");
