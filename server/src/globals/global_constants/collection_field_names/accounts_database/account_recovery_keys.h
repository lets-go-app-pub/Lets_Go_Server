//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (ACCOUNT_RECOVERY_COLLECTION_NAME)
namespace account_recovery_keys {
    //this is used to store the info for account recovery used by the web server
    //NOTE: the PHONE_NUMBER is used here instead of the user account _id so the web server has a current phone number copy
    inline const std::string VERIFICATION_CODE = "vC"; //string NOTE: accessed on django web server; make sure to change there if string is changed
    inline const std::string PHONE_NUMBER = "pN"; //string NOTE: accessed on django web server; make sure to change there if string is changed
    inline const std::string TIME_VERIFICATION_CODE_GENERATED = "tC"; //mongoDB Date Type; NOTE: accessed on django web server; make sure to change there if string is changed
    inline const std::string NUMBER_ATTEMPTS = "nA"; //int32; number of times user has attempted to 'guess' (or just got it wrong) correct current phone number NOTE: accessed on django web server; make sure to change there if string is changed
    inline const std::string USER_ACCOUNT_OID = "iD"; //oid; the current account oid of the account
}