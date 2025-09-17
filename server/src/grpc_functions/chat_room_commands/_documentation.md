
## Chat Room Times 

These are various times the client and server use to check which message have been viewed and which messages have been
sent to the client.

---

### Chat Room Last Time Updated

 * Use: Checking when messages were missed and require updating from the server (used on login and updateChatRoom()).

##### Notes

 * This time is client specific, there is no server representation.

##### Places Updated
Every time a message stored inside the chat room is received this is updated, however certain messages are
exceptions for example kThisUserJoinedChatRoom messages do not because they are messages that creates a new chat room
on the client.

---

### Chat Room Last Observed Time
 * Use: Checking against 'Chat Room Last Active Time' to see if this user has missed chat messages.

##### Notes

* Represented on the server as user_account_keys::chat_rooms::LAST_TIME_VIEWED. It is stored in the user account in
  order to transfer between devices.
* The observed time must be recorded from the client then sent to the server so DO NOT use it to interact with
  other users, it could be polluted.

##### Places Updated

Each time the chat room fragment onViewCreated is called the chat room will check the time sent of the last 
message (it will ignore the kUpdateObservedTimeMessage message itself) and if the message was sent after the 'Chat
Room Last Observed Time' then it will send a message to update it on the server (the server will never store
kUpdateObservedTimeMessage).

These functions set it on the server and so make sure the return of these sets value on the client (they are set
for the calling user only).
 1) clientMessageToServer()
 2) createChatRoom()
 3) joinChatRoom()
 4) removeFromChatRoom()
 5) promoteNewAdmin()
 6) updateChatRoomInfo()
 7) updateChatRoom()

The value is set whenever a user joins a chat room whether through joinChatRoom() or a match is made.

---

### Chat Room Last Active Time

* Use: Checking against Chat Room Last Observed Time to see if this user has missed chat messages.

##### Notes

* Represented on the server as chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME.

##### Places Updated

These are organized by location on the client, for example the updateChatRoomInfo() function on the server will
update the 'Chat Room Last Active Time' and also send back kChatRoomNameUpdatedMessage or kChatRoomPasswordUpdatedMessage
to the other users. This means that the response must update the 'Chat Room Last Active Time' and the messagesReceived()
function must update it for the respective messages.

When a message is received, these message types will need to set it on the client.
 1) kTextMessage
 2) kPictureMessage
 3) kMimeTypeMessage
 4) kLocationMessage
 5) kInviteMessage (this is a bit special, it won't update it on the server because it is only used for a single invite)
 6) kDifferentUserJoinedMessage
 7) kDifferentUserLeftMessage
 8) kUserKickedMessage
 9) kUserBannedMessage
 10) kNewAdminPromotedMessage
 11) kChatRoomNameUpdatedMessage
 12) kChatRoomPasswordUpdatedMessage

These functions set it on the server and so make sure the return of these sets the value on the client.
 1) createChatRoom() (create chat room helper to be more specific which is called when two users swipe 'yes' on each 
 other)
 2) joinChatRoom()
 3) leaveChatRoom() (because this leaves the chat room there is nothing to do on the client)
 4) removeFromChatRoom()
 5) promoteNewAdmin()
 6) updateChatRoomInfo()
 7) unMatch() (for sending user, however because this leaves the chat room there is nothing to do on the client)
 8) blockAndReportChatRoom() (only when un-matching, same notes as unMatch())

clientMessageToServer(); This function will set it on the server and so it return will need to be set on the client as
well. But only for certain messages.
 1) kTextMessage
 2) kPictureMessage
 3) kMimeTypeMessage
 4) kLocationMessage

---

### User Last Active Time

* Use: Display to other user 'last seen' time inside chat room info fragment.

##### Notes

 * Represented on the server as chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME.
 * A given user's device also keeps track of their personal 'last active time', however, it is not used for anything at
 the moment.

##### Places Updated

These are organized by location on the client, for example the updateChatRoomInfo() function on the server will
update the 'User Last Active Time' and also send back kChatRoomNameUpdatedMessage or kChatRoomPasswordUpdatedMessage to
the other users. This means that the response must update the 'User Last Active Time' and the messagesReceived() 
function must update it for the respective messages.

When a message is received, these message types will need to set it on the client.
 1) kTextMessage
 2) kPictureMessage
 3) kMimeTypeMessage
 4) kLocationMessage
 5) kInviteMessage
 6) kDifferentUserJoinedMessage
 7) kDifferentUserLeftMessage
 8) kUserKickedMessage (kicking user)
 9) kUserBannedMessage (banning user)
 10) kNewAdminPromotedMessage (promoting user)
 11) kChatRoomNameUpdatedMessage
 12) kChatRoomPasswordUpdatedMessage
 13) kUserActivityDetectedMessage

These functions set it on the server and so make sure the return of these sets the value on the client.
 1) createChatRoom()
 2) joinChatRoom()
 3) leaveChatRoom() NOTE: Not needed technically however because the client updates on kDifferentUserLeftMessage,
 to keep things consistent updated in the leaveChatRoom function on server as well.
 4) removeFromChatRoom() (for the account doing the kicking)
 5) promoteNewAdmin() (for the account doing the promoting)
 6) updateChatRoomInfo() (for the account doing the updating)
 7) unMatch() (for sending user) NOTE: Because this leaves the chat room there is nothing to do on the client.
 8) blockAndReportChatRoom() (only when un-matching and only for the account doing the blocking) NOTE: Because this
 leaves the chat room there is nothing to do on the client, it does not update for normal block & report actions 
 because user activity time because the chatRoom database is not accessed in the function.
 9) updateSingleChatRoomMember() (the user calling this should be updated)
 10) updateChatRoom() (the user calling this should be updated)

clientMessageToServer(); This function will set it on the server and so it return will need to be set on the client as
well. But only for certain messages.
 1) kTextMessage
 2) kPictureMessage
 3) kMimeTypeMessage
 4) kLocationMessage
 5) kInviteMessage
 6) kUserActivityDetectedMessage

---

## updating TypeOfChatMessage.MessageSpecifics

The protobuf message type MessageSpecifics may have things added to it. And in order to have a message added several
 places need to be updated. Depending on what the message does there could be more places, however this is a starting
 list of places that will need to be.

Client
 1) GlobalValues.messagesDaoSelectFinalMessageString
 2) GlobalValues.messagesDaoSelectChatRoomLastActiveTimeString
 3) ApplicationUtilities.kt for checkIfMessageTypeFitsFinalChatRoomMessage()
 4) ApplicationUtilities.kt for checkIfChatRoomLastActiveTimeRequiresUpdating()
 5) ChatRoomListChatRoomsAdapter.kt
 6) ChatStreamObject.kt convertTypeOfChatMessageToErrorString
 7) ChatStreamObject.kt receiveMessage

Server (If warnings are enabled, a lot of these will show up during compilation. May have to rebuild entire project.)
 1) convertChatMessageDocumentToChatMessageToClient() inside convert_chat_message_document_to_chat_message_to_client.cpp,
  sendMessageToChatRoom() inside send_message_to_chat_room.cpp and chat_room_message_keys.h may need new values added 
  to it if the message is passed from the server.
 2) Inside utility_general_functions.cpp isActiveMessageType() will probably need updated.
 3) convertMessageBodyTypeToString() inside convert_message_body_type_to_string.cpp will need updated. 
 4) A block inside chat_change_stream.cpp under beginChatChangeStream() needs updated.
 5) Inside client_message_to_server.cpp, there are TWO blocks that require updated (one will not show on warnings 
  because 'default' is used).

Server Testing
 1) chat_room_object.h will need it added as a new child of MessageSpecificsDoc.
 2) chat_room_message_doc.cpp will have a bunch of them.

NOTE: Also probably want to look around for where switch(on server)/when(on client) statements are used.
