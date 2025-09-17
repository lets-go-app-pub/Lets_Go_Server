//
// Created by jeremiah on 5/20/21.
//

#include "utility_testing_functions.h"

std::string convertReplyBodyCaseToString(ReplySpecifics::ReplyBodyCase replyBodyCase) {

    switch (replyBodyCase) {
        case ReplySpecifics::kTextReply:
            return "kTextReply";
        case ReplySpecifics::kPictureReply:
            return "kPictureReply";
        case ReplySpecifics::kLocationReply:
            return "kLocationReply";
        case ReplySpecifics::kMimeReply:
            return "kMimeReply";
        case ReplySpecifics::kInviteReply:
            return "kInviteReply";
        case ReplySpecifics::REPLY_BODY_NOT_SET:
            return "REPLY_BODY_NOT_SET";
    }

    return  "Error: invalid value";
}
