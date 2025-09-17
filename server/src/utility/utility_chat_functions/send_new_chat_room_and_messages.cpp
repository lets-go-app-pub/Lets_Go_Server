//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <helper_functions/messages_to_client.h>
#include <utility_testing_functions.h>

#include "utility_chat_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "store_and_send_messages.h"
#include "database_names.h"
#include "user_account_keys.h"
#include "chat_room_values.h"
#include "chat_room_header_keys.h"
#include "update_single_other_user.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//writes the chat room and all messages to the passed lambda writeResponseToClient
bool sendNewChatRoomAndMessages(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& userAccountsCollection,
        mongocxx::collection& chatRoomCollection,
        const bsoncxx::document::view& userAccountDocView,
        const std::string& chatRoomId,
        const std::chrono::milliseconds& timeChatRoomLastObserved,
        const std::chrono::milliseconds& currentTimestamp,
        const std::string& userAccountOID,
        StoreAndSendMessagesVirtual* storeAndSendMessagesToClient,
        const HowToHandleMemberPictures requestPictures,
        AmountOfMessage amountOfMessageInfoToRequest,
        bool requestFinalFewMessagesInFull,
        bool do_not_update_user_state,
        bool from_user_swiped_yes_on_event,
        const std::chrono::milliseconds& time_to_request_before_or_equal,
        const std::string& message_uuid_to_exclude,
        const std::function<bool()>& run_after_messages_requested
) {

    ChatMessageToClient responseMsg;
    responseMsg.set_sent_by_account_id(userAccountOID);

    auto chatMessage = responseMsg.mutable_message();
    auto thisUserJoinedChatRoomStart = chatMessage->mutable_message_specifics()->mutable_this_user_joined_chat_room_start_message();

    chatMessage->mutable_standard_message_info()->set_internal_force_send_message_to_current_user(true);
    chatMessage->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chatRoomId);

    bsoncxx::stdx::optional<bsoncxx::document::value> find_chat_header;
    try {

        mongocxx::options::find opts{};

        //NOTE: this is an exclusion projection, it will only project the last element of TIMES_JOINED_LEFT
        // the slice operator naturally acts an exclusion operator, if paired ONLY with _id : 0 then it will exclude all but the
        // last element of the array
        opts.projection(
            document{}
                << "_id" << 0
                << std::string(chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM).append(".").append(chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT) << open_document
                    << "$slice" << -1
                << close_document
            << finalize
        );

        //NOTE: if I access a collection that doesn't exist findChatHeader will simply not be set
        find_chat_header = chatRoomCollection.find_one(
            document{}
                << "_id" << chat_room_header_keys::ID
                << finalize,
            opts
        );
    }
    catch (const mongocxx::logic_error& e) {
        const std::string error_string = "Exception was thrown when attempting to find a chat room.\n";

        std::optional<std::string> chatRoomExceptionString = e.what();
        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            chatRoomExceptionString, error_string,
            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
            "collection_name", chatRoomCollection.name().to_string(),
            "verified_account_doc", userAccountDocView
        );
        return false;
    }

    if (find_chat_header) { //if chat room was found

        try {
            const bsoncxx::document::view chat_room_header_doc = find_chat_header->view();
            std::string event_oid_str;

            ChatRoomInfoMessage* const chat_room_info = thisUserJoinedChatRoomStart->mutable_chat_room_info();
            chat_room_info->set_chat_room_id(chatRoomId);
            chat_room_info->set_chat_room_last_observed_time(timeChatRoomLastObserved.count());

            chat_room_info->set_chat_room_name(
                    extractFromBsoncxx_k_utf8(
                            chat_room_header_doc,
                            chat_room_header_keys::CHAT_ROOM_NAME
                    )
            );

            chat_room_info->set_chat_room_password(
                    extractFromBsoncxx_k_utf8(
                            chat_room_header_doc,
                            chat_room_header_keys::CHAT_ROOM_PASSWORD
                    )
            );

            chat_room_info->set_chat_room_last_activity_time(
                    extractFromBsoncxx_k_date(
                            chat_room_header_doc,
                            chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME
                    ).value.count()
            );

            const auto event_id_element = chat_room_header_doc[chat_room_header_keys::EVENT_ID];
            if (event_id_element && event_id_element.type() == bsoncxx::type::k_oid) {
                event_oid_str = event_id_element.get_oid().value.to_string();
            }
            else if(!event_id_element) { //if element does not exist
                event_oid_str = chat_room_values::EVENT_ID_DEFAULT;
            }
            else { //if element exists is not type oid
                logElementError(
                        __LINE__, __FILE__,
                        event_id_element, chat_room_header_doc,
                        bsoncxx::type::k_oid, chat_room_header_keys::EVENT_ID,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chatRoomCollection.name().to_string()
                );
                return false;
            }

            chat_room_info->set_event_oid(event_oid_str);

            const auto pinned_location_element = chat_room_header_doc[chat_room_header_keys::PINNED_LOCATION];
            if (pinned_location_element && pinned_location_element.type() == bsoncxx::type::k_document) {
                const bsoncxx::document::view pinned_location_document = pinned_location_element.get_document().view();

                chat_room_info->set_longitude_pinned_location(
                        extractFromBsoncxx_k_double(
                                pinned_location_document,
                                chat_room_header_keys::pinned_location::LONGITUDE
                        )
                );
                chat_room_info->set_latitude_pinned_location(
                        extractFromBsoncxx_k_double(
                                pinned_location_document,
                                chat_room_header_keys::pinned_location::LATITUDE
                        )
                );
            }
            else if(!pinned_location_element) { //if element does not exist
                chat_room_info->set_longitude_pinned_location(chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE);
                chat_room_info->set_latitude_pinned_location(chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE);
            }
            else { //if element is not type document
                logElementError(
                        __LINE__, __FILE__,
                        pinned_location_element, chat_room_header_doc,
                        bsoncxx::type::k_document, chat_room_header_keys::PINNED_LOCATION,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chatRoomCollection.name().to_string()
                );
                return false;
            }

            std::string matching_oid_string;
            auto chatRoomIsMatchMadeElement = chat_room_header_doc[chat_room_header_keys::MATCHING_OID_STRINGS];
            if(!chatRoomIsMatchMadeElement) {
                logElementError(
                        __LINE__, __FILE__,
                        chatRoomIsMatchMadeElement, chat_room_header_doc,
                        bsoncxx::type::k_null, chat_room_header_keys::MATCHING_OID_STRINGS,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chatRoomCollection.name().to_string()
                );
                return false;
            }

            if (chatRoomIsMatchMadeElement.type() == bsoncxx::type::k_array
                ) { //if this is type array (this means it is a matching chat room without first contact made yet)

                bsoncxx::array::view chatRoomMatch = chatRoomIsMatchMadeElement.get_array().value;
                int index = 0;

                for (const auto& matchOID : chatRoomMatch) {

                    if (matchOID && matchOID.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8

                        std::string tempMatchingOIDString = matchOID.get_string().value.to_string();
                        if (userAccountOID != tempMatchingOIDString) { //if value is NOT this OID
                            matching_oid_string = tempMatchingOIDString;
                            chat_room_info->set_match_made_chat_room_oid(matching_oid_string);
                        }

                    } else { //if element does not exist or is not type utf8

                        const std::string error_string = "An oid inside MATCHING_OID_STRINGS was not type oid\n";

                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "verifiedAccount", userAccountDocView,
                                "index", index,
                                "array", chatRoomMatch,
                                "match_oid", (!matchOID ? "does_not_exist" : convertBsonTypeToString(matchOID.type()))
                        );

                        continue;
                    }

                    index++;
                }

                if (index != 2) {
                    const std::string error_string =
                            "There was an incorrect number of OIDs in a chat room that was only for matching; there should be 2 instead there were " +
                            std::to_string(index) + ".\n";

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", chatRoomCollection.name().to_string(),
                            "verifiedAccount", userAccountDocView,
                            "index", index,
                            "array", chatRoomMatch
                    );

                    //NOTE: ok to continue here, the entire array will be set to null if the match goes through
                }

            } else if (chatRoomIsMatchMadeElement.type() != bsoncxx::type::k_null) {  //if element is wrong type
                logElementError(
                        __LINE__, __FILE__,
                        chatRoomIsMatchMadeElement, chat_room_header_doc,
                        bsoncxx::type::k_null, chat_room_header_keys::MATCHING_OID_STRINGS,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chatRoomCollection.name().to_string()
                );
                return false;
            }

            const bsoncxx::array::view accountsInChatRoom = extractFromBsoncxx_k_array(
                    chat_room_header_doc,
                    chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
            );
            int userIndex = -1;

            //find and save the calling users info to the message
            //NOTE: need to make sure to send the chat room info back as the first message, but if I iterate through I
            // will need to store everyone in RAM before the user ideally these arrays shouldn't be too long anyway.
            int index = 0;
            std::string currentMemberOIDString;
            for (const auto& user_account_array_ele : accountsInChatRoom) {

                const bsoncxx::document::view user_account_doc = extractFromBsoncxxArrayElement_k_document(
                        user_account_array_ele
                );

                currentMemberOIDString = extractFromBsoncxx_k_oid(
                        user_account_doc,
                        chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                ).to_string();

                if (currentMemberOIDString == userAccountOID) {  //if the member is this account

                    userIndex = index;

                    //save current user account state
                    chat_room_info->set_account_state(
                            AccountStateInChatRoom(
                                    extractFromBsoncxx_k_int32(
                                            user_account_doc,
                                            chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                                    )
                            )
                    );

                    //get last activity time for this user
                    chat_room_info->set_user_last_activity_time(
                            extractFromBsoncxx_k_date(
                                    user_account_doc,
                                    chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME
                            ).value.count()
                    );

                    //get time user joined
                    const bsoncxx::array::view timesUserJoinedLeft = extractFromBsoncxx_k_array(
                            user_account_doc,
                            chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT
                    );

                    //there should be at most 1 element inside this array due to projection
                    std::chrono::milliseconds timestamp = std::chrono::milliseconds{-1L};
                    if(!timesUserJoinedLeft.empty()) {
                        if (timesUserJoinedLeft[0].type() == bsoncxx::type::k_date) {
                            timestamp = timesUserJoinedLeft[0].get_date().value;
                        } else {

                            const std::string error_string = "The first value inside TIMES_JOINED_LEFT did not exist or was the wrong type.";

                            storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "verified_account", userAccountDocView,
                                "index", index,
                                "times_user_joined_left", timesUserJoinedLeft,
                                "index_0", convertBsonTypeToString(timesUserJoinedLeft[0].type())
                            );

                            continue;
                        }
                    }

                    chat_room_info->set_time_joined(timestamp.count());

                    break;
                }

                index++;
            }

            bsoncxx::stdx::optional<bsoncxx::document::value> find_event_user_acct_doc;
            std::chrono::milliseconds event_expiration_time{general_values::event_expiration_time_values::USER_ACCOUNT};
            if(event_oid_str != chat_room_values::EVENT_ID_DEFAULT) { //If this is an event chat room.

                const bsoncxx::oid event_oid{event_oid_str};
                std::optional<std::string> find_user_acct_exception_string;
                try {
                    mongocxx::options::find opts;
                    opts.projection(buildUpdateSingleOtherUserProjectionDoc());

                    //find user account document
                    find_event_user_acct_doc = userAccountsCollection.find_one(
                        document{}
                            << "_id" << event_oid
                        << finalize,
                        opts
                    );
                }
                catch (const mongocxx::logic_error& e) {
                    find_user_acct_exception_string = e.what();
                }

                if(!find_event_user_acct_doc) {
                    //NOTE; Technically this could happen if the other user say deleted their account between this call
                    // and the login.
                    const std::string error_string = "Failed to find user account.\n";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            find_user_acct_exception_string, error_string,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection_name", chatRoomCollection.name().to_string(),
                            "user_account_doc", userAccountDocView
                    );
                }
                else {
                    const bsoncxx::document::view event_user_account = *find_event_user_acct_doc;

                    event_expiration_time =
                            extractFromBsoncxx_k_date(
                                    event_user_account,
                                    user_account_keys::EVENT_EXPIRATION_TIME
                            ).value;

                    if(event_expiration_time == general_values::event_expiration_time_values::USER_ACCOUNT.value) {
                        const std::string error_string = "An event had an EVENT_EXPIRATION_TIME set to the USER_ACCOUNT value.\n";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                find_user_acct_exception_string, error_string,
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection_name", chatRoomCollection.name().to_string(),
                                "user_account_doc", userAccountDocView,
                                "event_user_account_doc", event_user_account
                        );

                        //NOTE: Ok to continue here.
                    }
                    else {
                        if (event_expiration_time ==
                                 general_values::event_expiration_time_values::EVENT_CANCELED.value
                             || currentTimestamp >= event_expiration_time
                                 ) { //event is canceled or expired
                            chat_room_info->set_qr_code_image_bytes(chat_room_values::QR_CODE_DEFAULT);
                            chat_room_info->set_qr_code_message(chat_room_values::QR_CODE_MESSAGE_DEFAULT);
                            chat_room_info->set_qr_code_timestamp(chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT);
                        } else { //event is ongoing
                            const auto qr_code_element = chat_room_header_doc[chat_room_header_keys::QR_CODE];
                            const auto qr_code_message_element = chat_room_header_doc[chat_room_header_keys::QR_CODE_MESSAGE];
                            const auto qr_code_time_updated_element = chat_room_header_doc[chat_room_header_keys::QR_CODE_TIME_UPDATED];
                            if (qr_code_element && qr_code_element.type() == bsoncxx::type::k_utf8
                                && qr_code_message_element && qr_code_message_element.type() == bsoncxx::type::k_utf8
                                && qr_code_time_updated_element && qr_code_time_updated_element.type() == bsoncxx::type::k_date) {

                                chat_room_info->set_qr_code_image_bytes(
                                        qr_code_element.get_string().value.to_string()
                                );

                                chat_room_info->set_qr_code_message(
                                        qr_code_message_element.get_string().value.to_string()
                                );

                                chat_room_info->set_qr_code_timestamp(
                                        qr_code_time_updated_element.get_date().value.count()
                                );
                            }
                            else if(!qr_code_element && !qr_code_message_element && !qr_code_time_updated_element) { //if element(s) are wrong type
                                chat_room_info->set_qr_code_image_bytes(chat_room_values::QR_CODE_DEFAULT);
                                chat_room_info->set_qr_code_message(chat_room_values::QR_CODE_MESSAGE_DEFAULT);
                                chat_room_info->set_qr_code_timestamp(chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT);
                            } else {
                                const std::string error_string = "An element was the wrong type inside the chat room header.\n";
                                storeMongoDBErrorAndException(
                                        __LINE__, __FILE__,
                                        std::optional<std::string>(), error_string,
                                        "chat_room_id", chatRoomId,
                                        "user_account_oid", userAccountOID,
                                        "event_oid_str", event_oid_str,
                                        "chat_room_header_doc", chat_room_header_doc
                                );
                                //This will stop the event from being set back.
                                event_oid_str = "";
                            }
                        }
                    }
                }
            }

            /** Start THIS_USER_JOINED_CHAT_ROOM_START_MESSAGE messages here **/
            storeAndSendMessagesToClient->sendMessage(responseMsg);

            //chatMessage->set_message_type(MessageInstruction::THIS_USER_JOINED_CHAT_ROOM_MEMBER);
            auto thisUserJoinedChatRoomMember = chatMessage->mutable_message_specifics()->mutable_this_user_joined_chat_room_member_message();
            chatMessage->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chatRoomId);

            if(event_oid_str != chat_room_values::EVENT_ID_DEFAULT
                && !event_oid_str.empty()
                && find_event_user_acct_doc
                && event_expiration_time != general_values::event_expiration_time_values::USER_ACCOUNT.value
            ) { //If this is an event chat room and no errors occurred when extracting it above.

                auto chatRoomMemberInfo = thisUserJoinedChatRoomMember->mutable_member_info();
                chatRoomMemberInfo->Clear();
                auto userInfo = chatRoomMemberInfo->mutable_user_info();

                userInfo->set_account_oid(event_oid_str);

                //Send preset values back.
                chatRoomMemberInfo->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_EVENT);
                chatRoomMemberInfo->set_account_last_activity_time(chat_room_values::EVENT_USER_LAST_ACTIVITY_TIME_DEFAULT.count());

                const bsoncxx::document::view event_user_account = *find_event_user_acct_doc;

                HowToHandleMemberPictures request_event_pictures = requestPictures;

                if (event_expiration_time ==
                    general_values::event_expiration_time_values::EVENT_CANCELED.value
                    || currentTimestamp >= event_expiration_time
                ) { //event is canceled or expired
                    request_event_pictures = HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO;
                }

                if (saveUserInfoToMemberSharedInfoMessage(
                        mongoCppClient,
                        accountsDB,
                        userAccountsCollection,
                        event_user_account,
                        bsoncxx::oid{event_oid_str},
                        userInfo,
                        request_event_pictures,
                        currentTimestamp)
                        ) {
                    /** Sending THIS_USER_JOINED_CHAT_ROOM_MEMBER message here **/
                    storeAndSendMessagesToClient->sendMessage(responseMsg);
                }
            }

            //Want the index to be incremented first so that it doesn't need to be set before each continue statement.
            index = -1;
            for (const auto& user_account_array_ele : accountsInChatRoom) {
                index++;

                if (index == userIndex) { //if this is the user account
                    continue;
                }

                const bsoncxx::document::view user_account_doc = extractFromBsoncxxArrayElement_k_document(
                        user_account_array_ele
                );

                std::string member_oid_string;
                auto chatRoomMemberInfo = thisUserJoinedChatRoomMember->mutable_member_info();
                chatRoomMemberInfo->Clear();
                auto userInfo = chatRoomMemberInfo->mutable_user_info();

                member_oid_string = extractFromBsoncxx_k_oid(
                        user_account_doc,
                        chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                ).to_string();

                userInfo->set_account_oid(member_oid_string);

                if (member_oid_string == userAccountOID) { //if this is not the user account
                    const std::string error_string = "User account was found when it should not have been\n."
                                                     "(index != userIndex) was already checked.\n";

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection_name", chatRoomCollection.name().to_string(),
                            "array_document", user_account_doc,
                            "verified_account_doc", userAccountDocView,
                            "index", std::to_string(index)
                    );

                    //NOTE: OK to continue here
                    continue;
                }

                //save account state
                chatRoomMemberInfo->set_account_state(
                        AccountStateInChatRoom(
                                extractFromBsoncxx_k_int32(
                                        user_account_doc,
                                        chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                                )
                        )
                );

                userInfo->set_account_type(
                        UserAccountType::USER_ACCOUNT_TYPE
                );

                if (chatRoomMemberInfo->account_state() == AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
                    || chatRoomMemberInfo->account_state() == AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                ) { //if this account is inside chat room

                    //save member data
                    const bsoncxx::oid member_oid = bsoncxx::oid{member_oid_string};

                    std::optional<std::string> find_user_acct_exception_string;
                    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_acct_doc;
                    try {
                        mongocxx::options::find opts;
                        opts.projection(buildUpdateSingleOtherUserProjectionDoc());

                        //find user account document
                        find_user_acct_doc = userAccountsCollection.find_one(
                            document{}
                                << "_id" << member_oid
                            << finalize,
                            opts
                        );
                    }
                    catch (const mongocxx::logic_error& e) {
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                find_user_acct_exception_string, e.what(),
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection_name", chatRoomCollection.name().to_string(),
                                "verified_account_doc", userAccountDocView
                        );

                        continue;
                    }

                    if(!find_user_acct_doc) {
                        //NOTE; technically this could happen if the other user say deleted their account between this call and the log in
                        const std::string error_string = "Failed to find user verified account.\n";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                find_user_acct_exception_string, error_string,
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection_name", chatRoomCollection.name().to_string(),
                                "verified_account_doc", userAccountDocView
                        );

                        continue;
                    }

                    bsoncxx::document::view member_user_account = *find_user_acct_doc;

                    chatRoomMemberInfo->set_account_last_activity_time(
                            extractFromBsoncxx_k_date(
                                    user_account_doc,
                                    chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME
                            ).value.count()
                    );

                    HowToHandleMemberPictures member_pictures_request = requestPictures;
                    if (chatRoomIsMatchMadeElement.type() !=
                        bsoncxx::type::k_null) { //if this is a 'match made' account

                        //always request all pictures for match made accounts
                        member_pictures_request = HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO;
                    }

                    if (!saveUserInfoToMemberSharedInfoMessage(
                            mongoCppClient,
                            accountsDB,
                            userAccountsCollection,
                            member_user_account,
                            member_oid,
                            userInfo,
                            member_pictures_request,
                            currentTimestamp)
                    ) {
                        //NOTE: error has already been stored here
                        continue;
                    }

                }
                else { //if this account is not inside the chat room

                    userInfo->set_account_name(
                            extractFromBsoncxx_k_utf8(
                                    user_account_doc,
                                    chat_room_header_keys::accounts_in_chat_room::FIRST_NAME
                            )
                    );

                    const std::chrono::milliseconds thumbnail_timestamp = extractFromBsoncxx_k_date(
                                    user_account_doc,
                                    chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP
                    ).value;

                    const int thumbnail_size = extractFromBsoncxx_k_int32(
                            user_account_doc,
                            chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE
                    );

                    if(!extractThumbnailFromHeaderAccountDocument(
                            mongoCppClient,
                            accountsDB,
                            chatRoomCollection,
                            user_account_doc,
                            currentTimestamp,
                            userInfo,
                            member_oid_string,
                            thumbnail_size,
                            thumbnail_timestamp
                    )) {
                        //error already stored
                        continue;
                    }

                }

                /** Sending THIS_USER_JOINED_CHAT_ROOM_MEMBER message here **/
                storeAndSendMessagesToClient->sendMessage(responseMsg);
            }

            bsoncxx::builder::basic::array excluded_uuid_array_builder;
            if(!message_uuid_to_exclude.empty()) {
                excluded_uuid_array_builder.append(message_uuid_to_exclude);
            }

            //stream response to client sending message from chat room
            streamInitializationMessagesToClient(
                    mongoCppClient,
                    accountsDB,
                    chatRoomCollection,
                    userAccountsCollection,
                    userAccountDocView,
                    chatRoomId,
                    userAccountOID,
                    storeAndSendMessagesToClient,
                    amountOfMessageInfoToRequest,
                    DifferentUserJoinedChatRoomAmount::SKELETON, //members were already stored above
                    do_not_update_user_state,
                    true, //this must always be set to true, otherwise messages like USER_KICKED_MESSAGE will actually kick the user when being downloaded
                    requestFinalFewMessagesInFull,
                    std::chrono::milliseconds{-1L},
                    excluded_uuid_array_builder,
                    time_to_request_before_or_equal
            );

            if(run_after_messages_requested && !run_after_messages_requested()) {
                return false;
            }

            std::string memberOIDString;
            chatMessage->mutable_standard_message_info()->set_do_not_update_user_state(do_not_update_user_state);

            if(from_user_swiped_yes_on_event) {
                //This will signal the device to display the match to the user as an 'Event Found'.
                chatMessage
                        ->mutable_message_specifics()
                        ->mutable_this_user_joined_chat_room_finished_message()
                        ->set_yes_swipe_event_oid(
                            event_oid_str
                        );
            } else if (!isInvalidOIDString(matching_oid_string)) {
                chatMessage
                        ->mutable_message_specifics()
                        ->mutable_this_user_joined_chat_room_finished_message()
                        ->set_match_made_chat_room_oid(
                            matching_oid_string
                        );
            } else {
                //Make sure this is still set to finished message.
                chatMessage
                        ->mutable_message_specifics()
                        ->mutable_this_user_joined_chat_room_finished_message()
                        ->clear_match_made_chat_room_oid();
            }

            chatMessage
                    ->mutable_standard_message_info()
                    ->set_chat_room_id_message_sent_from(
                        chatRoomId
                    );

            /** Sending THIS_USER_JOINED_CHAT_ROOM_FINISHED message here **/
            storeAndSendMessagesToClient->sendMessage(std::move(responseMsg));

        } catch (const ErrorExtractingFromBsoncxx& e) {
            //Error already stored
            return false;
        }
    }
    else { //if chat room was not found; however it was found in user account

        //NOTE: this is technically possible if for example this thread requests the chat rooms from verified, then another thread
        // runs delete chat room then this attempts to access the chat room
        const std::string error_string = "Exception was thrown when attempting to find a chat room. Chat room should be checked to exist BEFORE this function is called\n";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection_name", chatRoomCollection.name().to_string(),
                "verified_account_doc", userAccountDocView
        );

        std::optional<std::string> remove_from_user_acct_exception_string;
        bsoncxx::stdx::optional<mongocxx::result::update> remove_from_user_acct;
        try {
            //find logged in account document
            remove_from_user_acct = userAccountsCollection.update_one(
                   document{}
                       << "_id" << bsoncxx::oid{userAccountOID}
                   << finalize,
                   document{}
                       << "$pull" << open_document
                           << user_account_keys::CHAT_ROOMS << open_document
                               << user_account_keys::chat_rooms::CHAT_ROOM_ID << bsoncxx::types::b_string{chatRoomId}
                           << close_document
                       << close_document
                   << finalize);
        }
        catch (const mongocxx::logic_error& e) {
            remove_from_user_acct_exception_string = e.what();
        }

        if (!remove_from_user_acct) { //if update failed

            const std::string remove_from_user_acct_error_string = "Removing chat room value from user account failed.\n";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    remove_from_user_acct_exception_string, remove_from_user_acct_error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection_name", chatRoomCollection.name().to_string(),
                    "verified_account_doc", userAccountDocView
            );

            return false;
        }
    }

    return true;
}
