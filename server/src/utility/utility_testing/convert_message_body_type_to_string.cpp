//
// Created by jeremiah on 5/4/21.
//

#include "utility_testing_functions.h"

std::string convertMessageBodyTypeToString(MessageSpecifics::MessageBodyCase messageCase) {

    std::string returnString = "Error: invalid value";

    switch(messageCase) {
        case MessageSpecifics::kTextMessage:
            returnString = "kTextMessage";
            break;
        case MessageSpecifics::kPictureMessage:
            returnString = "kPictureMessage";
            break;
        case MessageSpecifics::kLocationMessage:
            returnString = "kLocationMessage";
            break;
        case MessageSpecifics::kMimeTypeMessage:
            returnString = "kMimeTypeMessage";
            break;
        case MessageSpecifics::kInviteMessage:
            returnString = "kInviteMessage";
            break;
        case MessageSpecifics::kEditedMessage:
            returnString = "kEditedMessage";
            break;
        case MessageSpecifics::kDeletedMessage:
            returnString = "kDeletedMessage";
            break;
        case MessageSpecifics::kUserKickedMessage:
            returnString = "kUserKickedMessage";
            break;
        case MessageSpecifics::kUserBannedMessage:
            returnString = "kUserBannedMessage";
            break;
        case MessageSpecifics::kDifferentUserJoinedMessage:
            returnString = "kDifferentUserJoinedMessage";
            break;
        case MessageSpecifics::kDifferentUserLeftMessage:
            returnString = "kDifferentUserLeftMessage";
            break;
        case MessageSpecifics::kUpdateObservedTimeMessage:
            returnString = "kUpdateObservedTimeMessage";
            break;
        case MessageSpecifics::kThisUserJoinedChatRoomStartMessage:
            returnString = "kThisUserJoinedChatRoomStartMessage";
            break;
        case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage:
            returnString = "kThisUserJoinedChatRoomMemberMessage";
            break;
        case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage:
            returnString = "kThisUserJoinedChatRoomFinishedMessage";
            break;
        case MessageSpecifics::kThisUserLeftChatRoomMessage:
            returnString = "kThisUserLeftChatRoomMessage";
            break;
        case MessageSpecifics::kUserActivityDetectedMessage:
            returnString = "kUserActivityDetectedMessage";
            break;
        case MessageSpecifics::kChatRoomNameUpdatedMessage:
            returnString = "kChatRoomNameUpdatedMessage";
            break;
        case MessageSpecifics::kChatRoomPasswordUpdatedMessage:
            returnString = "kChatRoomPasswordUpdatedMessage";
            break;
        case MessageSpecifics::kNewAdminPromotedMessage:
            returnString = "kNewAdminPromotedMessage";
            break;
        case MessageSpecifics::kNewPinnedLocationMessage:
            returnString = "kNewPinnedLocationMessage";
            break;
        case MessageSpecifics::kChatRoomCapMessage:
            returnString = "kChatRoomCapMessage";
            break;
        case MessageSpecifics::kMatchCanceledMessage:
            returnString = "kMatchCanceledMessage";
            break;
        case MessageSpecifics::kNewUpdateTimeMessage:
            returnString = "kNewUpdateTimeMessage";
            break;
        case MessageSpecifics::kHistoryClearedMessage:
            returnString = "kHistoryClearedMessage";
            break;
        case MessageSpecifics::kLoadingMessage:
            returnString = "kLoadingMessage";
            break;
        case MessageSpecifics::MESSAGE_BODY_NOT_SET:
            returnString = "MESSAGE_BODY_NOT_SET";
            break;
    }

    return returnString;
}