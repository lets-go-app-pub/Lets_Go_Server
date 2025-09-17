//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (FEEDBACK_DATABASE_NAME) (OTHER_SUGGESTION_FEEDBACK_COLLECTION_NAME)
namespace other_suggestion_feedback_keys {
    inline const std::string ACCOUNT_OID = "iD"; //OID; the account OID of the person that sent the feedback
    inline const std::string MESSAGE = "mE"; //utf8; the message body from the suggestion
    inline const std::string TIMESTAMP_STORED = "dA"; //mongodb Date type; time this feedback was inserted
    inline const std::string MARKED_AS_SPAM = "sP"; //utf8; field is not set by default, set to SHARED_NAME_KEY of admin that reported it as spam if reported as spam
}