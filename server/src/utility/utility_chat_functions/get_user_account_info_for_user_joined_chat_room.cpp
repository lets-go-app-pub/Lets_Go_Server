//
// Created by jeremiah on 4/7/21.
//

#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include "utility_chat_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "utility_testing_functions.h"
#include "chat_room_commands_helper_functions.h"
#include "update_single_other_user.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool getUserAccountInfoForUserJoinedChatRoom(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& userAccountsCollection,
        const std::string& chatRoomID,
        const std::string& messageSentByOID,
        HowToHandleMemberPictures howToHandleMemberPictures,
        MemberSharedInfoMessage* userMemberInfo
) {
    const bsoncxx::oid memberOID = bsoncxx::oid{messageSentByOID};

    mongocxx::stdx::optional<bsoncxx::document::value> findMemberUserAccount;
    try {

        mongocxx::options::find opts;

        opts.projection(buildUpdateSingleOtherUserProjectionDoc());

        //find user account document
        findMemberUserAccount = userAccountsCollection.find_one(
                document{}
                    << "_id" << memberOID
                << finalize,
                opts);
    }
    catch (const mongocxx::logic_error& e) {
        std::optional<std::string> dummy_exception_string;
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, dummy_exception_string, std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "ObjectID_used", memberOID,
                "chat_room_name", collection_names::CHAT_ROOM_ID_ + chatRoomID
        );
        return false;
    }

    if (findMemberUserAccount) { //find succeeded

        const bsoncxx::document::view memberAccountDocView = findMemberUserAccount->view();
        userMemberInfo->set_account_oid(messageSentByOID);

        if (!saveUserInfoToMemberSharedInfoMessage(mongoCppClient,
                                                   accountsDB,
                                                   userAccountsCollection, memberAccountDocView, memberOID,
                                                   userMemberInfo,
                                                   howToHandleMemberPictures,
                                                   getCurrentTimestamp())
                ) {
            return false;
        }

    }
    else { //find user account doc failed

        //NOTE: This situation is possible if the user was deleted before a message was received. However, if
        // it does happen it should be very rare. Still storing an error for it, because realistically it should
        // be 'difficult' to happen.
        {
            std::string errorString = "Failed to find user account document document for kDifferentUserJoinedMessage.";

            std::optional<std::string> dummy_exception_string;
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, dummy_exception_string, errorString,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", memberOID,
                    "chat_room_name", collection_names::CHAT_ROOM_ID_ + chatRoomID
            );

            //NOTE: OK to continue here.
        }

        mongocxx::database chat_rooms_db = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];
        mongocxx::collection chat_room_collection = chat_rooms_db[collection_names::CHAT_ROOM_ID_ + chatRoomID];

        std::optional<std::string> find_user_account_header_exception_string;
        mongocxx::stdx::optional<bsoncxx::document::value> find_user_account_header;
        try {

            mongocxx::options::find opts;

            //this will project only the specific element representing the requested user from the header
            opts.projection(
                document{}
                    << "_id" << 0
                    << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                        << "$elemMatch" << open_document
                            << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << memberOID
                        << close_document
                    << close_document
                << finalize
            );

            find_user_account_header = chat_room_collection.find_one(
                document{}
                    << "_id" << chat_room_header_keys::ID
                << finalize,
                opts
            );
        }
        catch (const mongocxx::logic_error& e) {
            find_user_account_header_exception_string = e.what();
        }

        if(!find_user_account_header) {
            const std::string error_string = "An error occurred when attempting to extract user from chat room collection.";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, find_user_account_header_exception_string, error_string,
                    "ObjectID_used", memberOID,
                    "chat_room_name", collection_names::CHAT_ROOM_ID_ + chatRoomID
            );
            return false;
        }

        bsoncxx::document::view chatRoomAccountView = find_user_account_header->view();

        bsoncxx::document::view account_from_header;
        bool accountFound = false;
        bsoncxx::array::view accountsInChatRoom;

        auto accountsInChatRoomElement = chatRoomAccountView[chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM];
        if (accountsInChatRoomElement &&
            accountsInChatRoomElement.type() == bsoncxx::type::k_array) { //if element exists and is type array
            accountsInChatRoom = accountsInChatRoomElement.get_array().value;
        } else { //if element does not exist or is not type array
            logElementError(__LINE__, __FILE__,
                            accountsInChatRoomElement,
                            chatRoomAccountView, bsoncxx::type::k_array, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

            return false;
        }

        //there should only be 1 element in this because of the projection
        for (const auto& account : accountsInChatRoom) {
            if (account.type() == bsoncxx::type::k_document) {

                std::string accountOID;

                auto accountOidElement = account.get_document().view()[chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID];
                if (accountOidElement &&
                    accountOidElement.type() == bsoncxx::type::k_oid) { //if element exists and is type array
                    accountOID = accountOidElement.get_oid().value.to_string();
                } else { //if element does not exist or is not type array
                    logElementError(__LINE__, __FILE__,
                                    accountOidElement,
                                    chatRoomAccountView, bsoncxx::type::k_oid, chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID,
                                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                    continue;
                }

                if (messageSentByOID == accountOID) {
                    accountFound = true;
                    account_from_header = account.get_document().value;
                    break;
                }
            } else {

                std::string errorString = "An element of ACCOUNTS_IN_CHAT_ROOM was not type document.\n";

                std::optional<std::string> exceptionString;
                storeMongoDBErrorAndException(__LINE__,
                                              __FILE__, exceptionString, errorString,
                                              "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                              "collection", collection_names::CHAT_ROOM_ID_ + chatRoomID,
                                              "ObjectID_used", messageSentByOID,
                                              "type", convertBsonTypeToString(account.type()));

                //NOTE: still continuable here
            }
        }

        if(!accountFound) {
            std::string errorString = "User document not found inside chat room header when kDifferentUserJoined was sent to chat room.\n";

            std::optional<std::string> dummy_exception_string;
            storeMongoDBErrorAndException(__LINE__,
                                          __FILE__, dummy_exception_string, errorString,
                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                          "collection", collection_names::CHAT_ROOM_ID_ + chatRoomID,
                                          "ObjectID_used", messageSentByOID);

            return false;
        }

        std::chrono::milliseconds user_last_activity_time;

        auto userLastActivityTimeElement = account_from_header[chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME];
        if (userLastActivityTimeElement &&
            userLastActivityTimeElement.type() == bsoncxx::type::k_date) { //if element exists and is type date
            user_last_activity_time = userLastActivityTimeElement.get_date().value;
        } else { //if element does not exist or is not type date
            logElementError(__LINE__, __FILE__, userLastActivityTimeElement,
                            account_from_header, bsoncxx::type::k_date,
                            chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

            return false;
        }

        //allow this to be mostly default values
        OtherUserInfoForUpdates member;
        member.set_thumbnail_timestamp(-1);

        UpdateOtherUserResponse response;

        updateSingleChatRoomMemberNotInChatRoom(
                mongoCppClient,
                accountsDB,
                chat_room_collection,
                memberOID.to_string(),
                account_from_header,
                member,
                getCurrentTimestamp(),
                true,
                AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM, //Irrelevant
                user_last_activity_time,
                &response,
                [](){}
        );

        userMemberInfo->Swap(response.mutable_user_info());
    }

    return true;
}