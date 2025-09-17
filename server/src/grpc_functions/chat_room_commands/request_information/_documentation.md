## Request Chat Room Info Documentation

This file will cover requesting chat room information. Specifically it will cover
* updateChatRoom()
* updateSingleChatRoomMember()
* updateSingleOtherUser()
* updateSingleChatRoomMemberNotInChatRoom()
* buildBasicUpdateOtherUserResponse()

## General idea
There are some intricacies with what should be passed/returned from the two grpc functions updateChatRoom() and
updateSingleChatRoomMember(). This file will cover those as well as two of the functions they both call, namely 
updateSingleOtherUser() and updateSingleChatRoomMemberNotInChatRoom() which work to send back
the requested user and provide only the information that is out of date. <br/> 

Members end up being the most complex part to send back here. For updateChatRoom() and updateSingleChatRoomMember()
certain fields are guaranteed to be sent back with each member . Each member will be sent back using an
UpdateOtherUserResponse protobuf message. The following fields will be guaranteed to exist for each member sent back
(assuming the member requires an update, read below for specifics).
* return_status
* user_info.account_oid
* account_state
* timestamp_returned
* account_last_activity_time

## updateChatRoom

This function is meant to be run when the user clicks a chat room to check if anything is out of date with the users'
information. The function is meant to only send back the values that require updates. So for example if all info
matches for UpdateChatRoomInfo, UpdateChatRoomInfo will not be sent back (explained below). It will update several things

1. It will check the chat room name, chat room password, chat room last activity time, chat room pinned location and 
 the chat room event oid. If any of the three are out of date, it will send back all three inside an UpdateChatRoomInfo 
 message type.
2. It will check if there are any messages that have not been received since the last update time. It will then send
 back any messages inside a ChatMessagesList type. Note that this does NOT request backwards in time to avoid overlaps
 (the way extractChatRooms() does when the chat stream initializes). This is because any messages that were out of order
 should be in the process of being sent, and it would be quite a bit of overhead considering updateChatRoom() runs more
 often than functions like extractChatRooms().
3. A kUserActivityDetectedMessage is sent for the requesting user and the LAST_TIME_VIEWED is updated in the user 
 account to the same time. updateChatRoom() will send back the stored time of the message as a
 UpdateTimestampForUserActivityMessage.
4. All users as well as the chat room event inside the chat room will be compared with the users sent from the client. 
 This is by far the most complex part of the function. However, the idea is that if a user does not exist on the client
 the entire user will be sent back. If a user does exist on the client, the client info will be compared with the server
 info and any needed updates will be sent back.
5. The final type of message to be sent back will be an UpdateChatRoomAddendum. This is the only message on the list 
 that is guaranteed to be sent back as long as no error is returned, and the connection is stable. It will NOT always be
 the final message sent back (see below under 'Expected output' header for details).

### Expected output
For the expected order of messages see above. There are three ways the function can signal that it has ended.
1. It sends back an UpdateChatRoomResponse with ReturnStatus set, this represents an error occurred.
2. It sends back an UpdateChatRoomResponse with UpdateTimestampForUserActivityMessage set and the 
 user_exists_in_chat_room field is set to false.
3. It sends back an UpdateChatRoomResponse with UpdateChatRoomAddendum set. This means the function has completed
 sending back any information that required an update.

Output message types
* ChatMessagesList; This contains all messages that occurred after the 'chat_room_last_updated_time' field from the
 request.
* UpdateChatRoomInfo; This contains chat room name, chat room password and chat room last activity time. If this 
 message type is sent back then **all three fields will always be set, regardless of what specifically needs
 to be updated**.
* UpdateTimestampForUserActivityMessage; This contains the timestamp for the kUserActivityDetectedMessage that
 updateChatRoom() sends. It can also signal that the user was not inside the specified chat room if 
 user_exists_in_chat_room field is set to false.
* UpdateOtherUserResponse (member_response); This contains the information for a member that requires updates. Only
 information that is relevant is returned. See specifics on updateSingleOtherUser() and
 updateSingleChatRoomMemberNotInChatRoom() for more info.
* UpdateChatRoomAddendum; This will be the final message send back. It signals that the function completed successfully
 without any errors occurring.
* ReturnStatus; This will be sent back if an error occurred. No more messages will follow this message.
* UpdateQrCode; This will contain the QR Code and details for it.
* UpdateOtherUserResponse (event_response); This will contain information for the chat room event that is required to
 be updated.

NOTE: If more than chat_room_values::MAXIMUM_NUMBER_USERS_IN_CHAT_ROOM_TO_REQUEST_ALL_INFO members are currently
inside the chat room (ACCOUNT_STATE_IS_ADMIN or ACCOUNT_STATE_IN_CHAT_ROOM). Then the requested users will only have
their thumbnail sent back (if needed), not all picture information. Otherwise, all picture information will be checked.

### Expected input
The input is the UpdateChatRoomRequest protobuf message.
* The user must already be inside the chat room (ACCOUNT_STATE_IS_ADMIN or ACCOUNT_STATE_IN_CHAT_ROOM).
* LoginToServerBasicInfo must always be filled out. The user must also have completed the login process (filled out
  all information such as pictures and name).
* chat_room_id must always be filled out with a valid chat room id.
* The client should send back every member of the chat room **including the sending member** and the event with the
 message.
* chat_room_name and chat_room_password will take any input smaller than 
 server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES.
* chat_room_last_activity_time and chat_room_last_updated_time can take any input. It will be converted to a time that
 is -1 <= x <= currentTimestamp.
* pinned_location_longitude, pinned_location_latitude, qr_code_last_timestamp and event_oid should be set to their 
 default values if it is not an event chat room. The default values are respectively
 chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE, chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE,
 chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT and chat_room_values::EVENT_ID_DEFAULT.
* Members are sent in through OtherUserInfoForUpdates. Any members that are sent will be checked for updates. Any 
 members that are inside the chat room and were not sent to the server will be sent back in full (based on
 whether the user is still inside the chat room or not). The chat room event should be included here as well.

OtherUserInfoForUpdates has several fields.
* If a duplicate account_oid is sent, it will be ignored.
* If the account_state is incorrect it will be updated.
* thumbnail_size_in_bytes, thumbnail_index_number and thumbnail_timestamp will be checked together to see if the
 users' thumbnail requires an update.
* first_name and age will be returned if they are incorrect.
* member_info_last_updated_timestamp will be checked inside updateSingleOtherUser() to find any other out of date info.
* All pictures should be stored inside pictures_last_updated_timestamps. Duplicate PictureIndexInfo.index_Number will
 be ignored. 
* event_status should be set to LetsGoEventStatus::NOT_AN_EVENT for users and the proper event status for events.
* event_title should be set to the event title if an event is being requested or left blank if a user.

### Possible operations
These are here for reference purposes. Requesting updates for members turns out to be complex. These each represent a
possibility.

Possibilities of comparing users inside header to users passed from client. 'in chat room' means ACCOUNT_STATE_IS_ADMIN
or ACCOUNT_STATE_IN_CHAT_ROOM. 'NOT in chat room' means ACCOUNT_STATE_NOT_IN_CHAT_ROOM or ACCOUNT_STATE_BANNED. Members
requiring user account document will be added to a vector. The values inside this vector will then be extracted from the
database in mass after all users in header have been compared to the users from client.
- member in header && passed from client && NOT current user && in chat room; REQUIRES_UPDATE (user added to vector to be extracted from database)
- member in header && passed from client && NOT current user && NOT in chat room; updateSingleChatRoomMemberNotInChatRoom()
- member in header && passed from client && current user && account state does not match; buildBasicUpdateOtherUserResponse() for current member
- member NOT in header && passed from client && member NOT this user && in chat room; buildBasicUpdateOtherUserResponse() for NOT_IN_CHAT_ROOM
- member NOT in header && passed from client && member NOT this user && NOT in chat room; do nothing~
- member NOT in header && passed from client && member is this user; buildBasicUpdateOtherUserResponse() for current member (it should never be able to hit the point, it will say user not in chat room when trying to find in database)
- member in header && NOT passed from client && member NOT this user && in chat room; REQUIRES_ADDED (user added to vector to be extracted from database)
- member in header && NOT passed from client && member NOT this user && NOT in chat room; updateSingleChatRoomMemberNotInChatRoom()
- member in header && NOT passed from client && member is this user; buildBasicUpdateOtherUserResponse() for current member

After members stored in vector have been requested from database.
- member requested && user doc found && REQUIRES_UPDATE; updateSingleOtherUser()
- member requested && user doc found && REQUIRES_ADDED; saveUserInfoToMemberSharedInfoMessage()
- member requested && user doc NOT found && member NOT current user; buildBasicUpdateOtherUserResponse() for NOT_IN_CHAT_ROOM
- member requested && user doc NOT found && member is current user; buildBasicUpdateOtherUserResponse() for current member

## updateSingleChatRoomMember

The function is meant to be run when the user views a user info card that they do not have full information on. This
function will send back the necessary updates for a passed user. It is very similar to when a member is sent back 
through updateChatRoom() (the same functions are used, see below). However, it will always set at least the guaranteed
info (see above under 'General Idea' header). Any other info can be optionally set if it requires an updated

NOTE: The user can safely request themselves (there was no reason to restrict it at the time of this writing) although
this is very similar to what happens on a login. 

### Expected output
The response type is UpdateOtherUserResponse. This contains the information for the requested member. The message will
always contain the guaranteed info (see above under 'General Idea' header). However, after that only information that
requires an update is returned. See specifics on updateSingleOtherUser() and updateSingleChatRoomMemberNotInChatRoom()
for more info.

### Expected input
The input is the UpdateSingleChatRoomMemberRequest protobuf message.
* The user must already be inside the chat room (ACCOUNT_STATE_IS_ADMIN or ACCOUNT_STATE_IN_CHAT_ROOM).
* LoginToServerBasicInfo must always be filled out. The user must also have completed the login process (filled out
all information such as pictures and name).
* chat_room_id must always be filled out with a valid chat room id.
* OtherUserInfoForUpdates requirements are identical to updateChatRoom() except an invalid account oid or invalid 
account_state will cause the function to return an error.

### Possible operations
These are here for reference purposes. Requesting updates for members turns out to be complex. These each represent a
possibility.

Assuming the sending user is inside the chat room. These are the possibilities based on the target members' status.
* member in header && in chat room && member user account exists && chat room exists in member user account; 
updateSingleOtherUser()
* member in header && in chat room && member user account exists && chat room does NOT exist in member user account;
buildBasicUpdateOtherUserResponse()
* member in header && in chat room && member user account does NOT exist; buildBasicUpdateOtherUserResponse()
* member in header && NOT in chat room; updateSingleChatRoomMemberNotInChatRoom()
* member NOT in header; buildBasicUpdateOtherUserResponse()

## updateSingleChatRoomMemberNotInChatRoom()

This function is run when the user is not inside the chat room (ACCOUNT_STATE_NOT_IN_CHAT_ROOM or ACCOUNT_STATE_BANNED).
The idea behind this function is that it will request enough information for the users' messages to still be shown 
inside a chat room. However, not enough information for the member themselves to be shown. <br/>

It will check a few different things.
* The requested users' account state inside the chat room.
* The requested users' first name.
* The requested users' thumbnail.

If a user requires an update, certain fields inside UpdateOtherUserResponse are filled out. If user does not require an
update, nothing will be sent back.
* The guaranteed fields, see 'General idea' header for more info.
* Thumbnail Values; **The values below will only be set if the thumbnail requires an update.** If the thumbnail was
 deleted, the values from DeletedThumbnailInfo will be sent back in place of these (see 
 DeletedThumbnailInfo::saveDeletedThumbnailInfo for the actual values).
    - user_info.account_thumbnail_size; Set to the length of the thumbnail to check for corrupted files.
    - user_info.account_thumbnail_timestamp; Set to the timestamp the thumbnail was stored inside the header.
    - user_info.account_thumbnail; Set to the thumbnail in bytes.
    - user_info.account_thumbnail_index; Set to the index of the thumbnail for the requested user.

## updateSingleOtherUser()

This function is run when the user is inside the chat room (ACCOUNT_STATE_IS_ADMIN or ACCOUNT_STATE_IN_CHAT_ROOM) or
when an update for a match is requested by the client. This function will check and update all user information. Except
the thumbnail and pictures, if any of the user information requires an update, **all of it will be sent
back** (again, except the thumbnail and pictures). The thumbnail and pictures will be checked and sent back based on
the parameters of the function. <br/>

If any user information requires an update (checked against member_info_last_updated inside the requested member) then
it will all be updated by calling the function saveUserInfoToMemberSharedInfoMessage(). <br/>

If the thumbnail requires an update it will send back the new thumbnail. If the thumbnail has been deleted (all 
pictures have been removed for example) then it will send back the values from 
DeletedThumbnailInfo::saveDeletedThumbnailInfo. If any picture require an update it will send back the new information,
if the picture has been removed (by an admin, replacing it will send back the update) it will send back the values from
DeletedPictureInfo::saveDeletedPictureInfo. Pictures are updated individually, for example if a user account contains
three picture and only picture index 2 requires an update, only picture index 2 will be sent back none of the others. 
<br/>

If any info is sent back (pictures, thumbnail or account info) all the guaranteed fields are sent back as well. See
'General idea' header for more info.

## buildBasicUpdateOtherUserResponse()

This function is set up to provide the fields that are 'guaranteed'. See above under 'General idea' Header.
