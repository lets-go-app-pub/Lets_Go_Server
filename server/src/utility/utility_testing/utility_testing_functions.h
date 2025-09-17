//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <AccountLoginTypeEnum.pb.h>
#include <TypeOfChatMessage.grpc.pb.h>
#include <ChatMessageStream.pb.h>
#include <FindMatches.pb.h>
#include <utility_chat_functions.h>
#include "thread_safe_writes_waiting_to_process_vector.h"
#include "call_data_command.h"
#include "pushed_from_queue_location_enum.h"

//prints the enum Status for MessageInstructions inside TypeOfChatMessage.proto
//std::string convertMessageInstructionToString(MessageInstruction message_instruction);

//takes in a bson field type and returns a string representing that type
std::string convertBsonTypeToString(const bsoncxx::type& type);

//prints the enum Status for TypeOfChatMessage::MessageBodyCase inside TypeOfChatMessage.proto
std::string convertMessageBodyTypeToString(MessageSpecifics::MessageBodyCase messageCase);

//prints the enum Status for ReturnStatus
std::string convertReturnStatusToString(ReturnStatus returnStatus);

//prints the enum Status for SuccessTypes inside SingleMatchMessage
std::string convertSuccessTypesToString(findmatches::FindMatchesCapMessage::SuccessTypes successTypes);

//prints the enum Status for DeleteType inside TypeOfChatMessage.proto
std::string convertDeleteTypeToString(DeleteType deleteType);

//prints true or false
std::string convertBoolToString(int a);

//prints the enum RequiresInfo for login_function
std::string convertAccessStatusToString(int a);

//prints the enum Status for login_function
std::string convertLoginFunctionStatusToString(int a);

//prints the enum Status for SMSVerification
std::string convertSmsVerificationStatusToString(int a);

//prints the enum Status for ValidateArrayElementReturnEnum
std::string convertValidateArrayElementReturnEnumToString(int a);

//prints the enum Status for PushedToQueueFromLocation
std::string convertPushedToQueueFromLocationToString(PushedToQueueFromLocation pushedToQueueFromLocation);

//prints the enum Status for ReplySpecifics::ReplyBodyCase
std::string convertReplyBodyCaseToString(ReplySpecifics::ReplyBodyCase replyBodyCase);

//prints the enum Status for AccountLoginType
std::string convertAccountLoginTypeToString(AccountLoginType accountLoginType);

//prints the enum Status for ChatToClientResponse::ServerResponseCase
std::string convertServerResponseCaseToString(grpc_stream_chat::ChatToClientResponse::ServerResponseCase serverResponseCase);

//prints the enum Status for AccountStateInChatRoom
std::string convertAccountStateInChatRoomToString(const AccountStateInChatRoom& accountStateInChatRoom);

//prints the enum Status for DifferentUserJoinedChatRoomAmount
std::string convertDifferentUserJoinedChatRoomAmountToString(const DifferentUserJoinedChatRoomAmount& differentUserJoinedChatRoomAmount);

//prints the enum Status for CallDataCommandTypes
std::string convertCallDataCommandTypesToString(CallDataCommandTypes callDataCommandTypes);

//returns the duration between the 2 passed timespec values
double durationAsNanoSeconds(timespec stop, timespec start);
