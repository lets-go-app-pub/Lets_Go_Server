//
// Created by jeremiah on 8/27/22.
//
#include <fstream>
#include "collection_objects/accounts_objects/account_objects.h"
#include "collection_objects/reports_objects/reports_objects.h"
#include "ChatRoomCommands.pb.h"
#include "chat_room_commands.h"
#include "setup_login_info/setup_login_info.h"
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "utility_general_functions.h"
#include "generate_multiple_random_accounts.h"
#include "grpc_mock_stream/mock_stream.h"
#include "connection_pool_global_variable.h"
#include "chat_room_message_keys.h"
#include "update_single_other_user.h"
#include "request_information_helper_functions.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "event_request_message_is_valid.h"
#include "generate_randoms.h"
#include "event_admin_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GrpcUpdateSingleChatRoomMemberTesting : public ::testing::Test {
protected:

    bsoncxx::oid admin_account_oid;
    bsoncxx::oid member_account_oid;

    UserAccountDoc admin_account_doc;
    UserAccountDoc member_account_doc;

    std::string chat_room_id;
    std::string chat_room_password;

    UserPictureDoc member_thumbnail_picture;

    ChatRoomHeaderDoc chat_room_header;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        admin_account_oid = insertRandomAccounts(1, 0);
        member_account_oid = insertRandomAccounts(1, 0);

        admin_account_doc.getFromCollection(admin_account_oid);
        member_account_doc.getFromCollection(member_account_oid);

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
                member_account_oid,
                member_account_doc.logged_in_token,
                member_account_doc.installation_ids.front()
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
        member_account_doc.getFromCollection(member_account_oid);

        chat_room_header.getFromCollection(chat_room_id);

        //Get UserPictureDoc object for the generated user's thumbnail.
        for (const auto& pic : member_account_doc.pictures) {
            if (pic.pictureStored()) {
                bsoncxx::oid picture_reference;
                bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

                pic.getPictureReference(
                        picture_reference,
                        timestamp_stored
                );

                member_thumbnail_picture.getFromCollection(picture_reference);

                break;
            }
        }

        //Sleep to guarantee last active times will be different when running promoteNewAdmin().
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    //Will store only the admin account. Will store all other information as most recent info.
    grpc_chat_commands::UpdateSingleChatRoomMemberRequest setupSimpleUpdateSingleChatRoomMemberRequest() {
        grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request;

        setupUserLoginInfo(
                update_single_chat_room_member_request.mutable_login_info(),
                admin_account_oid,
                admin_account_doc.logged_in_token,
                admin_account_doc.installation_ids.front()
        );

        update_single_chat_room_member_request.set_chat_room_id(chat_room_id);

        const long current_timestamp_value = getCurrentTimestamp().count();

        auto chat_room_member_info = update_single_chat_room_member_request.mutable_chat_room_member_info();
        chat_room_member_info->set_account_oid(member_account_doc.current_object_oid.to_string());
        chat_room_member_info->set_thumbnail_size_in_bytes(member_thumbnail_picture.thumbnail_size_in_bytes);
        chat_room_member_info->set_thumbnail_index_number(member_thumbnail_picture.picture_index);
        chat_room_member_info->set_thumbnail_timestamp(current_timestamp_value);

        chat_room_member_info->set_first_name(member_account_doc.first_name);
        chat_room_member_info->set_age(member_account_doc.age);
        chat_room_member_info->set_member_info_last_updated_timestamp(current_timestamp_value);

        int picture_index = 0;
        for(const auto& pic : member_account_doc.pictures) {
            if (pic.pictureStored()) {
                bsoncxx::oid picture_reference;
                bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

                pic.getPictureReference(
                        picture_reference,
                        timestamp_stored
                );

                auto picture_last_updated = chat_room_member_info->add_pictures_last_updated_timestamps();
                picture_last_updated->set_last_updated_timestamp(timestamp_stored.value.count());
                picture_last_updated->set_index_number(picture_index);
            }
            picture_index++;
        }

        return update_single_chat_room_member_request;
    }

    void checkSuccessfulCallUpdatedValues(
            const ChatRoomHeaderDoc& chat_room_header_after,
            long timestamp_message_created,
            bool skip_member_check = false
            ) {
        UserAccountDoc admin_account_doc_after(admin_account_oid);
        UserAccountDoc member_account_doc_after(member_account_oid);

        bool found_match = false;
        for(auto& x : chat_room_header.accounts_in_chat_room) {
            if(x.account_oid == admin_account_oid) {
                for(const auto& y : chat_room_header_after.accounts_in_chat_room) {
                    if(y.account_oid == admin_account_oid) {
                        EXPECT_LT(x.last_activity_time, y.last_activity_time);
                        x.last_activity_time = y.last_activity_time;
                        found_match = true;
                        break;
                    }
                }
                break;
            }
        }
        EXPECT_TRUE(found_match);

        EXPECT_EQ(admin_account_doc, admin_account_doc_after);
        if(!skip_member_check) { EXPECT_EQ(member_account_doc, member_account_doc_after); }
        EXPECT_EQ(chat_room_header, chat_room_header_after);

        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
        mongocxx::collection chat_room_collection = chat_rooms_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

        auto message_doc = chat_room_collection.find_one(
                document{}
                        << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{std::chrono::milliseconds{timestamp_message_created}}
                << finalize
        );

        ASSERT_TRUE(message_doc);
    }
};

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {
                grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request;
                UpdateOtherUserResponse update_single_chat_room_member_response;

                update_single_chat_room_member_request.mutable_login_info()->CopyFrom(login_info);
                update_single_chat_room_member_request.set_chat_room_id(chat_room_id);

                const long current_timestamp_value = getCurrentTimestamp().count();

                auto chat_room_member_info = update_single_chat_room_member_request.mutable_chat_room_member_info();
                chat_room_member_info->set_account_oid(member_account_doc.current_object_oid.to_string());
                chat_room_member_info->set_thumbnail_size_in_bytes(member_thumbnail_picture.thumbnail_size_in_bytes);
                chat_room_member_info->set_thumbnail_index_number(member_thumbnail_picture.picture_index);
                chat_room_member_info->set_thumbnail_timestamp(current_timestamp_value);

                chat_room_member_info->set_first_name(member_account_doc.first_name);
                chat_room_member_info->set_age(member_account_doc.age);
                chat_room_member_info->set_member_info_last_updated_timestamp(current_timestamp_value);

                int picture_index = 0;
                for(const auto& pic : member_account_doc.pictures) {
                    if (pic.pictureStored()) {
                        bsoncxx::oid picture_reference;
                        bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

                        pic.getPictureReference(
                                picture_reference,
                                timestamp_stored
                        );

                        auto picture_last_updated = chat_room_member_info->add_pictures_last_updated_timestamps();
                        picture_last_updated->set_last_updated_timestamp(timestamp_stored.value.count());
                        picture_last_updated->set_index_number(picture_index);
                    }
                    picture_index++;
                }

                updateSingleChatRoomMember(
                        &update_single_chat_room_member_request,
                        &update_single_chat_room_member_response
                );

                return update_single_chat_room_member_response.return_status();
            }
    );

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc member_account_doc_after(member_account_oid);
    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(member_account_doc, member_account_doc_after);
    EXPECT_EQ(chat_room_header, chat_room_header_after);
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, invalidChatRoomId) {
    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    update_single_chat_room_member_request.set_chat_room_id("invalid_chat_room_id");

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    EXPECT_EQ(update_single_chat_room_member_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc member_account_doc_after(member_account_oid);
    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(member_account_doc, member_account_doc_after);
    EXPECT_EQ(chat_room_header, chat_room_header_after);
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, invalidChatRoomMemberInfo) {
    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    update_single_chat_room_member_request.mutable_chat_room_member_info()->set_account_oid("invalid_oid");

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    EXPECT_EQ(update_single_chat_room_member_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc member_account_doc_after(member_account_oid);
    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(member_account_doc, member_account_doc_after);
    EXPECT_EQ(chat_room_header, chat_room_header_after);
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, sendingUserNotInsideChatRoom) {
    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    for(auto& account : chat_room_header.accounts_in_chat_room) {
        if(account.account_oid == admin_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
            break;
        }
    }

    chat_room_header.setIntoCollection();

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    build_buildBasicUpdateOtherUserResponse_comparison(
            update_single_chat_room_member_response.timestamp_returned(),
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            admin_account_oid.to_string(),
            -1L,
            update_single_chat_room_member_response
    );

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc member_account_doc_after(member_account_oid);
    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(member_account_doc, member_account_doc_after);
    EXPECT_EQ(chat_room_header, chat_room_header_after);
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, success_requestedUserDoesNotExistInsideHeader) {
    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    int index = 0;
    for(auto& account : chat_room_header.accounts_in_chat_room) {
        if(account.account_oid == member_account_oid) {
            chat_room_header.accounts_in_chat_room.erase(
                    chat_room_header.accounts_in_chat_room.begin() + index
                    );
            break;
        }
        index++;
    }

    chat_room_header.setIntoCollection();

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    build_buildBasicUpdateOtherUserResponse_comparison(
            update_single_chat_room_member_response.timestamp_returned(),
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            member_account_oid.to_string(),
            -1L,
            update_single_chat_room_member_response
    );

    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    checkSuccessfulCallUpdatedValues(
            chat_room_header_after,
            update_single_chat_room_member_response.timestamp_returned()
    );
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, success_requestedUserAccountState_notInChatRoom) {
    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    for(auto& account : chat_room_header.accounts_in_chat_room) {
        if(account.account_oid == member_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
            break;
        }
    }

    chat_room_header.setIntoCollection();

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    build_updateSingleChatRoomMemberNotInChatRoom_comparison(
            update_single_chat_room_member_response.timestamp_returned(),
            member_account_oid.to_string(),
            chat_room_id,
            chat_room_header_after,
            update_single_chat_room_member_response,
            update_single_chat_room_member_request.chat_room_member_info()
    );

    checkSuccessfulCallUpdatedValues(
        chat_room_header_after,
        update_single_chat_room_member_response.timestamp_returned()
    );
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, success_requestedUserAccountState_inChatRoom_userAccountExists_containsChatRoomId) {
    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    build_updateSingleOtherUserOrEvent_comparison(
            update_single_chat_room_member_response.timestamp_returned(),
            member_account_oid.to_string(),
            chat_room_id,
            chat_room_header_after,
            update_single_chat_room_member_response,
            *update_single_chat_room_member_request.mutable_chat_room_member_info()
    );

    checkSuccessfulCallUpdatedValues(
            chat_room_header_after,
            update_single_chat_room_member_response.timestamp_returned()
    );
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, success_requestedUserAccountState_inChatRoom_userAccountExists_doesNotContainChatRoomId) {
    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    for(auto it = member_account_doc.chat_rooms.begin(); it != member_account_doc.chat_rooms.end(); ++it) {
        if(it->chat_room_id == chat_room_id) {
            member_account_doc.chat_rooms.erase(it);
            break;
        }
    }
    member_account_doc.setIntoCollection();

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    build_buildBasicUpdateOtherUserResponse_comparison(
            update_single_chat_room_member_response.timestamp_returned(),
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            member_account_oid.to_string(),
            -1L,
            update_single_chat_room_member_response
    );

    checkSuccessfulCallUpdatedValues(
            chat_room_header_after,
            update_single_chat_room_member_response.timestamp_returned()
    );
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, success_requestedUserAccountState_inChatRoom_userAccountDoesNotExist) {
    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    auto delete_result = user_accounts_collection.delete_one(
            document{}
                << "_id" << member_account_oid
            << finalize
    );

    ASSERT_TRUE(delete_result);
    EXPECT_EQ(delete_result->deleted_count(), 1);

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    build_buildBasicUpdateOtherUserResponse_comparison(
            update_single_chat_room_member_response.timestamp_returned(),
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            member_account_oid.to_string(),
            -1L,
            update_single_chat_room_member_response
    );

    checkSuccessfulCallUpdatedValues(
            chat_room_header_after,
            update_single_chat_room_member_response.timestamp_returned(),
            true
    );
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, sendingUserRequestsSelf) {

    //NOTE: Cannot think of any reason NOT to allow this, although it is a bit odd the ability to request self may
    // be useful in the future (even though login does something similar).

    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    UserPictureDoc admin_thumbnail_picture;

    for (const auto& pic : admin_account_doc.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            admin_thumbnail_picture.getFromCollection(picture_reference);

            break;
        }
    }

    const long current_timestamp_value = getCurrentTimestamp().count();

    auto chat_room_member_info = update_single_chat_room_member_request.mutable_chat_room_member_info();
    chat_room_member_info->set_account_oid(admin_account_doc.current_object_oid.to_string());
    chat_room_member_info->set_thumbnail_size_in_bytes(admin_thumbnail_picture.thumbnail_size_in_bytes);
    chat_room_member_info->set_thumbnail_index_number(admin_thumbnail_picture.picture_index);
    chat_room_member_info->set_thumbnail_timestamp(current_timestamp_value);

    chat_room_member_info->set_first_name(admin_account_doc.first_name);
    chat_room_member_info->set_age(admin_account_doc.age);
    chat_room_member_info->set_member_info_last_updated_timestamp(current_timestamp_value);

    int picture_index = 0;
    for(const auto& pic : admin_account_doc.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            auto picture_last_updated = chat_room_member_info->add_pictures_last_updated_timestamps();
            picture_last_updated->set_last_updated_timestamp(timestamp_stored.value.count());
            picture_last_updated->set_index_number(picture_index);
        }
        picture_index++;
    }

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    build_updateSingleOtherUserOrEvent_comparison(
            update_single_chat_room_member_response.timestamp_returned(),
            admin_account_oid.to_string(),
            chat_room_id,
            chat_room_header_after,
            update_single_chat_room_member_response,
            *update_single_chat_room_member_request.mutable_chat_room_member_info()
    );

    checkSuccessfulCallUpdatedValues(
            chat_room_header_after,
            update_single_chat_room_member_response.timestamp_returned()
    );
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, requestChatRoomEvent) {

    createAndStoreEventAdminAccount();

    //Sleep so current_timestamp is after admin account creation.
    std::this_thread::sleep_for(std::chrono::milliseconds{1});

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    auto event_info = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    //Add an event chat room
    auto event_created_return_values = generateRandomEvent(
            chat_room_admin_info,
            TEMP_ADMIN_ACCOUNT_NAME,
            current_timestamp,
            event_info
    );

    const std::string event_account_oid = event_created_return_values.event_account_oid;

    chat_room_id = event_created_return_values.chat_room_return_info.chat_room_id();
    chat_room_password = event_created_return_values.chat_room_return_info.chat_room_password();

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    ASSERT_FALSE(mock_server_writer.write_params.empty());
    if (!mock_server_writer.write_params.empty()) {
        ASSERT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            ASSERT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            ASSERT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    chat_room_header.getFromCollection(chat_room_id);
    admin_account_doc.getFromCollection(admin_account_oid);

    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    UserAccountDoc event_doc(bsoncxx::oid{event_account_oid});

    auto event_account_user = update_single_chat_room_member_request.mutable_chat_room_member_info();
    event_account_user->Clear();
    event_account_user->set_account_oid(event_account_oid);
    event_account_user->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_EVENT);
    event_account_user->set_first_name(event_doc.first_name);
    event_account_user->set_age(event_doc.age);
    event_account_user->set_member_info_last_updated_timestamp(current_timestamp.count());
    event_account_user->set_event_status(LetsGoEventStatus::ONGOING);

    //Making sure an update is required
    event_account_user->set_event_title(event_doc.event_values->event_title + "a");

    int picture_index = 0;
    bool thumbnail_found = false;
    for (const auto& pic: event_doc.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            auto pic_last_updated = event_account_user->add_pictures_last_updated_timestamps();
            pic_last_updated->set_index_number(picture_index);
            pic_last_updated->set_last_updated_timestamp(timestamp_stored.value.count());

            if (!thumbnail_found) {
                thumbnail_found = true;
                UserPictureDoc event_thumbnail_picture(picture_reference);

                event_account_user->set_thumbnail_size_in_bytes(event_thumbnail_picture.thumbnail_size_in_bytes);
                event_account_user->set_thumbnail_index_number(event_thumbnail_picture.picture_index);
                event_account_user->set_thumbnail_timestamp(event_thumbnail_picture.timestamp_stored);
            }
        }
        picture_index++;
    }

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    build_updateSingleOtherUserOrEvent_comparison(
            update_single_chat_room_member_response.timestamp_returned(),
            event_account_oid,
            chat_room_id,
            chat_room_header,
            update_single_chat_room_member_response,
            *event_account_user
    );

    checkSuccessfulCallUpdatedValues(
            chat_room_header_after,
            update_single_chat_room_member_response.timestamp_returned(),
            true
    );
}

TEST_F(GrpcUpdateSingleChatRoomMemberTesting, requestEventAdmin) {

    createAndStoreEventAdminAccount();

    //Sleep so current_timestamp is after admin account creation.
    std::this_thread::sleep_for(std::chrono::milliseconds{1});

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    auto event_info = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    //Add an event chat room
    auto event_created_return_values = generateRandomEvent(
            chat_room_admin_info,
            TEMP_ADMIN_ACCOUNT_NAME,
            current_timestamp,
            event_info
    );

    const std::string event_account_oid = event_created_return_values.event_account_oid;

    chat_room_id = event_created_return_values.chat_room_return_info.chat_room_id();
    chat_room_password = event_created_return_values.chat_room_return_info.chat_room_password();

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    ASSERT_FALSE(mock_server_writer.write_params.empty());
    if (!mock_server_writer.write_params.empty()) {
        ASSERT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            ASSERT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            ASSERT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    chat_room_header.getFromCollection(chat_room_id);
    admin_account_doc.getFromCollection(admin_account_oid);

    grpc_chat_commands::UpdateSingleChatRoomMemberRequest update_single_chat_room_member_request = setupSimpleUpdateSingleChatRoomMemberRequest();
    UpdateOtherUserResponse update_single_chat_room_member_response;

    UserAccountDoc event_admin_doc(event_admin_values::OID);

    auto event_admin_user_account = update_single_chat_room_member_request.mutable_chat_room_member_info();
    event_admin_user_account->Clear();
    event_admin_user_account->set_account_oid(event_admin_values::OID.to_string());
    event_admin_user_account->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_EVENT);
    event_admin_user_account->set_age(event_admin_values::AGE);
    event_admin_user_account->set_member_info_last_updated_timestamp(current_timestamp.count());

    //Making sure an update is required
    event_admin_user_account->set_first_name(event_admin_values::FIRST_NAME + "a");

    int picture_index = 0;
    bool thumbnail_found = false;
    for (const auto& pic: event_admin_doc.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            auto pic_last_updated = event_admin_user_account->add_pictures_last_updated_timestamps();
            pic_last_updated->set_index_number(picture_index);
            pic_last_updated->set_last_updated_timestamp(timestamp_stored.value.count());

            if (!thumbnail_found) {
                thumbnail_found = true;
                UserPictureDoc event_thumbnail_picture(picture_reference);

                event_admin_user_account->set_thumbnail_size_in_bytes(event_thumbnail_picture.thumbnail_size_in_bytes);
                event_admin_user_account->set_thumbnail_index_number(event_thumbnail_picture.picture_index);
                event_admin_user_account->set_thumbnail_timestamp(event_thumbnail_picture.timestamp_stored);
            }
        }
        picture_index++;
    }

    updateSingleChatRoomMember(
            &update_single_chat_room_member_request,
            &update_single_chat_room_member_response
    );

    ChatRoomHeaderDoc chat_room_header_after(chat_room_id);

    build_updateSingleOtherUserOrEvent_comparison(
            update_single_chat_room_member_response.timestamp_returned(),
            event_admin_values::OID.to_string(),
            chat_room_id,
            chat_room_header,
            update_single_chat_room_member_response,
            *event_admin_user_account
    );

    checkSuccessfulCallUpdatedValues(
            chat_room_header_after,
            update_single_chat_room_member_response.timestamp_returned(),
            true
    );
}
