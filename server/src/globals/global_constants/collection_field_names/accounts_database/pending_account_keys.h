//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (PENDING_ACCOUNT_COLLECTION_NAME)
namespace pending_account_keys {
    inline const std::string TYPE = "aT"; //int Type of account; follows AccountLoginType.
    inline const std::string PHONE_NUMBER = "pN"; //string; Used with all account types to store the phone number, allows for only 1 pending account per phone number (even if it is redundant when INDEXING is also a phone number).
    inline const std::string INDEXING = "aI"; //string; Will be phone number if accountType is phone or accountID if accountType is facebook or google.
    inline const std::string ID = "dI"; //string; NOTE: The installation ID, NOT the account id.
    inline const std::string VERIFICATION_CODE = "vC"; //string
    inline const std::string TIME_VERIFICATION_CODE_WAS_SENT = "tC"; //mongoDB Date Type; NOTE This is used in TTL indexing (removing the account) so it stores the time the code was sent, NOT the time it expires.
}