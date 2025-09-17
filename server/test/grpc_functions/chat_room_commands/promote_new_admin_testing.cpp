//
// Created by jeremiah on 8/23/22.
//

#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/collection.hpp>
#include <account_objects.h>
#include <reports_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "chat_room_commands_helper_functions.h"
#include "grpc_mock_stream/mock_stream.h"
#include "generate_randoms.h"
#include "event_admin_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class PromoteNewAdminTesting : public ::testing::Test {
protected:

    bsoncxx::oid admin_account_oid;
    bsoncxx::oid target_account_oid;

    UserAccountDoc admin_account_doc;
    UserAccountDoc target_account_doc;

    std::string chat_room_id;
    std::string chat_room_password;
    const std::string message_uuid = generateUUID();

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        admin_account_oid = insertRandomAccounts(1, 0);
        target_account_oid = insertRandomAccounts(1, 0);

        admin_account_doc.getFromCollection(admin_account_oid);
        target_account_doc.getFromCollection(target_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                admin_account_oid,
                admin_account_doc.logged_in_token,
                admin_account_doc.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();
        chat_room_password = create_chat_room_response.chat_room_password();

        grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

        setupUserLoginInfo(
                join_chat_room_request.mutable_login_info(),
                target_account_oid,
                target_account_doc.logged_in_token,
                target_account_doc.installation_ids.front()
        );

        join_chat_room_request.set_chat_room_id(chat_room_id);
        join_chat_room_request.set_chat_room_password(chat_room_password);

        grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

        joinChatRoom(&join_chat_room_request, &mock_server_writer);

        EXPECT_EQ(mock_server_writer.write_params.size(), 2);
        if (mock_server_writer.write_params.size() >= 2) {
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
            if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
                EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
                EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
            }
        }

        admin_account_doc.getFromCollection(admin_account_oid);
        target_account_doc.getFromCollection(target_account_oid);

        //Sleep to guarantee last active times will be different when running promoteNewAdmin().
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(PromoteNewAdminTesting, invalidLoginInfo) {

    checkLoginInfoClientOnly(
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {
                grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
                grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

                promote_new_admin_request.mutable_login_info()->CopyFrom(login_info);
                promote_new_admin_request.set_chat_room_id(chat_room_id);
                promote_new_admin_request.set_new_admin_account_oid(target_account_oid.to_string());
                promote_new_admin_request.set_message_uuid(message_uuid);

                promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

                return promote_new_admin_response.return_status();
            }
    );

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
}

TEST_F(PromoteNewAdminTesting, invalidChatRoomId) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    for(auto& account : generated_header_doc.accounts_in_chat_room) {
        if(account.account_oid == target_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id("123");
    promote_new_admin_request.set_new_admin_account_oid(target_account_oid.to_string());
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), false);
    EXPECT_EQ(promote_new_admin_response.timestamp_message_stored(), -2);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);
    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
    EXPECT_EQ(extracted_header_doc, generated_header_doc);
}

TEST_F(PromoteNewAdminTesting, invalidTargetOid) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    for(auto& account : generated_header_doc.accounts_in_chat_room) {
        if(account.account_oid == target_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(gen_random_alpha_numeric_string(rand() % 10));
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), false);
    EXPECT_EQ(promote_new_admin_response.timestamp_message_stored(), -2);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);
    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
    EXPECT_EQ(extracted_header_doc, generated_header_doc);
}

TEST_F(PromoteNewAdminTesting, invalidMessageUuid) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    for(auto& account : generated_header_doc.accounts_in_chat_room) {
        if(account.account_oid == target_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(target_account_oid.to_string());
    promote_new_admin_request.set_message_uuid(gen_random_alpha_numeric_string(rand() % 35));

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), false);
    EXPECT_EQ(promote_new_admin_response.timestamp_message_stored(), -2);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);
    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
    EXPECT_EQ(extracted_header_doc, generated_header_doc);
}

TEST_F(PromoteNewAdminTesting, sendingOidIsTargetOid) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    for(auto& account : generated_header_doc.accounts_in_chat_room) {
        if(account.account_oid == target_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(admin_account_oid.to_string());
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), true);
    EXPECT_EQ(promote_new_admin_response.timestamp_message_stored(), -1);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);
    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
    EXPECT_EQ(extracted_header_doc, generated_header_doc);
}

TEST_F(PromoteNewAdminTesting, targetOIDIsEventAdminOID) {

    createAndStoreEventAdminAccount();

    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    UserAccountDoc event_admin_doc(event_admin_values::OID);
    bsoncxx::oid picture_reference;
    bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
    for (const auto& pic: event_admin_doc.pictures) {
        if (pic.pictureStored()) {
            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );
            break;
        }
    }

    ASSERT_NE(timestamp_stored.value.count(), -1L);

    const::std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    //This situation can never happen, that is because the event admin is always admin. However, being admin also
    // means that they can not be promoted to admin to begin with. So this will properly test the functions restriction.
    UserPictureDoc event_admin_picture(picture_reference);
    generated_header_doc.accounts_in_chat_room.emplace_back(
        event_admin_values::OID,
        AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
        event_admin_values::FIRST_NAME,
        picture_reference.to_string(),
        timestamp_stored,
        event_admin_picture.thumbnail_size_in_bytes,
        bsoncxx::types::b_date{std::chrono::milliseconds{current_timestamp.count()}},
        std::vector<bsoncxx::types::b_date> {bsoncxx::types::b_date{std::chrono::milliseconds{current_timestamp.count() - 2}}}
    );

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(event_admin_values::OID.to_string());
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), true);
    EXPECT_EQ(promote_new_admin_response.timestamp_message_stored(), -1);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);
    UserAccountDoc event_admin_account_doc_after(event_admin_values::OID);
    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
    EXPECT_EQ(event_admin_doc, event_admin_account_doc_after);
    EXPECT_EQ(extracted_header_doc, generated_header_doc);
}

TEST_F(PromoteNewAdminTesting, successfullyRan) {

    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
        promote_new_admin_request.mutable_login_info(),
        admin_account_oid,
        admin_account_doc.logged_in_token,
        admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(target_account_oid.to_string());
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), true);
    EXPECT_GT(promote_new_admin_response.timestamp_message_stored(), -1);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_GT(extracted_header_doc.chat_room_last_active_time, generated_header_doc.chat_room_last_active_time);
    generated_header_doc.chat_room_last_active_time = extracted_header_doc.chat_room_last_active_time;

    EXPECT_EQ(generated_header_doc.accounts_in_chat_room.size(), 2);
    EXPECT_EQ(extracted_header_doc.accounts_in_chat_room.size(), 2);

    bsoncxx::types::b_date extract_admin_last_active_time{std::chrono::milliseconds{-1}};
    for(const auto& x : extracted_header_doc.accounts_in_chat_room) {
        if(x.account_oid == admin_account_oid) {
            extract_admin_last_active_time = x.last_activity_time;
            break;
        }
    }

    for(auto& x : generated_header_doc.accounts_in_chat_room) {
        if(x.account_oid == admin_account_oid) {
            EXPECT_GT(extract_admin_last_active_time, x.last_activity_time);
            x.last_activity_time = extract_admin_last_active_time;
            x.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;
        } else if(x.account_oid == target_account_oid) {
            x.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN;
        }
    }

    EXPECT_EQ(generated_header_doc, extracted_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);

    EXPECT_EQ(admin_account_doc.chat_rooms.size(), 1);
    EXPECT_EQ(extracted_admin_account_doc.chat_rooms.size(), 1);
    if(!admin_account_doc.chat_rooms.empty()
        && !extracted_admin_account_doc.chat_rooms.empty()
    ) {
        EXPECT_GT(
            extracted_admin_account_doc.chat_rooms[0].last_time_viewed,
            admin_account_doc.chat_rooms[0].last_time_viewed
        );
        admin_account_doc.chat_rooms[0].last_time_viewed = extracted_admin_account_doc.chat_rooms[0].last_time_viewed;
    }

    EXPECT_EQ(admin_account_doc, extracted_admin_account_doc);

    UserAccountDoc extracted_target_account_doc(target_account_oid);

    EXPECT_EQ(target_account_doc, extracted_target_account_doc);

    ChatRoomMessageDoc chat_room_message(message_uuid, chat_room_id);

    EXPECT_EQ(chat_room_message.id, message_uuid);
    if(!chat_room_message.id.empty()) {
        EXPECT_EQ(chat_room_message.message_type, MessageSpecifics::MessageBodyCase::kNewAdminPromotedMessage);
    }
}

TEST_F(PromoteNewAdminTesting, targetUserNotInChatRoom) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    for(auto& account : generated_header_doc.accounts_in_chat_room) {
        if(account.account_oid == target_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(target_account_oid.to_string());
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), false);
    EXPECT_EQ(promote_new_admin_response.timestamp_message_stored(), -1);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);
    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
    EXPECT_EQ(extracted_header_doc, generated_header_doc);
}

TEST_F(PromoteNewAdminTesting, sendingUserNotAdminOfChatRoom) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    for(auto& account : generated_header_doc.accounts_in_chat_room) {
        if(account.account_oid == admin_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(target_account_oid.to_string());
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), false);
    EXPECT_EQ(promote_new_admin_response.timestamp_message_stored(), -1);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);
    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
    EXPECT_EQ(extracted_header_doc, generated_header_doc);
}

TEST_F(PromoteNewAdminTesting, chatRoomDoesNotExist) {
    const std::string dummy_chat_room_id = generateRandomChatRoomId();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(dummy_chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(target_account_oid.to_string());
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::LG_ERROR);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), false);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
}

TEST_F(PromoteNewAdminTesting, adminNotInChatRoomHeader) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    //remove admin account
    for(auto it = generated_header_doc.accounts_in_chat_room.begin(); it != generated_header_doc.accounts_in_chat_room.end();) {
        if(it->account_oid == admin_account_oid) {
            generated_header_doc.accounts_in_chat_room.erase(it);
            break;
        } else {
            ++it;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(target_account_oid.to_string());
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::LG_ERROR);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), false);
    EXPECT_EQ(promote_new_admin_response.timestamp_message_stored(), -1);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);
    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
    EXPECT_EQ(extracted_header_doc, generated_header_doc);
}

TEST_F(PromoteNewAdminTesting, targetNotInChatRoomHeader) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    //remove admin account
    for(auto it = generated_header_doc.accounts_in_chat_room.begin(); it != generated_header_doc.accounts_in_chat_room.end();) {
        if(it->account_oid == target_account_oid) {
            generated_header_doc.accounts_in_chat_room.erase(it);
            break;
        } else {
            ++it;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(target_account_oid.to_string());
    promote_new_admin_request.set_message_uuid(message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    EXPECT_EQ(promote_new_admin_response.return_status(), ReturnStatus::LG_ERROR);
    EXPECT_EQ(promote_new_admin_response.user_account_states_matched(), false);
    EXPECT_EQ(promote_new_admin_response.timestamp_message_stored(), -1);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);
    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
    EXPECT_EQ(extracted_header_doc, generated_header_doc);
}
