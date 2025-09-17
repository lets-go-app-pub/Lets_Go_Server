//
// Created by jeremiah on 8/15/22.
//

#include "chat_change_stream_helper_functions.h"
#include "send_messages_implementation.h"
#include "chat_change_stream_values.h"

//Messages that are targeted at a specific user (the user is added or removed from a chat room)
// need special considerations if the message is out of order. This function will handle them.
// It will essentially send any messages that also target the same user to the users again to
// avoid unwanted conditions such as leave->join turning into join->leave.
void iterateAndSendMessagesToTargetUsers(
        const MessageWaitingToBeSent& message_info,
        const std::deque<MessageWaitingToBeSent>& messages_reference,
        const std::_Deque_iterator<MessageWaitingToBeSent, MessageWaitingToBeSent&, MessageWaitingToBeSent*>& iterator_pos_to_be_inserted_at
) {

    //If the sending message was of type kDifferentUserJoined, then any messages that were sent out of order need
    // to be sent to the user. This is done above when sendMessageToUsers() is called.
    //If the sending message was a type that left, and it is out of order then it can leave AFTER something
    // else happened. For example if a join should have been called afterwards, then it will join->leave
    // instead of leave->join. So need to iterate through missed messages and re-do any target messages.
    // Also note that when a kDifferentUserJoined is sent back to the calling user, it is converted into
    // a newUpdateMessage by the ChatStreamObjectContainer.
    //Any other type of message will be out of order, however it will not change anything in the system. It
    // will also be sent back elsewhere with kDifferentUserJoined (if a user left the messages do not
    // need sent back).
    for(auto it = iterator_pos_to_be_inserted_at; it != messages_reference.end(); ++it) {

        ChatMessageToClient* response_message;

        if(!it->chat_to_client_response->has_return_new_chat_message()
           || it->chat_to_client_response->return_new_chat_message().messages_list().empty()) {

            const std::string errorString = "An empty message was stored inside messages_reference\n";

            std::optional<std::string> dummyExceptionString;
            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                          dummyExceptionString, errorString,
                                          "chat_room_id", message_info.chat_room_id,
                                          "message_string", it->chat_to_client_response->DebugString(),
                                          "messages_reference.size()", std::to_string(messages_reference.size())
            );

            continue;
        }
        else {
            response_message = it->chat_to_client_response->mutable_return_new_chat_message()->mutable_messages_list(0);
        }

        //check if message must modify anything
        //NOTE: If the message_targets vector was sorted this could be iterated through more efficiently. However, in
        // practice the vector will at largest only get to be size of two, so it will be irrelevant.
        for(auto& current_target : message_info.message_targets) {
            for(const auto& message_target : it->message_targets) {
                if(message_target.account_oid == current_target.account_oid) {
                    if(message_target.add_or_remove == AddOrRemove::MESSAGE_TARGET_ADD) { //add target user

                        //Must get a reference to the user BEFORE inserting the chat room. Otherwise, the user could end in between
                        // insertUserOIDToChatRoomId() and sendMessageToUsers() and leak an object.
                        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> chat_stream_container = user_open_chat_streams.find(message_target.account_oid);

                        //insert must come BEFORE so the user will be added from the map_of_chat_rooms_to_users THEN get the message.
                        if (chat_stream_container != nullptr) {
                            insertUserOIDToChatRoomId(
                                    it->chat_room_id,
                                    message_target.account_oid,
                                    chat_stream_container->ptr()->getCurrentIndexValue()
                            );
                        }

                        sendTargetMessageToSpecificUser(
                                it->chat_room_id,
                                it->chat_to_client_response,
                                response_message,
                                message_target.account_oid,
                                chat_stream_container
                        );
                    }
                    else if(message_target.add_or_remove == MESSAGE_TARGET_REMOVE) { //remove target user

                        //Must get a reference to the user BEFORE sending the message, otherwise the user could end in between
                        // sendMessageToUsers() and eraseUserOIDFromChatRoomId() and leak an object.
                        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> chat_stream_container = user_open_chat_streams.find(message_target.account_oid);

                        sendTargetMessageToSpecificUser(
                                it->chat_room_id,
                                it->chat_to_client_response,
                                response_message,
                                message_target.account_oid,
                                chat_stream_container
                        );

                        //Must erase AFTER the message is sent or the user will not get the message because they were removed.
                        if (chat_stream_container != nullptr) {
                            eraseUserOIDFromChatRoomId(
                                    it->chat_room_id,
                                    message_target.account_oid,
                                    chat_stream_container->ptr()->getCurrentIndexValue()
                            );
                        }
                    }

                    //each account oid will only exist in message_targets once, no reason to continue iterating
                    break;
                }
            }
        }
    }
}

//Remove the messages until all cached_messages are under the max time.
void removeCachedMessagesOverMaxTime(
        std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>>& cached_messages,
        size_t& cached_messages_size_in_bytes,
        const std::chrono::milliseconds& current_timestamp
) {
    //Remove any messages that have reached the cached time limit and any chat room deque that
    // are empty as a result.
    for(auto it = cached_messages.begin(); it != cached_messages.end();) {
        while(!it->second.empty()) {
            if((current_timestamp - it->second.front().time_received) > chat_change_stream_values::MAX_TIME_TO_CACHE_MESSAGES) {
                cached_messages_size_in_bytes -= it->second.front().current_message_size;
                it->second.pop_front();
            } else {
                break;
            }
        }

        if(it->second.empty()) {
            it = cached_messages.erase(it);
            cached_messages_size_in_bytes -= chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
        } else {
            ++it;
        }
    }
}

//Remove the oldest messages until cached_messages is under 90% of max.
void removeCachedMessagesOverMaxBytes(
        std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>>& cached_messages,
        size_t& cached_messages_size_in_bytes
) {
    //don't allow cached_messages size to get over MAX_SIZE_FOR_MESSAGE_CACHE_IN_BYTES
    if(cached_messages_size_in_bytes > chat_change_stream_values::MAX_SIZE_FOR_MESSAGE_CACHE_IN_BYTES) {
        std::vector<std::pair<std::string, std::chrono::milliseconds>> timestamps_by_chat_room;
        for(const auto& [chat_room_id, chat_room_deque] : cached_messages) {
            for(const auto& message : chat_room_deque) {
                timestamps_by_chat_room.emplace_back(chat_room_id, message.time_message_stored);
            }
        }

        //set in descending order so that the back elements can be popped
        std::sort(timestamps_by_chat_room.begin(), timestamps_by_chat_room.end(), [](
                const auto& lhs, const auto& rhs
        ){
            return lhs.second > rhs.second;
        });

        //leave 90% of the bytes
        const static size_t PERCENT_OF_MESSAGE_CACHE_TO_KEEP_IN_BYTES = chat_change_stream_values::MAX_SIZE_FOR_MESSAGE_CACHE_IN_BYTES * 9/10;

        while(cached_messages_size_in_bytes > PERCENT_OF_MESSAGE_CACHE_TO_KEEP_IN_BYTES
              && !timestamps_by_chat_room.empty()) {

            auto cached_message_ptr = cached_messages.find(timestamps_by_chat_room.back().first);

            if(cached_message_ptr != cached_messages.end()) {

                if(!cached_message_ptr->second.empty()) {
                    cached_messages_size_in_bytes -= cached_message_ptr->second.front().current_message_size;
                    cached_message_ptr->second.pop_front();
                } else {
                    const std::string errorString = "An empty chat room was found inside cached_messages. The chat"
                                                    " room should always be removed when the final message is popped.\n";

                    std::optional<std::string> dummyExceptionString;
                    storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                  dummyExceptionString, errorString,
                                                  "cached_messages.size()", std::to_string(cached_messages.size()));
                }

                if(cached_message_ptr->second.empty()) {
                    cached_messages.erase(timestamps_by_chat_room.back().first);
                    cached_messages_size_in_bytes -= chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
                }
            } else {
                const std::string errorString = "Elements were added from this chat room a moment ago. The chat room id should always"
                                                "exist when attempting to remove it here. This could mean the deque is accessed concurrently.\n";

                std::optional<std::string> dummyExceptionString;
                storeMongoDBErrorAndException(__LINE__, __FILE__,
                                              dummyExceptionString, errorString,
                                              "cached_messages.size()", std::to_string(cached_messages.size()),
                                              "timestamps_by_chat_room.size()", std::to_string(timestamps_by_chat_room.size()));
            }

            timestamps_by_chat_room.pop_back();
        }
    }
}

//Will send the previous messages inside cached_messages over the last chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.
// Will then add on a kDifferentUserJoinedMessage for the current user. This function is only meant to be called for the user
// that originally sent the kDifferentUserJoinedMessage message.
void sendPreviouslyStoredMessagesWithDifferentUserJoined(
        const std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>>& cached_messages,
        const std::string& chatRoomId,
        ChatMessageToClient* responseMsg,
        const std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>>& stream_container_object
        ) {

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> messages_reply_vector;

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> send_messages_object(
            messages_reply_vector);

    std::vector<std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>> messages_to_send;

    auto chat_room_ptr = cached_messages.find(chatRoomId);

    if (chat_room_ptr != cached_messages.end()) {

        auto first_iterator_to_send_back = chat_room_ptr->second.end();

        //If the kDifferentUserJoinedMessage message was inserted out of order, this takes care of sending back any messages
        // AFTER this message as well as before it
        //iterate from the back to the front to find the first index to send
        //using iterator is generally faster for a deque than random access
        for (auto it = chat_room_ptr->second.rbegin(); it != chat_room_ptr->second.rend(); ++it, first_iterator_to_send_back = it.base()) {
            //break when difference between times is greater than time_to_request_on_user_joined
            if (
                (std::chrono::milliseconds{responseMsg->timestamp_stored()} - it->time_message_stored) >
                chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES
                ) {
                break;
            }
        }

        for (auto it = first_iterator_to_send_back; it != chat_room_ptr->second.end(); ++it) {
            for (auto& message: *it->chat_to_client_response->mutable_return_new_chat_message()->mutable_messages_list()) {

                MessageSpecifics::MessageBodyCase current_message_type = message.message().message_specifics().message_body_case();

                if (
                        //only send back deleted message with valid delete types for this user
                        (current_message_type == MessageSpecifics::kDeletedMessage
                         && (
                                 //invalid delete type
                                 (message.message().message_specifics().deleted_message().delete_type() !=
                                  DeleteType::DELETE_FOR_SINGLE_USER
                                  &&
                                  message.message().message_specifics().deleted_message().delete_type() !=
                                  DeleteType::DELETE_FOR_ALL_USERS)
                                 //deleted for a different user
                                 ||
                                 (message.message().message_specifics().deleted_message().delete_type() ==
                                  DeleteType::DELETE_FOR_SINGLE_USER
                                  && responseMsg->sent_by_account_id() != message.sent_by_account_id())
                            )
                        )
                        ||
                        //only send back invite types for this user
                        (current_message_type == MessageSpecifics::kInviteMessage
                         && responseMsg->sent_by_account_id() != message.message().message_specifics().invite_message().invited_user_account_oid()
                         && responseMsg->sent_by_account_id() != message.sent_by_account_id()
                        )
                ) {
                    continue;
                }
                else if(current_message_type == MessageSpecifics::kDifferentUserJoinedMessage
                        && responseMsg->sent_by_account_id() != message.sent_by_account_id()) { //a different user joined the chat room

                    //cleanup messages so that the next message can be sent in order
                    send_messages_object.finalCleanup();

                    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> vector_to_move;

                    //Must make a new vector because send_messages_object holds a reference to messages_reply_vector.
                    vector_to_move.insert(
                        vector_to_move.end(),
                        std::make_move_iterator(messages_reply_vector.begin()),
                        std::make_move_iterator(messages_reply_vector.end())
                    );

                    messages_to_send.emplace_back(
                            [_messages_reply_vector = std::move(vector_to_move)](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector){
                                    reply_vector.insert(
                                            reply_vector.end(),
                                            std::make_move_iterator(_messages_reply_vector.begin()),
                                            std::make_move_iterator(_messages_reply_vector.end())
                                    );
                            }
                    );

                    messages_reply_vector.clear();

                    //This will make another database call required even if one was not done before. See inside
                    // buildDifferentUserMessageForReceivingUser() for details. Ideally this should not run much.
                    //It is also important that it be stored as a separate element inside reply_vector. This is because
                    // the message can get larger because the user information can be requested from the database.
                    messages_to_send.emplace_back(
                            buildDifferentUserMessageForReceivingUser(
                                    it->chat_to_client_response,
                                    true
                            )
                    );
                }
                else { //other message types

                    //Do not want do_not_update_user_state set. If a message must update this user account state AND it
                    // was missed the case will be handled inside iterateAndSendMessagesToTargetUsers().
                    message.mutable_message()->mutable_standard_message_info()->set_do_not_update_user_state(
                            true);
                    message.mutable_message()->mutable_standard_message_info()->set_internal_force_send_message_to_current_user(
                            true);
                    //makes a copy so value can be set back
                    send_messages_object.sendMessage(message);
                    message.mutable_message()->mutable_standard_message_info()->set_do_not_update_user_state(
                            false);
                    message.mutable_message()->mutable_standard_message_info()->set_internal_force_send_message_to_current_user(
                            false);
                }
            }
        }
    }
    // else {} //chat room not found, this is OK

    //send the kDifferentUserJoinedMessage message
    //NOTE: Leave injected_from_different_user_joined_message false on this message. Otherwise,
    // the message will NOT be removed inside ChatStreamContainerObject (as it should be).
    ChatMessageToClient current_message;

    buildDifferentUserChatMessageToClientForSender(
            &current_message,
            responseMsg->sent_by_account_id(),
            responseMsg->message_uuid(),
            responseMsg->timestamp_stored(),
            chatRoomId
    );

    send_messages_object.sendMessage(current_message);

    send_messages_object.finalCleanup();

    messages_to_send.emplace_back(
            [_messages_reply_vector = std::move(messages_reply_vector)](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector){
                    reply_vector.insert(
                            reply_vector.end(),
                            std::make_move_iterator(_messages_reply_vector.begin()),
                            std::make_move_iterator(_messages_reply_vector.end())
                    );
            }
    );

    stream_container_object->ptr()->injectStreamResponse(
            [messages_to_send = std::move(messages_to_send)](
                    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector
            ) {
                for(const auto& lambda : messages_to_send) {
                    lambda(reply_vector);
                }
            },
            stream_container_object
    );

}

void sendMessageToUsers(
        const std::string& chatRoomId,
        const std::shared_ptr<grpc_stream_chat::ChatToClientResponse>& chat_to_client_response,
        ChatMessageToClient* responseMsg,
        const std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>>& cached_messages
) {

    const MessageSpecifics::MessageBodyCase& messageType = responseMsg->message().message_specifics().message_body_case();

    if (messageType != MessageSpecifics::kDeletedMessage
        && messageType != MessageSpecifics::kInviteMessage
        && messageType != MessageSpecifics::kDifferentUserJoinedMessage
            ) {
        auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
        if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists
            //This method will only require a maximum of one database call.
            chatRoomOIDs->second.iterateAndSendMessageUniqueMessageToSender(
                    responseMsg->sent_by_account_id(),
                    buildMessageLambdaToBeInjected(chat_to_client_response),
                    buildNewUpdateTimeMessageLambda(
                            responseMsg,
                            chatRoomId
                    )
            );
        }
    }
    else if (messageType == MessageSpecifics::kDifferentUserJoinedMessage) {
        auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
        if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists

            //This method will only require a maximum of one database call.
            //Sending to other users must come second, the message is modified here and a race condition
            // can occur if the message is updated while being accessed. When it is returned to the sending user
            // it generates a completely different message.
            chatRoomOIDs->second.iterateAndSendMessageExcludeSender(
                    responseMsg->sent_by_account_id(),
                    buildDifferentUserMessageForReceivingUser(
                        chat_to_client_response,
                        false
                    )
            );

            //This user just joined the chat room (this message type guarantees it) so they are inside the chat room. No
            // reason to interact with map_of_chat_rooms_to_users.
            std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                    responseMsg->sent_by_account_id());

            //cached_messages is not thread safe, therefore make sure the user is connected to this server before generating the
            // message.
            if (stream_container_object != nullptr) {
                sendPreviouslyStoredMessagesWithDifferentUserJoined(
                    cached_messages,
                    chatRoomId,
                    responseMsg,
                    stream_container_object
                );
            }

        }
    }
    else if (messageType == MessageSpecifics::kDeletedMessage) {

        DeleteType deleteType = responseMsg->message().message_specifics().deleted_message().delete_type();
        auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
        if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists
            if (deleteType == DELETE_FOR_ALL_USERS) {
                chatRoomOIDs->second.iterateAndSendMessageUniqueMessageToSender(
                        responseMsg->sent_by_account_id(),
                        buildMessageLambdaToBeInjected(chat_to_client_response),
                        buildNewUpdateTimeMessageLambda(
                                responseMsg,
                                chatRoomId
                        )
                );
            } else if (deleteType == DeleteType::DELETE_FOR_SINGLE_USER) {
                chatRoomOIDs->second.sendMessageToSpecificUser(
                        buildNewUpdateTimeMessageLambda(
                                responseMsg,
                                chatRoomId
                        ),
                        responseMsg->sent_by_account_id()
                );
            }
        }
    }
    else { //INVITED_TO_CHAT_ROOM

        std::cout << "INVITED_MESSAGE_SENT\n";

        auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
        if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists
            chatRoomOIDs->second.sendMessagesToTwoUsers(
                    buildMessageLambdaToBeInjected(chat_to_client_response),
                    responseMsg->message().message_specifics().invite_message().invited_user_account_oid(),
                    buildNewUpdateTimeMessageLambda(
                            responseMsg,
                            chatRoomId
                    ),
                    responseMsg->sent_by_account_id()
            );
        }
    }
}

void sendTargetMessageToSpecificUser(
        const std::string& chatRoomId,
        const std::shared_ptr<grpc_stream_chat::ChatToClientResponse>& chat_to_client_response,
        ChatMessageToClient* responseMsg,
        const std::string& user_account_oid,
        const std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>>& chat_stream_container
) {

    const MessageSpecifics::MessageBodyCase& messageType = responseMsg->message().message_specifics().message_body_case();

    if (messageType != MessageSpecifics::kDeletedMessage
        && messageType != MessageSpecifics::kInviteMessage
        && messageType != MessageSpecifics::kDifferentUserJoinedMessage
            ) {

        std::function<void(
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)> function_to_send;

        if (responseMsg->sent_by_account_id() == user_account_oid) {
            function_to_send = buildNewUpdateTimeMessageLambda(
                    responseMsg,
                    chatRoomId
            );
        } else {
            function_to_send = buildMessageLambdaToBeInjected(chat_to_client_response);
        }

        chat_stream_container->ptr()->injectStreamResponse(
                function_to_send,
                chat_stream_container
        );
    }
    else if (messageType == MessageSpecifics::kDifferentUserJoinedMessage) {

        std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)> function_to_send;

        if (responseMsg->sent_by_account_id() == user_account_oid) {
            //No need to request previous messages, they were already requested if this is relevant.
            function_to_send = buildDifferentUserMessageForSender(
                    responseMsg,
                    chatRoomId
            );
        }
        else {
            //This will make another database call required even if one was not done before. See inside
            // buildDifferentUserMessageForReceivingUser() for details. Ideally this should not run much.
            function_to_send = buildDifferentUserMessageForReceivingUser(
                    chat_to_client_response,
                    false
            );
        }

        chat_stream_container->ptr()->injectStreamResponse(
                function_to_send,
                chat_stream_container
        );
    }
    else if (messageType == MessageSpecifics::kDeletedMessage) {

        DeleteType delete_type = responseMsg->message().message_specifics().deleted_message().delete_type();
        if(delete_type == DELETE_FOR_ALL_USERS) {
            std::function<void(
                    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)> function_to_send;

            if (responseMsg->sent_by_account_id() == user_account_oid) {
                function_to_send = buildNewUpdateTimeMessageLambda(
                        responseMsg,
                        chatRoomId
                );
            } else {
                function_to_send = buildMessageLambdaToBeInjected(chat_to_client_response);
            }

            chat_stream_container->ptr()->injectStreamResponse(
                    function_to_send,
                    chat_stream_container
            );
        } else if(delete_type == DeleteType::DELETE_FOR_SINGLE_USER) {
            if (responseMsg->sent_by_account_id() == user_account_oid) {
                chat_stream_container->ptr()->injectStreamResponse(
                        buildNewUpdateTimeMessageLambda(
                                responseMsg,
                                chatRoomId
                        ),
                        chat_stream_container
                );
            }
        }
    }
    else { //INVITED_TO_CHAT_ROOM

        std::function<void(
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)> function_to_send;

        if(user_account_oid == responseMsg->message().message_specifics().invite_message().invited_user_account_oid()) {
            function_to_send = buildMessageLambdaToBeInjected(chat_to_client_response);
        } else if(user_account_oid == responseMsg->sent_by_account_id()) {
            function_to_send = buildNewUpdateTimeMessageLambda(
                    responseMsg,
                    chatRoomId
            );
        } else {
            return;
        }

        chat_stream_container->ptr()->injectStreamResponse(
                function_to_send,
                chat_stream_container
        );
    }
}