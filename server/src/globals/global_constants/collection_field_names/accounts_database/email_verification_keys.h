//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (EMAIL_VERIFICATION_COLLECTION_NAME)
namespace email_verification_keys {
    //this is used to store the info for email verification used by the web server
    inline const std::string USER_ACCOUNT_REFERENCE = "_id"; //Object Id; NOTE: accessed on django web server; make sure to change there if string is changed
    inline const std::string VERIFICATION_CODE = "vC"; //string; NOTE: accessed on django web server; make sure to change there if string is changed
    inline const std::string TIME_VERIFICATION_CODE_GENERATED = "tC"; //mongoDB Date Type; NOTE: accessed on django web server; make sure to change there if string is changed
    inline const std::string ADDRESS_BEING_VERIFIED = "eA"; //string; NOTE: accessed on django web server; make sure to change there if string is changed
}