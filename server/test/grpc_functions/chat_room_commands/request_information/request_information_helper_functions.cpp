//
// Created by jeremiah on 8/27/22.
//

#include <fstream>
#include "collection_objects/accounts_objects/account_objects.h"
#include "collection_objects/reports_objects/reports_objects.h"
#include "ChatRoomCommands.pb.h"
#include "chat_room_commands.h"
#include "gtest/gtest.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "connection_pool_global_variable.h"
#include "chat_room_message_keys.h"
#include "chat_room_commands_helper_functions.h"
#include "update_single_other_user.h"
#include "request_information_helper_functions.h"
#include "compare_equivalent_messages.h"

void build_updateSingleChatRoomMemberNotInChatRoom_comparison(
        long current_timestamp_used,
        const std::string& user_account_oid,
        const std::string& chat_room_id,
        const ChatRoomHeaderDoc& header_doc,
        const UpdateOtherUserResponse& update_response_msg,
        const OtherUserInfoForUpdates& client_info
) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_collection = accounts_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    UpdateOtherUserResponse generated_response;
    bool successfully_found_user = false;

    for(const auto& account : header_doc.accounts_in_chat_room) {
        if(account.account_oid.to_string() == user_account_oid) {
            bsoncxx::builder::stream::document header_account_builder;
            account.convertToDocument(header_account_builder);

            updateSingleChatRoomMemberNotInChatRoom(
                    mongo_cpp_client,
                    accounts_db,
                    chat_room_collection,
                    user_account_oid,
                    header_account_builder.view(),
                    client_info,
                    std::chrono::milliseconds {current_timestamp_used},
                    false,
                    account.state_in_chat_room,
                    std::chrono::milliseconds {account.last_activity_time},
                    &generated_response,
                    [&]() {
                        successfully_found_user = true;
                    }
            );
            break;
        }
    }

    if(!successfully_found_user) {
        generated_response.Clear();
    }

    compareEquivalentMessages(
            update_response_msg,
            generated_response
    );
}

void build_updateSingleOtherUserOrEvent_comparison(
        long current_timestamp_used,
        const std::string& user_account_oid,
        const std::string& chat_room_id,
        const ChatRoomHeaderDoc& header_doc,
        const UpdateOtherUserResponse& update_response_msg,
        const OtherUserInfoForUpdates& other_user,
        HowToHandleMemberPictures handle_member_pictures
) {
    UpdateOtherUserResponse generated_response;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    bool successfully_wrote = false;

    for(const auto& account : header_doc.accounts_in_chat_room) {
        if(account.account_oid.to_string() == user_account_oid) {

            UserAccountDoc new_chat_room_user_oid_doc(account.account_oid);

            bsoncxx::builder::stream::document user_document_builder;
            new_chat_room_user_oid_doc.convertToDocument(user_document_builder);

            updateSingleOtherUser(
                    mongo_cpp_client,
                    accounts_db,
                    user_document_builder.view(),
                    user_accounts_collection,
                    collection_names::CHAT_ROOM_ID_ + chat_room_id,
                    user_account_oid,
                    other_user,
                    std::chrono::milliseconds{current_timestamp_used},
                    false,
                    account.state_in_chat_room,
                    std::chrono::milliseconds{account.last_activity_time},
                    handle_member_pictures,
                    &generated_response,
                    [&]() {
                        successfully_wrote = true;
                    }
            );
            break;
        }
    }

    if(!successfully_wrote && header_doc.event_id && header_doc.event_id->to_string() == user_account_oid) {
        UserAccountDoc new_chat_room_user_oid_doc(bsoncxx::oid{user_account_oid});

        bsoncxx::builder::stream::document user_document_builder;
        new_chat_room_user_oid_doc.convertToDocument(user_document_builder);

        updateSingleOtherUser(
                mongo_cpp_client,
                accounts_db,
                user_document_builder.view(),
                user_accounts_collection,
                collection_names::CHAT_ROOM_ID_ + chat_room_id,
                user_account_oid,
                other_user,
                std::chrono::milliseconds{current_timestamp_used},
                false,
                AccountStateInChatRoom::ACCOUNT_STATE_EVENT,
                chat_room_values::EVENT_USER_LAST_ACTIVITY_TIME_DEFAULT,
                handle_member_pictures,
                &generated_response,
                [&]() {
                    successfully_wrote = true;
                }
        );
    }

    EXPECT_TRUE(successfully_wrote);

    if(!successfully_wrote) {
        generated_response.Clear();
    }

    compareEquivalentMessages(
            update_response_msg,
            generated_response
    );
}

void build_buildBasicUpdateOtherUserResponse_comparison(
        long current_timestamp_used,
        AccountStateInChatRoom user_account_state,
        const std::string& user_account_oid,
        long user_last_activity_time,
        const UpdateOtherUserResponse& update_response_msg
) {
    UpdateOtherUserResponse generated_response;

    buildBasicUpdateOtherUserResponse(
            &generated_response,
            user_account_state,
            user_account_oid,
            std::chrono::milliseconds{current_timestamp_used},
            user_last_activity_time
    );

    compareEquivalentMessages(
            update_response_msg,
            generated_response
    );
}