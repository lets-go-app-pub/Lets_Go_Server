//
// Created by jeremiah on 2/9/23.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (PRE_LOGIN_CHECKERS_COLLECTION_NAME)
namespace pre_login_checkers_keys {
    inline const std::string INSTALLATION_ID = "_id"; //string; The installation id, a UUID given to each installation of the app.

    inline const std::string NUMBER_SMS_VERIFICATION_MESSAGES_SENT = "nVs"; //int32; the total number of times sms verification messages have been sent from this documents' device ID NOTE: works with SMS_VERIFICATION_MESSAGES_LAST_UPDATE_TIME
    inline const std::string SMS_VERIFICATION_MESSAGES_LAST_UPDATE_TIME = "vLu"; //int64; (NOT MONGODB DATE TYPE); this is unixTimestamp/TIME_BETWEEN_SMS_VERIFICATION_MESSAGES_BY_INSTALLATION_ID, so it is not the full timestamp NOTE: works with NUMBER_SMS_VERIFICATION_MESSAGES_SENT
}