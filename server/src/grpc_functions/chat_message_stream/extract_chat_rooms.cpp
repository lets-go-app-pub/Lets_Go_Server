//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <how_to_handle_member_pictures.h>
#include <utility_chat_functions.h>
#include <store_and_send_messages.h>
#include <connection_pool_global_variable.h>
#include <helper_functions/messages_to_client.h>
#include <send_messages_implementation.h>
#include <utility_testing_functions.h>

#include "chat_message_stream.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "chat_room_header_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//essentially 'counts' in base 26, the 26 digits are the lower case letters
//NOTE: the 'most significant digit' would be the final char not the first
void generateNextElementName(std::string& previous_element_name) {

    static const char FIRST_CHARACTER = 'a';
    static const char FINAL_CHARACTER = 'z';
    bool modified_string = false;

    //increment next char
    for (char& c : previous_element_name) {
        if (c != FINAL_CHARACTER) {
            c++;
            modified_string = true;
            break;
        }
    }

    if (!modified_string) { //if all chars were the final character

        //set all current characters to first char
        for (char& c : previous_element_name) {
            c = FIRST_CHARACTER;
        }

        //append another first char
        previous_element_name += FIRST_CHARACTER;
    }
}

UserAccountChatRoomComparison compareUserAccountChatRoomsStates(
        std::vector<std::string> pre_chat_room_ids_inside_user_account_doc,
        mongocxx::collection& user_account_collection,
        const bsoncxx::oid& user_account_oid
        ) {

    //find user account document with only chat rooms projected
    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account_chat_rooms_value;
    std::optional<std::string> find_chat_rooms_exception_string;
    try {

        mongocxx::options::find opts;

        opts.projection(
                document{}
                        << "_id" << 0
                        << std::string(user_account_keys::CHAT_ROOMS).append(".").append(user_account_keys::chat_rooms::CHAT_ROOM_ID) << 1
                        << finalize
        );

        find_user_account_chat_rooms_value = user_account_collection.find_one(
                document{}
                        << "_id" << user_account_oid
                        << finalize,
                opts);

    }
    catch (const mongocxx::logic_error& e) {
        find_chat_rooms_exception_string = std::string(e.what());
    }

    if(!find_user_account_chat_rooms_value) {
        std::string error_string = "User account document was not found immediately after it was previously found.";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__, find_chat_rooms_exception_string,
                error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "_id", user_account_oid
        );

        return UserAccountChatRoomComparison::CHAT_ROOM_COMPARISON_ERROR;
    }

    bsoncxx::document::view find_user_account_chat_rooms_view = find_user_account_chat_rooms_value->view();
    bsoncxx::array::view chatRoomIds;

    auto postChatRoomsElement = find_user_account_chat_rooms_view[user_account_keys::CHAT_ROOMS];
    if (postChatRoomsElement &&
        postChatRoomsElement.type() == bsoncxx::type::k_array) { //if element exists and is type array
        chatRoomIds = postChatRoomsElement.get_array().value;
    } else { //if element does not exist or is not type array
        logElementError(__LINE__, __FILE__, postChatRoomsElement,
                        find_user_account_chat_rooms_view, bsoncxx::type::k_array,
                        chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID,
                        database_names::ACCOUNTS_DATABASE_NAME,
                        collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return UserAccountChatRoomComparison::CHAT_ROOM_COMPARISON_ERROR;
    }

    std::vector<std::string> post_chat_room_ids_inside_user_account_doc;
    for (const auto& c: chatRoomIds) {
        if (c.type() == bsoncxx::type::k_document) {

            bsoncxx::document::view chatRoomDoc = c.get_document().value;
            std::string chatRoomId;

            //extract chat room Id
            auto chatRoomIdElement = chatRoomDoc[user_account_keys::chat_rooms::CHAT_ROOM_ID];
            if (chatRoomIdElement &&
                chatRoomIdElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                chatRoomId = chatRoomIdElement.get_string().value.to_string();
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, chatRoomIdElement,
                                chatRoomDoc, bsoncxx::type::k_utf8, user_account_keys::chat_rooms::CHAT_ROOM_ID,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                continue;
            }

            post_chat_room_ids_inside_user_account_doc.emplace_back(std::move(chatRoomId));
        }
        else {
            std::string errorString =
                    "A value of '" + user_account_keys::CHAT_ROOMS + "' was not type document.\n";

            std::optional<std::string> exceptionString;
            storeMongoDBErrorAndException(__LINE__, __FILE__, exceptionString,
                                          errorString,
                                          "database", database_names::ACCOUNTS_DATABASE_NAME,
                                          "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                          "document_extracted_from", find_user_account_chat_rooms_view,
                                          "type", convertBsonTypeToString(c.type()));

            continue;
        }
    }

    //This means an account state was updated while this function was processing, retry the function.
    if(post_chat_room_ids_inside_user_account_doc.size() != pre_chat_room_ids_inside_user_account_doc.size()) {
        return UserAccountChatRoomComparison::CHAT_ROOM_COMPARISON_DO_NOT_MATCH;
    }

    std::sort(post_chat_room_ids_inside_user_account_doc.begin(), post_chat_room_ids_inside_user_account_doc.end());
    std::sort(pre_chat_room_ids_inside_user_account_doc.begin(), pre_chat_room_ids_inside_user_account_doc.end());

    //if any chat rooms have changed, retry the loop
    for(size_t i = 0; i < post_chat_room_ids_inside_user_account_doc.size(); ++i) {
        if(post_chat_room_ids_inside_user_account_doc[i] != pre_chat_room_ids_inside_user_account_doc[i]) {
            return UserAccountChatRoomComparison::CHAT_ROOM_COMPARISON_DO_NOT_MATCH;
        }
    }

    return UserAccountChatRoomComparison::CHAT_ROOM_COMPARISON_MATCH;
}

ThreadPoolSuspend extractChatRooms(
        const grpc_stream_chat::InitialLoginMessageRequest& request,
        StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE>& sendMessagesObject,
        std::set<std::string>& chat_room_ids_user_is_part_of,
        bool& successful,
        long calling_current_index_value,
        const std::string& userAccountOIDStr,
        const std::chrono::milliseconds& currentTimestamp
) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection userAccountCollection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const bsoncxx::oid user_account_oid{userAccountOIDStr};

    //There is a chance that a situation can occur where.
    // 1) User extracts their user account document.
    // 2) User is kicked from a chat room.
    // 3) User updates chat room and kicked message is sent back with do_not_update_user_state.
    // 4) The new kick message is sent back to the user and is ignored because it is a duplicate.
    //This could happen and the device would not 'know' that it was removed from the chat room. In order
    // to prevent this or a similar situation, chat room states are checked at the end of the loop and if
    // they match the chat room states at the start of the loop, it will continue. If not it will retry.
    bool account_chat_rooms_match = true;

    while(account_chat_rooms_match) {

        struct ChatRoomInfo {
            std::string chat_room_id;
            std::chrono::milliseconds chat_room_last_time_updated = std::chrono::milliseconds{-1L};
            std::chrono::milliseconds chat_room_last_time_observed = std::chrono::milliseconds{-1L};
            bsoncxx::builder::basic::array recent_message_uuids;

            bool last_time_observed_outdated_on_client = false;

            ChatRoomInfo(
                std::string _chat_room_id,
                const std::chrono::milliseconds& _chat_room_last_time_updated,
                const std::chrono::milliseconds& _chat_room_last_time_observed
            ) :
                chat_room_id(std::move(_chat_room_id)),
                chat_room_last_time_updated(_chat_room_last_time_updated),
                chat_room_last_time_observed(_chat_room_last_time_observed) {}

            bool operator<(const ChatRoomInfo& rhs) const {
                return chat_room_id < rhs.chat_room_id;
            }
        };

        std::vector<ChatRoomInfo> chat_rooms_from_grpc;
        {
            std::set<std::string> stored_chat_rooms;
            for (auto& c: request.chat_room_values()) {

                std::chrono::milliseconds timeLastViewed;
                std::chrono::milliseconds timeLastUpdated;

                if (isInvalidChatRoomId(c.chat_room_id())) {
                    //if chat room id is invalid, do not use it
                    continue;
                }

                if (c.chat_room_last_time_viewed() <=
                    currentTimestamp.count()) { //if the observed time is before the present time
                    timeLastViewed = std::chrono::milliseconds{c.chat_room_last_time_viewed()};
                } else { //if the observed time is not before the present time
                    timeLastViewed = currentTimestamp;
                }

                if (c.chat_room_last_time_updated() <=
                    currentTimestamp.count()) { //if the updated time is before the present time
                    timeLastUpdated = std::chrono::milliseconds{c.chat_room_last_time_updated()};
                } else { //if the updated time is not before the present time
                    timeLastUpdated = currentTimestamp;
                }

                //most_recent_message_uuid does not need to be valid. If it is invalid it will simply not
                // exist inside the database.

                auto result = stored_chat_rooms.insert(c.chat_room_id());
                //If a duplicate chat room is stored here, it can cause an exception below when
                // chat_rooms_from_grpc is accessed to build a document for updating the user account
                // document.
                if (result.second) {

                    chat_rooms_from_grpc.emplace_back(
                            c.chat_room_id(),
                            timeLastUpdated,
                            timeLastViewed
                    );

                    for (const auto& message_uuid: c.most_recent_message_uuids()) {
                        chat_rooms_from_grpc.back().recent_message_uuids.append(message_uuid);
                    }
                }
            }
        }

        bsoncxx::document::value findUserAccountsProjectionDoc = document{}
                << "_id" << 0
                << user_account_keys::CHAT_ROOMS << 1
                << user_account_keys::FIRST_NAME << 1
                << user_account_keys::PICTURES << 1
                << user_account_keys::AGE << 1
                << user_account_keys::GENDER << 1
                << user_account_keys::CITY << 1
                << user_account_keys::BIO << 1
                << user_account_keys::CATEGORIES << 1
                << finalize;

        //find user account document with only chat rooms projected
        bsoncxx::stdx::optional<bsoncxx::document::value> findUserAccountDocValue;
        try {

            if (chat_rooms_from_grpc.empty()) { //if there were no chat rooms passed from the client

                mongocxx::options::find findUserAccountDocOpts;

                findUserAccountDocOpts.projection(findUserAccountsProjectionDoc.view());

                findUserAccountDocValue = userAccountCollection.find_one(
                        document{}
                            << "_id" << user_account_oid
                        << finalize,
                        findUserAccountDocOpts
                );

            } else { //if chat room(s) passed from client

                bsoncxx::builder::stream::document updateDoc;
                bsoncxx::builder::basic::array arrayBuilder;
                std::string elementName = "a";

                mongocxx::options::find_one_and_update findOneAndUpdateUserAccountDocOpts;

                findOneAndUpdateUserAccountDocOpts.projection(findUserAccountsProjectionDoc.view());

                //update the document to use the latest observed times
                for (const auto& chatRoom: chat_rooms_from_grpc) {

                    std::string verifiedChatRoomIdKey = elementName;
                    verifiedChatRoomIdKey += ".";
                    verifiedChatRoomIdKey += user_account_keys::chat_rooms::CHAT_ROOM_ID;

                    arrayBuilder.append(document{}
                                                << verifiedChatRoomIdKey << chatRoom.chat_room_id
                                                << finalize);

                    const auto mongoDBDate = bsoncxx::types::b_date{
                            std::chrono::milliseconds(chatRoom.chat_room_last_time_observed)};

                    std::string tempElementUpdate = user_account_keys::CHAT_ROOMS;
                    tempElementUpdate += ".$[";
                    tempElementUpdate += elementName;
                    tempElementUpdate += "].";

                    const std::string ELEM_UPDATE = tempElementUpdate;

                    updateDoc
                            << ELEM_UPDATE + user_account_keys::chat_rooms::LAST_TIME_VIEWED << mongoDBDate;

                    generateNextElementName(elementName);
                }

                findOneAndUpdateUserAccountDocOpts.array_filters(arrayBuilder.view());

                //return timestamps afterwards
                findOneAndUpdateUserAccountDocOpts.return_document(mongocxx::options::return_document::k_after);

                //NOTE: this does not retrieve the documents after the update, this means that the newest times have not been saved
                findUserAccountDocValue = userAccountCollection.find_one_and_update(
                        document{}
                            << "_id" << user_account_oid
                        << finalize,
                        document{}
                            << "$max" << updateDoc.view()
                        << finalize,
                        findOneAndUpdateUserAccountDocOpts
                );
            }

        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
        }

        if (!findUserAccountDocValue) {
            successful = false;
            co_return;
        }

        const auto find_user_account_doc_view = findUserAccountDocValue->view();
        bsoncxx::array::view chatRoomIds;

        //2 types of messages
        //1) user-action messages (ex: left channel, kicked from channel, etc...)
        //2) chat messages (ex: chat message, location, etc...)
        //SERVER: return a list of client chat rooms that do not match (need a type for chat room removed and a type for chat room added)
        //IF NEW CHAT ROOM;
        //send back all users inside chat room
        //find last 20 chat messages and send them back to the client
        //IF PREVIOUSLY HELD CHAT ROOM;
        //find all user-action messages since last timestamp and send them back to the client (probably want them all, less processing power for server and )
        //find up to the last 20 chat messages and send them back to the client
        //get chat info for chat rooms this account belongs to
        auto chatRoomsElement = find_user_account_doc_view[user_account_keys::CHAT_ROOMS];
        if (chatRoomsElement &&
            chatRoomsElement.type() == bsoncxx::type::k_array) { //if element exists and is type array
            chatRoomIds = chatRoomsElement.get_array().value;
        } else { //if element does not exist or is not type array
            logElementError(__LINE__, __FILE__, chatRoomsElement,
                            find_user_account_doc_view, bsoncxx::type::k_array,
                            chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::USER_ACCOUNTS_COLLECTION_NAME);

            successful = false;
            co_return;
        }

        mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];
        std::vector<ChatRoomInfo> chat_rooms_inside_user_account_doc;
        std::vector<std::string> pre_chat_room_ids_inside_user_account_doc;
        for (const auto& c: chatRoomIds) {
            if (c.type() == bsoncxx::type::k_document) {

                bsoncxx::document::view chatRoomDoc = c.get_document().value;
                std::string chatRoomId;
                std::chrono::milliseconds lastTimeViewed;

                //extract chat room Id
                auto chatRoomIdElement = chatRoomDoc[user_account_keys::chat_rooms::CHAT_ROOM_ID];
                if (chatRoomIdElement &&
                    chatRoomIdElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    chatRoomId = chatRoomIdElement.get_string().value.to_string();
                } else { //if element does not exist or is not type utf8
                    logElementError(__LINE__, __FILE__, chatRoomIdElement,
                                    chatRoomDoc, bsoncxx::type::k_utf8, user_account_keys::chat_rooms::CHAT_ROOM_ID,
                                    database_names::ACCOUNTS_DATABASE_NAME,
                                    collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                    continue;
                }

                //extract chat room last time viewed
                auto chatRoomLastTimeViewedElement = chatRoomDoc[user_account_keys::chat_rooms::LAST_TIME_VIEWED];
                if (chatRoomLastTimeViewedElement && chatRoomLastTimeViewedElement.type() ==
                                                     bsoncxx::type::k_date) { //if element exists and is type date
                    lastTimeViewed = chatRoomLastTimeViewedElement.get_date().value;
                } else { //if element does not exist or is not type date
                    logElementError(__LINE__, __FILE__,
                                    chatRoomLastTimeViewedElement,
                                    chatRoomDoc, bsoncxx::type::k_date,
                                    user_account_keys::chat_rooms::LAST_TIME_VIEWED,
                                    database_names::ACCOUNTS_DATABASE_NAME,
                                    collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                    continue;
                }

                chat_room_ids_user_is_part_of.insert(chatRoomId);

                pre_chat_room_ids_inside_user_account_doc.emplace_back(chatRoomId);
                chat_rooms_inside_user_account_doc.emplace_back(
                        std::move(chatRoomId),
                        std::chrono::milliseconds{-1},
                        lastTimeViewed
                );

            } else {
                std::string errorString =
                        "A value of '" + user_account_keys::CHAT_ROOMS + "' was not type document.\n";

                std::optional<std::string> exceptionString;
                storeMongoDBErrorAndException(__LINE__, __FILE__, exceptionString,
                                              errorString,
                                              "database", database_names::ACCOUNTS_DATABASE_NAME,
                                              "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                              "document_extracted_from", find_user_account_doc_view,
                                              "type", convertBsonTypeToString(c.type()));

                continue;
            }
        }

        //setup map for chat room users
        //NOTE: This is part of concurrency, it is important that it comes.
        // After the user account document has been extracted so that it has all chat rooms from the user account.
        // Before the messages have been extracted, otherwise it may miss messages because it is not a part of the map_of_chat_rooms_to_users yet.
        for (const auto& chat_room_id: chat_room_ids_user_is_part_of) {
            RUN_COROUTINE(
                    insertUserOIDToChatRoomId_coroutine,
                    chat_room_id,
                    userAccountOIDStr,
                    calling_current_index_value
            );
        }

        std::sort(chat_rooms_inside_user_account_doc.begin(), chat_rooms_inside_user_account_doc.end());
        std::sort(chat_rooms_from_grpc.begin(), chat_rooms_from_grpc.end());

        //each chat room should be stored in one (and only one) of these vectors
        std::vector<ChatRoomInfo> chatRoomsToBeAddedToClient;
        std::vector<std::string> chatRoomsToBeRemovedFromClient;
        std::vector<ChatRoomInfo> chatRoomsToBeUpdatedOnClient;

        size_t user_account_index = 0, grpc_index = 0;
        while (user_account_index < chat_rooms_inside_user_account_doc.size() &&
               grpc_index < chat_rooms_from_grpc.size()) {

            if (chat_rooms_inside_user_account_doc[user_account_index].chat_room_id ==
                chat_rooms_from_grpc[grpc_index].chat_room_id) { //if chat room ids are the same

                if (chat_rooms_from_grpc[grpc_index].chat_room_last_time_observed <
                    chat_rooms_inside_user_account_doc[user_account_index].chat_room_last_time_observed) { //if the stored time from database is greater, use that as the observed time

                    chat_rooms_inside_user_account_doc[user_account_index].last_time_observed_outdated_on_client = true;
                    //chat_room_last_time_updated is a value unique to each device so the server does not store this value
                    chat_rooms_inside_user_account_doc[user_account_index].chat_room_last_time_updated = chat_rooms_from_grpc[grpc_index].chat_room_last_time_updated;
                    chat_rooms_inside_user_account_doc[user_account_index].recent_message_uuids = std::move(
                            chat_rooms_from_grpc[grpc_index].recent_message_uuids);
                    chatRoomsToBeUpdatedOnClient.emplace_back(
                            std::move(chat_rooms_inside_user_account_doc[user_account_index])
                    );
                } else { //if the times are equal then the time is not outdated

                    //NOTE: the case where client observed time > server observed time is handled above
                    chatRoomsToBeUpdatedOnClient.emplace_back(
                            std::move(chat_rooms_from_grpc[grpc_index])
                    );
                }

                user_account_index++;
                grpc_index++;
            } else if (chat_rooms_inside_user_account_doc[user_account_index].chat_room_id <
                       chat_rooms_from_grpc[grpc_index].chat_room_id) { //if user account chat room id is less than passed chat room id
                chatRoomsToBeAddedToClient.emplace_back(
                        std::move(chat_rooms_inside_user_account_doc[user_account_index])
                );
                user_account_index++;
            } else { //if passed chat room id is less than verified chat room id
                chatRoomsToBeRemovedFromClient.emplace_back(chat_rooms_from_grpc[grpc_index].chat_room_id);
                grpc_index++;
            }
        }

        for (size_t i = user_account_index; i < chat_rooms_inside_user_account_doc.size(); i++) {
            chatRoomsToBeAddedToClient.emplace_back(
                    std::move(chat_rooms_inside_user_account_doc[i])
            );
        }

        for (size_t i = grpc_index; i < chat_rooms_from_grpc.size(); i++) {
            chatRoomsToBeRemovedFromClient.emplace_back(chat_rooms_from_grpc[i].chat_room_id);
        }

        //send back added chat room info to client
        for (const auto& chatRoom: chatRoomsToBeAddedToClient) {
            mongocxx::collection chatRoomCollection = chatRoomDB[collection_names::CHAT_ROOM_ID_ +
                                                                 chatRoom.chat_room_id];
#ifdef _DEBUG
            std::cout << "Sending back added chat room: " << chatRoom.chat_room_id << '\n';
#endif

            if (!sendNewChatRoomAndMessages(
                    mongoCppClient,
                    accountsDB,
                    userAccountCollection,
                    chatRoomCollection,
                    find_user_account_doc_view,
                    chatRoom.chat_room_id,
                    chatRoom.chat_room_last_time_observed,
                    currentTimestamp,
                    userAccountOIDStr,
                    &sendMessagesObject,
                    HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
                    AmountOfMessage::ONLY_SKELETON,
                    false,
                    true
            )
                    ) {
                //NOTE: error was handled inside function
                successful = false;
                co_return;
            }
        }

        //send back removed chat room info to client
        for (const auto& chatRoomID: chatRoomsToBeRemovedFromClient) {

#if  defined(_DEBUG) && !defined(LG_TESTING)
            //This will spam the stress tests if it is on for LG_TESTING.
            std::cout << "Sending back removed chat room: " << chatRoomID << '\n';
#endif

            ChatMessageToClient responseMsg;
            responseMsg.set_sent_by_account_id(userAccountOIDStr);

            auto typeOfChatMessage = responseMsg.mutable_message();
            typeOfChatMessage->mutable_message_specifics()->mutable_this_user_left_chat_room_message();
            typeOfChatMessage->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chatRoomID);

            sendMessagesObject.sendMessage(std::move(responseMsg));
        }

        //send back updated chat room info to client
        for (auto& chatRoomInfo: chatRoomsToBeUpdatedOnClient) {

#if defined(_DEBUG) && !defined(LG_TESTING)
            std::cout << "Sending back update for chat room: " << chatRoomInfo.chat_room_id << '\n';
#endif

            mongocxx::collection chatRoomCollection = chatRoomDB[collection_names::CHAT_ROOM_ID_ +
                                                                 chatRoomInfo.chat_room_id];

            if (chatRoomInfo.last_time_observed_outdated_on_client) { //if last time observed requires updating

                ChatMessageToClient responseMsg;
                responseMsg.set_sent_by_account_id(userAccountOIDStr);

                auto typeOfChatMessage = responseMsg.mutable_message();
                typeOfChatMessage->mutable_message_specifics()->mutable_update_observed_time_message()->set_chat_room_last_observed_time(
                        chatRoomInfo.chat_room_last_time_observed.count());
                typeOfChatMessage->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
                        chatRoomInfo.chat_room_id);

                sendMessagesObject.sendMessage(std::move(responseMsg));
            }

            const std::chrono::milliseconds updated_time = chatRoomInfo.chat_room_last_time_updated -
                                                           chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES;

            //stream response to client sending message from chat room
            //NOTE: there are 2 ways this could be updated
            // 1) the current way, where it sends back all messages the client is missing and the client responds to each message actively
            // as if it is receiving the message
            // 2) something similar to updateSingleChatRoomMember() function inside chat_room_commands this will require the client to send a list of members
            // to the chat room and either all the chat room info or will need to send back all the updated chat room info
            //THOUGHTS
            // 1) is easier to implement on the server certainly, also it does not require dealing with iterating through the chat room header
            // also most of the processing will be done on the client side for this, however it could have problems, if a single function that receives a message
            // on the client has a bug then the chat rooms could not update properly, this will have to be handled on the client side with error handling
            // 2) this one will take considerably more to implement on the server side, most likely will need 3 new message types, one for updating the chat room, one for adding
            // a member (might be able to use THIS_USER_JOINED_CHAT_ROOM_MEMBER) and one for updating a user account state inside the chat room, also all messages will still
            // have to be sent back.
            //DECISION
            // I think I will stick with way 1) right now, worst case the error handling on the client can remove and add the chat room, or request a user that was
            // not properly sent back, and honestly its good practice to implement these on the client regardless of which method I use. NOTE FROM LATER-The update chat room function
            // is required to do considerably more than this implementation is. It must check each user's info (which this does not) and it would need a condition to NOT download complete user info
            // for anyone, it will also need to check active times, chat room info, etc... Most of updateChatRoom() will end up inside a boolean value of if(!from_server_initialization) and the worst
            // case here is that the chat room info is out of date until the user clicks it at which point updateChatRoom() will run and handle it. This method of implementing it is actually
            // a nice lightweight method perfect for stream initialization.
            streamInitializationMessagesToClient(
                    mongoCppClient,
                    accountsDB,
                    chatRoomCollection,
                    userAccountCollection,
                    find_user_account_doc_view,
                    chatRoomInfo.chat_room_id,
                    userAccountOIDStr,
                    &sendMessagesObject,
                    //When the chat stream initially connects, it must have enough info returned to show as a notification.
                    // The notification char number seems to be limited on Android meaning that this amount should be
                    // enough to show a text message (the other types simply show a small line of text anyway e.g. 'Picture
                    // Message').
                    AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
                    DifferentUserJoinedChatRoomAmount::INFO_WITH_THUMBNAIL_NO_PICTURES,
                    true,
                    false,
                    false,
                    updated_time,
                    chatRoomInfo.recent_message_uuids
            );
        }

        UserAccountChatRoomComparison chat_room_comparison = compareUserAccountChatRoomsStates(
                pre_chat_room_ids_inside_user_account_doc,
                userAccountCollection,
                user_account_oid
        );

        switch (chat_room_comparison) {
            case CHAT_ROOM_COMPARISON_ERROR:
                successful = false;
                co_return;
            case CHAT_ROOM_COMPARISON_DO_NOT_MATCH:
                sendMessagesObject.clearAllMessages(); //on continue
                continue;
            case CHAT_ROOM_COMPARISON_MATCH:
                account_chat_rooms_match = false;
                break;
        }

    }

    successful = true;
    co_return;
}