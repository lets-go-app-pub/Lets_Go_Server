//
// Created by jeremiah on 6/1/22.
//

#include <utility_general_functions.h>
#include <general_values.h>
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>
#include <account_objects.h>
#include <mongocxx/exception/logic_error.hpp>
#include <ChatRoomCommands.pb.h>
#include <chat_rooms_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "utility_chat_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include "grpc_mock_stream/mock_stream.h"
#include <set_fields_functions.h>
#include <setup_login_info.h>
#include <generate_randoms.h>
#include <chat_room_message_keys.h>
#include <accepted_mime_types.h>
#include <generate_random_messages.h>
#include <chat_message_pictures_keys.h>
#include <deleted_objects.h>
#include <chat_room_values.h>
#include <user_account_keys.h>
#include <extract_thumbnail_from_verified_doc.h>

#include "utility_chat_functions_test.h"
#include "chat_change_stream_helper_functions.h"
#include "grpc_values.h"
#include "deleted_thumbnail_info.h"
#include "update_single_other_user.h"
#include "compare_equivalent_messages.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "user_event_commands.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class UtilityChatFunctions : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    static void setupForEventChatRoom(
            const std::chrono::milliseconds& current_timestamp,
            const bsoncxx::oid& requesting_account_oid,
            const bsoncxx::oid& first_account_oid,
            UserAccountDoc& requesting_user_account_doc,
            UserAccountDoc& first_user_account_doc,
            const EventRequestMessage& event_info,
            const CreateEventReturnValues& event_created_return_values,
            const bool canceled_or_expired_event,
            const HowToHandleMemberPictures request_pictures_amount = HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO
    ) {

        const std::string& chat_room_id = event_created_return_values.chat_room_return_info.chat_room_id();

        grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

        setupUserLoginInfo(
                join_chat_room_request.mutable_login_info(),
                first_account_oid,
                first_user_account_doc.logged_in_token,
                first_user_account_doc.installation_ids.front()
        );

        join_chat_room_request.set_chat_room_id(chat_room_id);
        join_chat_room_request.set_chat_room_password(
                event_created_return_values.chat_room_return_info.chat_room_password());

        grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

        joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

        EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
        if (join_mock_server_writer.write_params.size() >= 2) {
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
            if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
                EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
                EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                          ReturnStatus::SUCCESS);
            }
        }

        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
        mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

        mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
        mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

        //add some messages
        generateRandomTextMessage(
                first_account_oid,
                first_user_account_doc.logged_in_token,
                first_user_account_doc.installation_ids.front(),
                chat_room_id,
                generateUUID()
        );

        generateRandomPictureMessage(
                first_account_oid,
                first_user_account_doc.logged_in_token,
                first_user_account_doc.installation_ids.front(),
                chat_room_id,
                generateUUID()
        );

        generateRandomLocationMessage(
                first_account_oid,
                first_user_account_doc.logged_in_token,
                first_user_account_doc.installation_ids.front(),
                chat_room_id,
                generateUUID()
        );

        bsoncxx::builder::stream::document user_account_doc_builder;
        requesting_user_account_doc.convertToDocument(user_account_doc_builder);

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
        StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
                reply_vector);


        bool return_val = sendNewChatRoomAndMessages(
                mongo_cpp_client,
                accounts_db,
                user_accounts_collection,
                chat_room_collection,
                user_account_doc_builder.view(),
                chat_room_id,
                current_timestamp,
                current_timestamp,
                requesting_account_oid.to_string(),
                &storeAndSendMessagesToClient,
                request_pictures_amount,
                AmountOfMessage::COMPLETE_MESSAGE_INFO,
                false,
                false
        );

        EXPECT_TRUE(return_val);

        ChatRoomHeaderDoc chat_room_header_doc(chat_room_id);

        //make sure chat room header was found
        ASSERT_NE(chat_room_header_doc.chat_room_password, "");

        storeAndSendMessagesToClient.finalCleanup();

        UserAccountDoc after_requesting_user_account_doc(requesting_account_oid);

        ASSERT_EQ(reply_vector.size(), 1);
        ASSERT_EQ(reply_vector[0]->mutable_return_new_chat_message()->messages_list_size(), 10);

        //Messages
        //THIS_USER_JOINED_CHAT_ROOM_START_MESSAGE
        //THIS_USER_JOINED_CHAT_ROOM_MEMBER
        //THIS_USER_JOINED_CHAT_ROOM_MEMBER
        //THIS_USER_JOINED_CHAT_ROOM_MEMBER
        //ChatRoomCapMessage
        //DifferentUserJoinedChatRoomChatMessage
        //TextChatMessage
        //PictureChatMessage
        //LocationChatMessage
        //THIS_USER_JOINED_CHAT_ROOM_FINISHED

        ChatMessageToClient response;
        response.set_sent_by_account_id(requesting_account_oid.to_string());

        auto chat_message = response.mutable_message();
        auto this_user_joined_chat_room_start = chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_start_message();
        chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);
        chat_message->mutable_standard_message_info()->set_internal_force_send_message_to_current_user(true);

        auto chat_room_info = this_user_joined_chat_room_start->mutable_chat_room_info();
        chat_room_info->set_chat_room_id(chat_room_id);
        chat_room_info->set_chat_room_last_observed_time(current_timestamp.count());
        chat_room_info->set_chat_room_name(chat_room_header_doc.chat_room_name);
        chat_room_info->set_chat_room_password(chat_room_header_doc.chat_room_password);
        chat_room_info->set_chat_room_last_activity_time(chat_room_header_doc.chat_room_last_active_time.value.count());

        chat_room_info->set_longitude_pinned_location(event_info.location_longitude());
        chat_room_info->set_latitude_pinned_location(event_info.location_latitude());

        chat_room_info->set_event_oid(event_created_return_values.event_account_oid);

        if(canceled_or_expired_event || event_info.qr_code_file_size() == 0) {
            chat_room_info->set_qr_code_image_bytes(chat_room_values::QR_CODE_DEFAULT);
            chat_room_info->set_qr_code_message(chat_room_values::QR_CODE_MESSAGE_DEFAULT);
            chat_room_info->set_qr_code_timestamp(chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT);
        } else {
            chat_room_info->set_qr_code_image_bytes(event_info.qr_code_file_in_bytes());
            chat_room_info->set_qr_code_message(event_info.qr_code_message().empty() ? chat_room_values::QR_CODE_MESSAGE_DEFAULT : event_info.qr_code_message());
            chat_room_info->set_qr_code_timestamp(current_timestamp.count() + 2);
        }

        compareEquivalentMessages<ChatMessageToClient>(
                reply_vector[0]->mutable_return_new_chat_message()->messages_list()[0],
                response
        );

        auto this_user_joined_chat_room_member = chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_member_message();
        chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);

        //Save event
        {
            UserAccountDoc event_account_doc(*chat_room_header_doc.event_id);

            auto chat_room_member_info = this_user_joined_chat_room_member->mutable_member_info();
            chat_room_member_info->Clear();
            auto user_info = chat_room_member_info->mutable_user_info();

            user_info->set_account_oid(event_account_doc.current_object_oid.to_string());
            chat_room_member_info->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_EVENT);
            chat_room_member_info->set_account_last_activity_time(
                    chat_room_values::EVENT_USER_LAST_ACTIVITY_TIME_DEFAULT.count());

            bsoncxx::builder::stream::document builder;
            event_account_doc.convertToDocument(builder);

            HowToHandleMemberPictures request_event_picture = request_pictures_amount;
            if(canceled_or_expired_event) {
                request_event_picture = HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO;
            }

            return_val = saveUserInfoToMemberSharedInfoMessage(
                    mongo_cpp_client,
                    accounts_db,
                    user_accounts_collection,
                    builder.view(),
                    event_account_doc.current_object_oid,
                    user_info,
                    request_event_picture,
                    current_timestamp
            );

            compareEquivalentMessages<ChatMessageToClient>(
                    reply_vector[0]->mutable_return_new_chat_message()->messages_list()[1],
                    response
            );
        }

        int index = 2;
        for (const auto& account: chat_room_header_doc.accounts_in_chat_room) {
            auto chat_room_member_info = this_user_joined_chat_room_member->mutable_member_info();
            chat_room_member_info->Clear();
            auto user_info = chat_room_member_info->mutable_user_info();

            user_info->set_account_oid(account.account_oid.to_string());
            chat_room_member_info->set_account_state(account.state_in_chat_room);
            chat_room_member_info->set_account_last_activity_time(account.last_activity_time.value.count());

            UserAccountDoc member_user_account_doc(account.account_oid);

            bsoncxx::builder::stream::document builder;
            member_user_account_doc.convertToDocument(builder);

            return_val = saveUserInfoToMemberSharedInfoMessage(
                    mongo_cpp_client,
                    accounts_db,
                    user_accounts_collection,
                    builder.view(),
                    account.account_oid,
                    user_info,
                    request_pictures_amount,
                    current_timestamp
            );

            compareEquivalentMessages<ChatMessageToClient>(
                    reply_vector[0]->mutable_return_new_chat_message()->messages_list()[index],
                    response
            );

            index++;
        }

        chat_message->mutable_standard_message_info()->set_do_not_update_user_state(false);
        chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_finished_message()->set_match_made_chat_room_oid(
                "");
        chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        reply_vector[0]->mutable_return_new_chat_message()->messages_list()[9],
                        response
                )
        );
    }
};

class LeaveChatRoomTesting : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    std::chrono::milliseconds currentTimestamp{-1};

    UserAccountDoc user_account;

    ChatRoomHeaderDoc original_chat_room_header;

    UserPictureDoc thumbnail_picture;

    bsoncxx::builder::stream::document user_account_builder;

    ReturnStatus return_status = ReturnStatus::UNKNOWN;
    std::chrono::milliseconds timestamp_stored{-1};

    const std::function<void(
            const ReturnStatus&,
            const std::chrono::milliseconds&
    )> set_error_response = [&](
            const ReturnStatus& _return_status,
            const std::chrono::milliseconds& _timestamp_stored
    ) {
        return_status = _return_status;
        timestamp_stored = _timestamp_stored;
    };

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(generated_account_oid);

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                generated_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        //sleep to guarantee currentTimestamp comes after create chat room, cap message and different user joined
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

        //This must come after createChatRoom(), so the timestamp will be later than the chat room created time.
        currentTimestamp = getCurrentTimestamp();

        original_chat_room_header.getFromCollection(create_chat_room_response.chat_room_id());

        //get UserPictureDoc object for the generated user's thumbnail
        for (const auto& pic: user_account.pictures) {
            if (pic.pictureStored()) {
                bsoncxx::oid picture_reference;
                bsoncxx::types::b_date timestamp_picture_stored = bsoncxx::types::b_date{
                        std::chrono::milliseconds{-1L}};

                pic.getPictureReference(
                        picture_reference,
                        timestamp_picture_stored
                );

                thumbnail_picture.getFromCollection(picture_reference);

                break;
            }
        }
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    bool runFunction(mongocxx::client_session* callback_session) {

        //account was updated, must re-extract builder
        user_account_builder.clear();
        user_account.convertToDocument(user_account_builder);

        return leaveChatRoom(
                chat_room_db,
                accounts_db,
                user_accounts_collection,
                callback_session,
                user_account_builder.view(),
                create_chat_room_response.chat_room_id(),
                generated_account_oid,
                currentTimestamp,
                set_error_response
        );
    }
};

class ConvertChatMessageDocumentToChatMessageToClient : public ::testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::unique_ptr<ExtractUserInfoObjects> extract_user_info_objects = nullptr;

    bsoncxx::builder::stream::document message_doc;

    const bsoncxx::oid current_user_oid{};

    const std::string message_uuid = generateUUID();
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid sent_from_oid{};

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        extract_user_info_objects = std::make_unique<ExtractUserInfoObjects>(
                mongo_cpp_client, accounts_db, user_accounts_collection
        );

        message_doc
                << "_id" << message_uuid
                << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{current_timestamp}
                << chat_room_message_keys::MESSAGE_SENT_BY << sent_from_oid;
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }


};

class ExtractThumbnailFromHeaderAccountDocumentTests : public ::testing::Test {
protected:

    const std::string chat_room_id = generateRandomChatRoomId();
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid member_account_oid{};

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];


    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(UtilityChatFunctions, addOrRemoveChatRoomIdByMessageType_joinChatRoom) {

    ChatMessageToClient response;

    std::vector<std::string> add_chat_room_times_called;

    auto add_chat_room_lambda = [&](const std::string& account_oid) {
        add_chat_room_times_called.emplace_back(account_oid);
    };

    std::vector<std::string> remove_chat_room_times_called;

    auto remove_chat_room_lambda = [&](const std::string& account_oid) {
        remove_chat_room_times_called.emplace_back(account_oid);
    };

    bsoncxx::oid current_user_account_oid;

    response.set_sent_by_account_id(current_user_account_oid.to_string());
    response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

    addOrRemoveChatRoomIdByMessageType(
            response,
            add_chat_room_lambda,
            remove_chat_room_lambda
    );

    ASSERT_EQ(add_chat_room_times_called.size(), 1);

    EXPECT_EQ(add_chat_room_times_called[0], current_user_account_oid.to_string());

    EXPECT_EQ(remove_chat_room_times_called.size(), 0);
}

TEST_F(UtilityChatFunctions, addOrRemoveChatRoomIdByMessageType_leftChatRoom) {

    ChatMessageToClient response;

    std::vector<std::string> add_chat_room_times_called;

    auto add_chat_room_lambda = [&](const std::string& account_oid) {
        add_chat_room_times_called.emplace_back(account_oid);
    };

    std::vector<std::string> remove_chat_room_times_called;

    auto remove_chat_room_lambda = [&](const std::string& account_oid) {
        remove_chat_room_times_called.emplace_back(account_oid);
    };

    bsoncxx::oid current_user_account_oid;

    response.set_sent_by_account_id(current_user_account_oid.to_string());
    response.mutable_message()->mutable_message_specifics()->mutable_different_user_left_message();

    addOrRemoveChatRoomIdByMessageType(
            response,
            add_chat_room_lambda,
            remove_chat_room_lambda
    );

    EXPECT_EQ(add_chat_room_times_called.size(), 0);

    ASSERT_EQ(remove_chat_room_times_called.size(), 1);

    EXPECT_EQ(remove_chat_room_times_called[0], current_user_account_oid.to_string());
}

TEST_F(UtilityChatFunctions, addOrRemoveChatRoomIdByMessageType_userKicked) {

    ChatMessageToClient response;

    std::vector<std::string> add_chat_room_times_called;

    auto add_chat_room_lambda = [&](const std::string& account_oid) {
        add_chat_room_times_called.emplace_back(account_oid);
    };

    std::vector<std::string> remove_chat_room_times_called;

    auto remove_chat_room_lambda = [&](const std::string& account_oid) {
        remove_chat_room_times_called.emplace_back(account_oid);
    };

    bsoncxx::oid current_user_account_oid;
    bsoncxx::oid match_account_oid;

    response.set_sent_by_account_id(current_user_account_oid.to_string());
    response.mutable_message()->mutable_message_specifics()->mutable_user_kicked_message()->set_kicked_account_oid(
            match_account_oid.to_string());

    addOrRemoveChatRoomIdByMessageType(
            response,
            add_chat_room_lambda,
            remove_chat_room_lambda
    );

    EXPECT_EQ(add_chat_room_times_called.size(), 0);

    ASSERT_EQ(remove_chat_room_times_called.size(), 1);

    EXPECT_EQ(remove_chat_room_times_called[0], match_account_oid.to_string());
}

TEST_F(UtilityChatFunctions, addOrRemoveChatRoomIdByMessageType_userBanned) {

    ChatMessageToClient response;

    std::vector<std::string> add_chat_room_times_called;

    auto add_chat_room_lambda = [&](const std::string& account_oid) {
        add_chat_room_times_called.emplace_back(account_oid);
    };

    std::vector<std::string> remove_chat_room_times_called;

    auto remove_chat_room_lambda = [&](const std::string& account_oid) {
        remove_chat_room_times_called.emplace_back(account_oid);
    };

    bsoncxx::oid current_user_account_oid;
    bsoncxx::oid match_account_oid;

    response.set_sent_by_account_id(current_user_account_oid.to_string());
    response.mutable_message()->mutable_message_specifics()->mutable_user_banned_message()->set_banned_account_oid(
            match_account_oid.to_string());

    addOrRemoveChatRoomIdByMessageType(
            response,
            add_chat_room_lambda,
            remove_chat_room_lambda
    );

    EXPECT_EQ(add_chat_room_times_called.size(), 0);

    ASSERT_EQ(remove_chat_room_times_called.size(), 1);

    EXPECT_EQ(remove_chat_room_times_called[0], match_account_oid.to_string());
}

TEST_F(UtilityChatFunctions, addOrRemoveChatRoomIdByMessageType_matchCanceledMessage) {

    ChatMessageToClient response;

    std::vector<std::string> add_chat_room_times_called;

    auto add_chat_room_lambda = [&](const std::string& account_oid) {
        add_chat_room_times_called.emplace_back(account_oid);
    };

    std::vector<std::string> remove_chat_room_times_called;

    auto remove_chat_room_lambda = [&](const std::string& account_oid) {
        remove_chat_room_times_called.emplace_back(account_oid);
    };

    bsoncxx::oid current_user_account_oid;
    bsoncxx::oid match_account_oid;

    response.set_sent_by_account_id(current_user_account_oid.to_string());
    response.mutable_message()->mutable_message_specifics()->mutable_match_canceled_message()->set_matched_account_oid(
            match_account_oid.to_string());

    addOrRemoveChatRoomIdByMessageType(
            response,
            add_chat_room_lambda,
            remove_chat_room_lambda
    );

    EXPECT_EQ(add_chat_room_times_called.size(), 0);

    ASSERT_EQ(remove_chat_room_times_called.size(), 2);

    EXPECT_EQ(remove_chat_room_times_called[0], match_account_oid.to_string());
    EXPECT_EQ(remove_chat_room_times_called[1], current_user_account_oid.to_string());
}

TEST_F(UtilityChatFunctions, extractReplyFromMessage_notAReply) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{};

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractReplyFromMessage(
            chat_room_id,
            active_message_info_doc,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );
}

TEST_F(UtilityChatFunctions, extractReplyFromMessage_textMessageReply) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    const bsoncxx::oid sent_from_oid{};
    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kTextReply;
    const std::string chat_message_text = "message_text";

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << chat_room_message_keys::message_specifics::active_info::reply::CHAT_MESSAGE_TEXT << chat_message_text
            << close_document;

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractReplyFromMessage(
            chat_room_id,
            active_message_info_doc,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;

    generated_active_message_info.set_is_reply(true);
    generated_active_message_info.mutable_reply_info()->set_reply_is_sent_from_user_oid(sent_from_oid.to_string());
    generated_active_message_info.mutable_reply_info()->set_reply_is_to_message_uuid(reply_to_uuid);
    generated_active_message_info.mutable_reply_info()->mutable_reply_specifics()->mutable_text_reply()->set_message_text(
            chat_message_text);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );
}

TEST_F(UtilityChatFunctions, extractReplyFromMessage_pictureMessageReply) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    const bsoncxx::oid sent_from_oid{};
    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kPictureReply;
    const std::string picture_thumbnail_in_bytes = "picture_thumbnail_in_bytes";
    const int picture_thumbnail_size = (int) picture_thumbnail_in_bytes.size();

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_IN_BYTES << picture_thumbnail_in_bytes
            << chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_SIZE << picture_thumbnail_size
            << close_document;

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractReplyFromMessage(
            chat_room_id,
            active_message_info_doc,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;

    generated_active_message_info.set_is_reply(true);
    generated_active_message_info.mutable_reply_info()->set_reply_is_sent_from_user_oid(sent_from_oid.to_string());
    generated_active_message_info.mutable_reply_info()->set_reply_is_to_message_uuid(reply_to_uuid);
    generated_active_message_info.mutable_reply_info()->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_in_bytes(
            picture_thumbnail_in_bytes);
    generated_active_message_info.mutable_reply_info()->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_file_size(
            picture_thumbnail_size);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );

}

TEST_F(UtilityChatFunctions, extractReplyFromMessage_locationMessageReply) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    const bsoncxx::oid sent_from_oid{};
    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kLocationReply;

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << close_document;

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractReplyFromMessage(
            chat_room_id,
            active_message_info_doc,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;

    generated_active_message_info.set_is_reply(true);
    generated_active_message_info.mutable_reply_info()->set_reply_is_sent_from_user_oid(sent_from_oid.to_string());
    generated_active_message_info.mutable_reply_info()->set_reply_is_to_message_uuid(reply_to_uuid);
    generated_active_message_info.mutable_reply_info()->mutable_reply_specifics()->mutable_location_reply();

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );
}

TEST_F(UtilityChatFunctions, extractReplyFromMessage_mimeTypeMessageReply) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    const bsoncxx::oid sent_from_oid{};
    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kMimeReply;
    const std::string mime_type_thumbnail_in_bytes = "mime_type_thumbnail_in_bytes";
    const int mime_type_thumbnail_size = (int) mime_type_thumbnail_in_bytes.size();

    ASSERT_FALSE(accepted_mime_types.empty());

    const std::string reply_mime_type = *accepted_mime_types.begin();

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_IN_BYTES << mime_type_thumbnail_in_bytes
            << chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_SIZE << mime_type_thumbnail_size
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_MIME_TYPE << reply_mime_type
            << close_document;

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractReplyFromMessage(
            chat_room_id,
            active_message_info_doc,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;

    generated_active_message_info.set_is_reply(true);
    generated_active_message_info.mutable_reply_info()->set_reply_is_sent_from_user_oid(sent_from_oid.to_string());
    generated_active_message_info.mutable_reply_info()->set_reply_is_to_message_uuid(reply_to_uuid);
    generated_active_message_info.mutable_reply_info()->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_in_bytes(
            mime_type_thumbnail_in_bytes);
    generated_active_message_info.mutable_reply_info()->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_file_size(
            mime_type_thumbnail_size);
    generated_active_message_info.mutable_reply_info()->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_mime_type(
            reply_mime_type);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );
}

TEST_F(UtilityChatFunctions, extractReplyFromMessage_invitedMessageReply) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    const bsoncxx::oid sent_from_oid{};
    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kInviteReply;

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << close_document;

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractReplyFromMessage(
            chat_room_id,
            active_message_info_doc,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;

    generated_active_message_info.set_is_reply(true);
    generated_active_message_info.mutable_reply_info()->set_reply_is_sent_from_user_oid(sent_from_oid.to_string());
    generated_active_message_info.mutable_reply_info()->set_reply_is_to_message_uuid(reply_to_uuid);
    generated_active_message_info.mutable_reply_info()->mutable_reply_specifics()->mutable_invite_reply();

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );
}

TEST_F(UtilityChatFunctions, extractMessageDeleted_notDeleted) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    const bsoncxx::oid sent_from_oid{};
    const std::string reply_to_uuid = generateUUID();
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array;

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractMessageDeleted(
            chat_room_id,
            sent_from_oid.to_string(),
            active_message_info_doc,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );

}

TEST_F(UtilityChatFunctions, extractMessageDeleted_deletedForAll) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    const bsoncxx::oid sent_from_oid{};
    const std::string reply_to_uuid = generateUUID();
    const DeleteType delete_type = DeleteType::DELETE_FOR_ALL_USERS;

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array;

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractMessageDeleted(
            chat_room_id,
            sent_from_oid.to_string(),
            active_message_info_doc,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;
    generated_active_message_info.set_is_deleted(true);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );

}

TEST_F(UtilityChatFunctions, extractMessageDeleted_deletedForDifferentUser) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    const bsoncxx::oid sent_from_oid{};
    const std::string reply_to_uuid = generateUUID();
    const DeleteType delete_type = DeleteType::DELETE_FOR_SINGLE_USER;

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array
            << bsoncxx::oid{}.to_string()
            << close_array;

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractMessageDeleted(
            chat_room_id,
            sent_from_oid.to_string(),
            active_message_info_doc,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );

}

TEST_F(UtilityChatFunctions, extractMessageDeleted_deletedForCurrentUser) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    bsoncxx::builder::stream::document active_message_info_doc;

    const bsoncxx::oid sent_from_oid{};
    const std::string reply_to_uuid = generateUUID();
    const DeleteType delete_type = DeleteType::DELETE_FOR_SINGLE_USER;

    active_message_info_doc
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array
            << sent_from_oid.to_string()
            << close_array;

    ActiveMessageInfo passed_active_message_info;

    bool return_val = extractMessageDeleted(
            chat_room_id,
            sent_from_oid.to_string(),
            active_message_info_doc,
            &passed_active_message_info
    );

    EXPECT_TRUE(return_val);

    ActiveMessageInfo generated_active_message_info;
    generated_active_message_info.set_is_deleted(true);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_active_message_info,
                    passed_active_message_info
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, textMessage_skeletonOnly) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kTextMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = gen_random_alpha_numeric_string(rand() % 100 + 1);
    const bool text_is_edited = false;
    const std::chrono::milliseconds text_edited_time{-1};

    const AmountOfMessage amount_of_message = AmountOfMessage::ONLY_SKELETON;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << text_message
            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << text_is_edited
            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{text_edited_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, onlyStoreMessageBoolValue) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kTextMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = gen_random_alpha_numeric_string(rand() % 100 + 1);
    const bool text_is_edited = false;
    const std::chrono::milliseconds text_edited_time{-1};

    const AmountOfMessage amount_of_message = AmountOfMessage::ONLY_SKELETON;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << text_message
            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << text_is_edited
            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{text_edited_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            true,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(true);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, textMessage_enoughToDisplayAsFinalMessage_withReply) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kTextMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = gen_random_alpha_numeric_string(
            rand() % server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE);
    const bool text_is_edited = false;
    const std::chrono::milliseconds text_edited_time{-1};

    const AmountOfMessage amount_of_message = AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document << close_document
            << close_document
            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << text_message
            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << text_is_edited
            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{text_edited_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(false);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info()->set_is_reply(
            true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(
            text_is_edited);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(
            text_edited_time.count());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            text_message.substr(0, server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE));

    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
    std::cout << "passed_response\n" << passed_response.DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, textMessage_enoughToDisplayAsFinalMessage_fullMessageSentBack) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kTextMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = gen_random_alpha_numeric_string(
            rand() % server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE);
    const bool text_is_edited = false;
    const std::chrono::milliseconds text_edited_time{-1};

    const AmountOfMessage amount_of_message = AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << text_message
            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << text_is_edited
            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{text_edited_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(
            text_is_edited);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(
            text_edited_time.count());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            text_message.substr(0, server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE));

    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
    std::cout << "passed_response\n" << passed_response.DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient,
       textMessage_enoughToDisplayAsFinalMessage_fullMessageNOTSentBack) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kTextMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = gen_random_alpha_numeric_string(
            rand() % 1000 + 1 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE);
    const bool text_is_edited = false;
    const std::chrono::milliseconds text_edited_time{-1};

    const AmountOfMessage amount_of_message = AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << text_message
            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << text_is_edited
            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{text_edited_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(false);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(
            text_is_edited);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(
            text_edited_time.count());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            text_message.substr(0, server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE));

    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
    std::cout << "passed_response\n" << passed_response.DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, textMessage_completeMessageInfo) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kTextMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = gen_random_alpha_numeric_string(
            rand() % 1000 + 1 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE);
    const bool text_is_edited = false;
    const std::chrono::milliseconds text_edited_time{-1};

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << text_message
            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << text_is_edited
            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{text_edited_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(
            text_is_edited);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(
            text_edited_time.count());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            text_message);

    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
    std::cout << "passed_response\n" << passed_response.DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, textMessage_completeMessageInfo_withReply) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kTextMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = gen_random_alpha_numeric_string(
            rand() % 1000 + 1 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE);
    const bool text_is_edited = false;
    const std::chrono::milliseconds text_edited_time{-1};

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kTextReply;
    const std::string reply_message_text = gen_random_alpha_numeric_string(rand() % 1000 + 1);

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << chat_room_message_keys::message_specifics::active_info::reply::CHAT_MESSAGE_TEXT << reply_message_text
            << close_document
            << close_document
            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << text_message
            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << text_is_edited
            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{text_edited_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info()->set_is_reply(
            true);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            sent_from_oid.to_string());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            reply_to_uuid);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_text_reply()->set_message_text(
            reply_message_text);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(
            text_is_edited);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(
            text_edited_time.count());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            text_message);

    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
    std::cout << "passed_response\n" << passed_response.DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, textMessage_messageEdited) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kTextMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = gen_random_alpha_numeric_string(
            rand() % 1000 + 1 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE);
    const bool text_is_edited = true;
    const std::chrono::milliseconds text_edited_time{current_timestamp + std::chrono::milliseconds{1000}};

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << text_message
            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << text_is_edited
            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{text_edited_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_is_edited(
            text_is_edited);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_edited_time(
            text_edited_time.count());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            text_message);

    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
    std::cout << "passed_response\n" << passed_response.DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, pictureMessage_skeletonOnly) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kPictureMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = bsoncxx::oid{}.to_string();
    const int picture_image_width = rand() % 200 + 20;
    const int picture_image_height = rand() % 200 + 20;

    const AmountOfMessage amount_of_message = AmountOfMessage::ONLY_SKELETON;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::PICTURE_OID << text_message
            << chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH << picture_image_width
            << chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT << picture_image_height
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_height(
            picture_image_height);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_width(
            picture_image_width);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, pictureMessage_enoughToDisplayAsFinalMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kPictureMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = bsoncxx::oid{}.to_string();
    const int picture_image_width = rand() % 200 + 20;
    const int picture_image_height = rand() % 200 + 20;

    const AmountOfMessage amount_of_message = AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::PICTURE_OID << text_message
            << chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH << picture_image_width
            << chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT << picture_image_height
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_height(
            picture_image_height);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_width(
            picture_image_width);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, pictureMessage_completeMessageInfo) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kPictureMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = bsoncxx::oid{}.to_string();

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    ChatMessagePictureDoc chat_message_pic = generateRandomChatPicture(chat_room_id);
    chat_message_pic.setIntoCollection();

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::PICTURE_OID << chat_message_pic.current_object_oid.to_string()
            << chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH << chat_message_pic.width
            << chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT << chat_message_pic.height
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_height(
            chat_message_pic.height);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_width(
            chat_message_pic.width);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_picture_file_in_bytes(
            chat_message_pic.picture_in_bytes);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_picture_file_size(
            chat_message_pic.picture_size_in_bytes);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, pictureMessage_completeMessageInfo_withReply) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kPictureMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;

    const std::string text_message = bsoncxx::oid{}.to_string();

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    ChatMessagePictureDoc chat_message_pic = generateRandomChatPicture(chat_room_id);
    chat_message_pic.setIntoCollection();

    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kLocationReply;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << close_document
            << close_document
            << chat_room_message_keys::message_specifics::PICTURE_OID << chat_message_pic.current_object_oid.to_string()
            << chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH << chat_message_pic.width
            << chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT << chat_message_pic.height
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->mutable_active_message_info()->set_is_reply(
            true);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            reply_to_uuid);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            sent_from_oid.to_string());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_location_reply();

    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_height(
            chat_message_pic.height);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_image_width(
            chat_message_pic.width);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_picture_file_in_bytes(
            chat_message_pic.picture_in_bytes);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_picture_message()->set_picture_file_size(
            chat_message_pic.picture_size_in_bytes);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, locationMessage_skeletonOnly) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kLocationMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    double longitude;
    double latitude;

    const AmountOfMessage amount_of_message = AmountOfMessage::ONLY_SKELETON;

    //10e4 will give 4 decimal places of precision (NOTE: This method will not work for more because of
    // the precision of doubles).
    int precision = 10e4;

    //latitude must be a number -90 to 90
    latitude = rand() % (90 * precision);
    if (rand() % 2) {
        latitude *= -1;
    }
    latitude /= precision;

    //longitude must be a number -180 to 180
    longitude = rand() % (180 * precision);
    if (rand() % 2) {
        longitude *= -1;
    }
    longitude /= precision;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::LOCATION_LONGITUDE << longitude
            << chat_room_message_keys::message_specifics::LOCATION_LATITUDE << latitude
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->set_latitude(
            latitude);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->set_longitude(
            longitude);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, locationMessage_enoughToDisplayAsFinalMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kLocationMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    double longitude;
    double latitude;

    const AmountOfMessage amount_of_message = AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE;

    //10e4 will give 4 decimal places of precision (NOTE: This method will not work for more because of
    // the precision of doubles).
    int precision = 10e4;

    //latitude must be a number -90 to 90
    latitude = rand() % (90 * precision);
    if (rand() % 2) {
        latitude *= -1;
    }
    latitude /= precision;

    //longitude must be a number -180 to 180
    longitude = rand() % (180 * precision);
    if (rand() % 2) {
        longitude *= -1;
    }
    longitude /= precision;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::LOCATION_LONGITUDE << longitude
            << chat_room_message_keys::message_specifics::LOCATION_LATITUDE << latitude
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->set_latitude(
            latitude);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->set_longitude(
            longitude);

    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
    std::cout << "passed_response\n" << passed_response.DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, locationMessage_completeMessageInfo) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kLocationMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    double longitude;
    double latitude;

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    //10e4 will give 4 decimal places of precision (NOTE: This method will not work for more because of
    // the precision of doubles).
    int precision = 10e4;

    //latitude must be a number -90 to 90
    latitude = rand() % (90 * precision);
    if (rand() % 2) {
        latitude *= -1;
    }
    latitude /= precision;

    //longitude must be a number -180 to 180
    longitude = rand() % (180 * precision);
    if (rand() % 2) {
        longitude *= -1;
    }
    longitude /= precision;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::LOCATION_LONGITUDE << longitude
            << chat_room_message_keys::message_specifics::LOCATION_LATITUDE << latitude
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->set_latitude(
            latitude);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->set_longitude(
            longitude);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, locationMessage_completeMessageInfo_withReply) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kLocationMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    double longitude;
    double latitude;

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kLocationReply;

    //10e4 will give 4 decimal places of precision (NOTE: This method will not work for more because of
    // the precision of doubles).
    int precision = 10e4;

    //latitude must be a number -90 to 90
    latitude = rand() % (90 * precision);
    if (rand() % 2) {
        latitude *= -1;
    }
    latitude /= precision;

    //longitude must be a number -180 to 180
    longitude = rand() % (180 * precision);
    if (rand() % 2) {
        longitude *= -1;
    }
    longitude /= precision;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << close_document
            << close_document
            << chat_room_message_keys::message_specifics::LOCATION_LONGITUDE << longitude
            << chat_room_message_keys::message_specifics::LOCATION_LATITUDE << latitude
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::COMPLETE_MESSAGE_INFO);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->mutable_active_message_info()->set_is_reply(
            true);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            reply_to_uuid);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            sent_from_oid.to_string());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_location_reply();

    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->set_latitude(
            latitude);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_location_message()->set_longitude(
            longitude);

    std::cout << "passed_response\n" << passed_response.DebugString() << '\n';
    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, mimeTypeMessage_skeletonOnly) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kMimeTypeMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    const std::string mime_type_url = gen_random_alpha_numeric_string(rand() % 100 + 100);
    const std::string mime_type_type = *accepted_mime_types.begin();
    const int mime_type_image_width = rand() % 2000 + 200;
    const int mime_type_image_height = rand() % 2000 + 200;

    const AmountOfMessage amount_of_message = AmountOfMessage::ONLY_SKELETON;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::MIME_TYPE_URL << mime_type_url
            << chat_room_message_keys::message_specifics::MIME_TYPE_TYPE << mime_type_type
            << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_WIDTH << mime_type_image_width
            << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_HEIGHT << mime_type_image_height
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_url_of_download(
            mime_type_url);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_mime_type(
            mime_type_type);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_width(
            mime_type_image_width);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_height(
            mime_type_image_height);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, mimeTypeMessage_enoughToDisplayAsFinalMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kMimeTypeMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    const std::string mime_type_url = gen_random_alpha_numeric_string(rand() % 100 + 100);
    const std::string mime_type_type = *accepted_mime_types.begin();
    const int mime_type_image_width = rand() % 2000 + 200;
    const int mime_type_image_height = rand() % 2000 + 200;

    const AmountOfMessage amount_of_message = AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::MIME_TYPE_URL << mime_type_url
            << chat_room_message_keys::message_specifics::MIME_TYPE_TYPE << mime_type_type
            << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_WIDTH << mime_type_image_width
            << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_HEIGHT << mime_type_image_height
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_url_of_download(
            mime_type_url);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_mime_type(
            mime_type_type);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_width(
            mime_type_image_width);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_height(
            mime_type_image_height);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, mimeTypeMessage_completeMessageInfo) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kMimeTypeMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    const std::string mime_type_url = gen_random_alpha_numeric_string(rand() % 100 + 100);
    const std::string mime_type_type = *accepted_mime_types.begin();
    const int mime_type_image_width = rand() % 2000 + 200;
    const int mime_type_image_height = rand() % 2000 + 200;

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::MIME_TYPE_URL << mime_type_url
            << chat_room_message_keys::message_specifics::MIME_TYPE_TYPE << mime_type_type
            << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_WIDTH << mime_type_image_width
            << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_HEIGHT << mime_type_image_height
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_url_of_download(
            mime_type_url);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_mime_type(
            mime_type_type);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_width(
            mime_type_image_width);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_height(
            mime_type_image_height);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, mimeTypeMessage_completeMessageInfo_withReply) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kMimeTypeMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    const std::string mime_type_url = gen_random_alpha_numeric_string(rand() % 100 + 100);
    const std::string mime_type_type = *accepted_mime_types.begin();
    const int mime_type_image_width = rand() % 2000 + 200;
    const int mime_type_image_height = rand() % 2000 + 200;

    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kLocationReply;

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << close_document
            << close_document
            << chat_room_message_keys::message_specifics::MIME_TYPE_URL << mime_type_url
            << chat_room_message_keys::message_specifics::MIME_TYPE_TYPE << mime_type_type
            << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_WIDTH << mime_type_image_width
            << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_HEIGHT << mime_type_image_height
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->mutable_active_message_info()->set_is_reply(
            true);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            reply_to_uuid);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            sent_from_oid.to_string());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_location_reply();

    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_url_of_download(
            mime_type_url);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_mime_type(
            mime_type_type);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_width(
            mime_type_image_width);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->set_image_height(
            mime_type_image_height);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, inviteTypeMessage_skeletonOnly) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kInviteMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    const std::string invited_user_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_id = generateRandomChatRoomId();
    const std::string invited_chat_room_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_password = gen_random_alpha_numeric_string(rand() % 100 + 5);

    const AmountOfMessage amount_of_message = AmountOfMessage::ONLY_SKELETON;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID << current_user_oid.to_string()
            << chat_room_message_keys::message_specifics::INVITED_USER_NAME << invited_user_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID << invited_chat_room_id
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME << invited_chat_room_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD << invited_chat_room_password
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_account_oid(
            current_user_oid.to_string());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_name(
            invited_user_name);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_id(
            invited_chat_room_id);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_name(
            invited_chat_room_name);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_password(
            invited_chat_room_password);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, inviteTypeMessage_sentToWrongUser) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kInviteMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    const std::string invited_user_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_id = generateRandomChatRoomId();
    const std::string invited_chat_room_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_password = gen_random_alpha_numeric_string(rand() % 100 + 5);

    const AmountOfMessage amount_of_message = AmountOfMessage::ONLY_SKELETON;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID << bsoncxx::oid{}.to_string()
            << chat_room_message_keys::message_specifics::INVITED_USER_NAME << invited_user_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID << invited_chat_room_id
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME << invited_chat_room_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD << invited_chat_room_password
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info()->set_is_deleted(
            true);

    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
    std::cout << "passed_response\n" << passed_response.DebugString() << '\n';

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, inviteTypeMessage_enoughToDisplayAsFinalMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kInviteMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    const std::string invited_user_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_id = generateRandomChatRoomId();
    const std::string invited_chat_room_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_password = gen_random_alpha_numeric_string(rand() % 100 + 5);

    const AmountOfMessage amount_of_message = AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID << current_user_oid.to_string()
            << chat_room_message_keys::message_specifics::INVITED_USER_NAME << invited_user_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID << invited_chat_room_id
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME << invited_chat_room_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD << invited_chat_room_password
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_account_oid(
            current_user_oid.to_string());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_name(
            invited_user_name);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_id(
            invited_chat_room_id);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_name(
            invited_chat_room_name);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_password(
            invited_chat_room_password);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, inviteTypeMessage_completeMessageInfo) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kInviteMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    const std::string invited_user_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_id = generateRandomChatRoomId();
    const std::string invited_chat_room_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_password = gen_random_alpha_numeric_string(rand() % 100 + 5);

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
            << close_document
            << chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID << current_user_oid.to_string()
            << chat_room_message_keys::message_specifics::INVITED_USER_NAME << invited_user_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID << invited_chat_room_id
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME << invited_chat_room_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD << invited_chat_room_password
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_account_oid(
            current_user_oid.to_string());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_name(
            invited_user_name);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_id(
            invited_chat_room_id);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_name(
            invited_chat_room_name);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_password(
            invited_chat_room_password);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, inviteTypeMessage_completeMessageInfo_withReply) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kInviteMessage;
    const DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET;
    const std::string text_message = bsoncxx::oid{}.to_string();
    const std::string invited_user_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_id = generateRandomChatRoomId();
    const std::string invited_chat_room_name = gen_random_alpha_numeric_string(rand() % 100 + 5);
    const std::string invited_chat_room_password = gen_random_alpha_numeric_string(rand() % 100 + 5);

    const std::string reply_to_uuid = generateUUID();
    const ReplySpecifics::ReplyBodyCase reply_type = ReplySpecifics::ReplyBodyCase::kLocationReply;

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << delete_type
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array
            << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << sent_from_oid.to_string()
            << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << reply_to_uuid
            << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << reply_type
            << close_document
            << close_document
            << chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID << current_user_oid.to_string()
            << chat_room_message_keys::message_specifics::INVITED_USER_NAME << invited_user_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID << invited_chat_room_id
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME << invited_chat_room_name
            << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD << invited_chat_room_password
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::COMPLETE_MESSAGE_INFO);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info()->set_is_reply(
            true);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_to_message_uuid(
            reply_to_uuid);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info()->mutable_reply_info()->set_reply_is_sent_from_user_oid(
            sent_from_oid.to_string());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info()->mutable_reply_info()->mutable_reply_specifics()->mutable_location_reply();

    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info();
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_account_oid(
            current_user_oid.to_string());
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_name(
            invited_user_name);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_id(
            invited_chat_room_id);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_name(
            invited_chat_room_name);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_chat_room_password(
            invited_chat_room_password);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, userKickedMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kUserKickedMessage;
    const std::string user_kicked_kicked_oid = bsoncxx::oid{}.to_string();

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::USER_KICKED_KICKED_OID << user_kicked_kicked_oid
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_user_kicked_message()->set_kicked_account_oid(
            user_kicked_kicked_oid);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, userBannedMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kUserBannedMessage;
    const std::string user_banned_banned_oid = bsoncxx::oid{}.to_string();

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::USER_BANNED_BANNED_OID << user_banned_banned_oid
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_user_banned_message()->set_banned_account_oid(
            user_banned_banned_oid);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, differentUserJoinedMessage_amountOfMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage;
    const AccountStateInChatRoom account_state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE << account_state_in_chat_room
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->set_account_state(
            account_state_in_chat_room);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->set_account_last_activity_time(
            current_timestamp.count());

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, differentUserJoinedMessage_infoWithoutImages) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage;
    const AccountStateInChatRoom account_state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;

    message_doc.clear();

    message_doc
            << "_id" << message_uuid
            << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{current_timestamp}
            << chat_room_message_keys::MESSAGE_SENT_BY << generated_account_oid
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE << account_state_in_chat_room
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::INFO_WITHOUT_IMAGES,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(generated_account_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->set_account_state(
            account_state_in_chat_room);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->set_account_last_activity_time(
            current_timestamp.count());

    return_val = getUserAccountInfoForUserJoinedChatRoom(
            extract_user_info_objects->mongoCppClient,
            extract_user_info_objects->accountsDB,
            extract_user_info_objects->userAccountsCollection,
            chat_room_id, generated_account_oid.to_string(),
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
            generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info()
    );

    EXPECT_TRUE(return_val);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info()->set_current_timestamp(
            passed_response.message().message_specifics().different_user_joined_message().member_info().user_info().current_timestamp()
    );

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, differentUserJoinedMessage_infoWithThumbnailNoPicutures) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage;
    const AccountStateInChatRoom account_state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;

    message_doc.clear();

    message_doc
            << "_id" << message_uuid
            << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{current_timestamp}
            << chat_room_message_keys::MESSAGE_SENT_BY << generated_account_oid
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE << account_state_in_chat_room
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::INFO_WITH_THUMBNAIL_NO_PICTURES,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(generated_account_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->set_account_state(
            account_state_in_chat_room);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->set_account_last_activity_time(
            current_timestamp.count());

    return_val = getUserAccountInfoForUserJoinedChatRoom(
            extract_user_info_objects->mongoCppClient,
            extract_user_info_objects->accountsDB,
            extract_user_info_objects->userAccountsCollection,
            chat_room_id, generated_account_oid.to_string(),
            HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
            generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info()
    );

    EXPECT_TRUE(return_val);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info()->set_current_timestamp(
            passed_response.message().message_specifics().different_user_joined_message().member_info().user_info().current_timestamp()
    );

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, differentUserJoinedMessage_allInfoAndImages) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage;
    const AccountStateInChatRoom account_state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;

    message_doc.clear();

    message_doc
            << "_id" << message_uuid
            << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{current_timestamp}
            << chat_room_message_keys::MESSAGE_SENT_BY << generated_account_oid
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE << account_state_in_chat_room
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::ALL_INFO_AND_IMAGES,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(generated_account_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->set_account_state(
            account_state_in_chat_room);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->set_account_last_activity_time(
            current_timestamp.count());

    return_val = getUserAccountInfoForUserJoinedChatRoom(
            extract_user_info_objects->mongoCppClient,
            extract_user_info_objects->accountsDB,
            extract_user_info_objects->userAccountsCollection,
            chat_room_id, generated_account_oid.to_string(),
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info()
    );

    EXPECT_TRUE(return_val);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info()->set_current_timestamp(
            passed_response.message().message_specifics().different_user_joined_message().member_info().user_info().current_timestamp()
    );

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, differentUserLeftMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kDifferentUserLeftMessage;
    const std::string user_new_account_admin_oid = bsoncxx::oid{}.to_string();

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::USER_NEW_ACCOUNT_ADMIN_OID << user_new_account_admin_oid
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_different_user_left_message()->set_new_admin_account_oid(
            user_new_account_admin_oid);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, chatRoomNameUpdatedMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kChatRoomNameUpdatedMessage;
    const std::string name_new_name = gen_random_alpha_numeric_string(rand() % 100);

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::NAME_NEW_NAME << name_new_name
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_chat_room_name_updated_message()->set_new_chat_room_name(
            name_new_name);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, chatRoomPasswordUpdatedMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kChatRoomPasswordUpdatedMessage;
    const std::string password_new_password = gen_random_alpha_numeric_string(rand() % 100);

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::PASSWORD_NEW_PASSWORD << password_new_password
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_chat_room_password_updated_message()->set_new_chat_room_password(
            password_new_password);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, newAdminPromotedMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kNewAdminPromotedMessage;
    const std::string new_admin_admin_account_oid = bsoncxx::oid{}.to_string();

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::NEW_ADMIN_ADMIN_ACCOUNT_OID << new_admin_admin_account_oid
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_new_admin_promoted_message()->set_promoted_account_oid(
            new_admin_admin_account_oid);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, newLocationPinnedMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kNewPinnedLocationMessage;
    const std::string new_admin_admin_account_oid = bsoncxx::oid{}.to_string();

    // the precision of doubles).
    int precision = 10e4;

    //longitude must be a number -180 to 180
    double generated_longitude = rand() % (180*precision);
    if(rand() % 2) {
        generated_longitude *= -1;
    }
    generated_longitude /= precision;

    double generated_latitude = rand() % (90*precision);
    if(rand() % 2) {
        generated_latitude *= -1;
    }
    generated_latitude /= precision;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                << chat_room_message_keys::message_specifics::PINNED_LOCATION_LONGITUDE << bsoncxx::types::b_double{generated_longitude}
                << chat_room_message_keys::message_specifics::PINNED_LOCATION_LATITUDE << bsoncxx::types::b_double{generated_latitude}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_new_pinned_location_message()->set_longitude(
            generated_longitude);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_new_pinned_location_message()->set_latitude(
            generated_latitude);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, matchCanceledMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kMatchCanceledMessage;
    const std::string match_canceled_account_oid = bsoncxx::oid{}.to_string();

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::MATCH_CANCELED_ACCOUNT_OID << match_canceled_account_oid
            << close_document;

    ChatMessageToClient passed_response;
    bool called_from_chat_stream_initialization = false;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            called_from_chat_stream_initialization,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
    generated_response.mutable_message()->mutable_standard_message_info()->set_do_not_update_user_state(
            called_from_chat_stream_initialization);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_match_canceled_message()->set_matched_account_oid(
            match_canceled_account_oid);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    called_from_chat_stream_initialization = true;
    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            called_from_chat_stream_initialization,
            nullptr
    );

    EXPECT_TRUE(return_val);

    generated_response.mutable_message()->mutable_standard_message_info()->set_do_not_update_user_state(
            called_from_chat_stream_initialization);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            called_from_chat_stream_initialization,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, editedMessage_skeletonOnly) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kEditedMessage;
    const std::string edited_modified_message_uuid = generateUUID();
    const std::string edited_new_message_text = gen_random_alpha_numeric_string(rand() % 5000 + 1);
    const std::string edited_previous_message_text = gen_random_alpha_numeric_string(rand() % 5000 + 1);
    const std::chrono::milliseconds edited_modified_message_created_time{
            current_timestamp - std::chrono::milliseconds{1000}};

    const AmountOfMessage amount_of_message = AmountOfMessage::ONLY_SKELETON;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::EDITED_THIS_MESSAGE_UUID << message_uuid
            << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID << edited_modified_message_uuid
            << chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT << edited_new_message_text
            << chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT << edited_previous_message_text
            << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_CREATED_TIME << bsoncxx::types::b_date{
            edited_modified_message_created_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_edited_message()->set_message_uuid(
            edited_modified_message_uuid);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient,
       editedMessage_enoughToDisplayAsFinalMessage_fullMessageSentBack) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kEditedMessage;
    const std::string edited_modified_message_uuid = generateUUID();
    const std::string edited_new_message_text = gen_random_alpha_numeric_string(
            rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE - 1) + 1);
    const std::string edited_previous_message_text = gen_random_alpha_numeric_string(rand() % 5000 + 1);
    const std::chrono::milliseconds edited_modified_message_created_time{
            current_timestamp - std::chrono::milliseconds{1000}};

    const AmountOfMessage amount_of_message = AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::EDITED_THIS_MESSAGE_UUID << message_uuid
            << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID << edited_modified_message_uuid
            << chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT << edited_new_message_text
            << chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT << edited_previous_message_text
            << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_CREATED_TIME << bsoncxx::types::b_date{
            edited_modified_message_created_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_edited_message()->set_message_uuid(
            edited_modified_message_uuid);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_edited_message()->set_new_message(
            edited_new_message_text);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient,
       editedMessage_enoughToDisplayAsFinalMessage_fullMessageNOTSentBack) {

    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kEditedMessage;
    const std::string edited_modified_message_uuid = generateUUID();
    const std::string edited_new_message_text = gen_random_alpha_numeric_string(
            rand() % 1000 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1);
    const std::string edited_previous_message_text = gen_random_alpha_numeric_string(rand() % 5000 + 1);
    const std::chrono::milliseconds edited_modified_message_created_time{
            current_timestamp - std::chrono::milliseconds{1000}};

    const AmountOfMessage amount_of_message = AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::EDITED_THIS_MESSAGE_UUID << message_uuid
            << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID << edited_modified_message_uuid
            << chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT << edited_new_message_text
            << chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT << edited_previous_message_text
            << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_CREATED_TIME << bsoncxx::types::b_date{
            edited_modified_message_created_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_edited_message()->set_message_uuid(
            edited_modified_message_uuid);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_edited_message()->set_new_message(
            edited_new_message_text.substr(0,
                                           server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE));

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, editedMessage_completeMessageInfo) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kEditedMessage;
    const std::string edited_modified_message_uuid = generateUUID();
    const std::string edited_new_message_text = gen_random_alpha_numeric_string(
            rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE - 1) + 1);
    const std::string edited_previous_message_text = gen_random_alpha_numeric_string(rand() % 5000 + 1);
    const std::chrono::milliseconds edited_modified_message_created_time{
            current_timestamp - std::chrono::milliseconds{1000}};

    const AmountOfMessage amount_of_message = AmountOfMessage::COMPLETE_MESSAGE_INFO;

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::EDITED_THIS_MESSAGE_UUID << message_uuid
            << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID << edited_modified_message_uuid
            << chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT << edited_new_message_text
            << chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT << edited_previous_message_text
            << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_CREATED_TIME << bsoncxx::types::b_date{
            edited_modified_message_created_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            amount_of_message,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(amount_of_message);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_edited_message()->set_message_uuid(
            edited_modified_message_uuid);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_edited_message()->set_new_message(
            edited_new_message_text);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, deletedMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kDeletedMessage;
    const std::string deleted_modified_message_uuid = generateUUID();
    const DeleteType deleted_deleted_type = DeleteType::DELETE_FOR_ALL_USERS;
    const std::chrono::milliseconds deleted_modified_message_created_time{
            current_timestamp - std::chrono::milliseconds{1000}};

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << chat_room_message_keys::message_specifics::DELETED_THIS_MESSAGE_UUID << message_uuid
            << chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_UUID << deleted_modified_message_uuid
            << chat_room_message_keys::message_specifics::DELETED_DELETED_TYPE << deleted_deleted_type
            << chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_CREATED_TIME << bsoncxx::types::b_date{
            deleted_modified_message_created_time}
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_deleted_message()->set_message_uuid(
            deleted_modified_message_uuid);
    generated_response.mutable_message()->mutable_message_specifics()->mutable_deleted_message()->set_delete_type(
            deleted_deleted_type);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, userActivityDetectedMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kUserActivityDetectedMessage;
    const std::string deleted_modified_message_uuid = generateUUID();
    const std::chrono::milliseconds deleted_modified_message_created_time{
            current_timestamp - std::chrono::milliseconds{1000}};

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.clear_message_uuid();
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_user_activity_detected_message();

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(ConvertChatMessageDocumentToChatMessageToClient, chatRoomCapMessage) {
    const std::string chat_room_id = generateRandomChatRoomId(8);

    const MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::kChatRoomCapMessage;
    const std::string deleted_modified_message_uuid = generateUUID();
    const std::chrono::milliseconds deleted_modified_message_created_time{
            current_timestamp - std::chrono::milliseconds{1000}};

    message_doc
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
            << close_document;

    ChatMessageToClient passed_response;

    bool return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    ChatMessageToClient generated_response;

    generated_response.set_message_uuid(message_uuid);
    generated_response.set_sent_by_account_id(sent_from_oid.to_string());
    generated_response.set_timestamp_stored(current_timestamp.count());

    generated_response.set_only_store_message(false);

    generated_response.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    generated_response.mutable_message()->mutable_standard_message_info()->set_amount_of_message(
            AmountOfMessage::ONLY_SKELETON);
    generated_response.mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);

    generated_response.mutable_message()->mutable_message_specifics()->mutable_chat_room_cap_message();

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

    passed_response.Clear();

    return_val = convertChatMessageDocumentToChatMessageToClient(
            message_doc,
            chat_room_id,
            current_user_oid.to_string(),
            false,
            &passed_response,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            extract_user_info_objects.get()
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );
}

TEST_F(UtilityChatFunctions, extractChatPicture) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    const std::string chat_room_id = generateRandomChatRoomId();

    ChatMessagePictureDoc chat_message_pic = generateRandomChatPicture(chat_room_id);
    chat_message_pic.setIntoCollection();

    UserAccountDoc generated_account_doc(generated_account_oid);
    const std::string generated_message_uuid = generateUUID();

    generateRandomPictureMessage(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_message_pictures_collection = chat_room_db[collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME];

    int returned_picture_size = -2;
    int returned_picture_height = -2;
    int returned_picture_width = -2;
    std::string picture_byte_string;

    const auto pic_successfully_extracted = [&](
            const int pictureSize,
            const int pictureHeight,
            const int pictureWidth,
            std::string& pictureByteString
    ) {
        returned_picture_size = pictureSize;
        returned_picture_height = pictureHeight;
        returned_picture_width = pictureWidth;
        picture_byte_string = pictureByteString;
    };

    bool pic_not_found_or_corrupt_bool = false;

    const auto pic_not_found_or_corrupt = [&]() {
        pic_not_found_or_corrupt_bool = true;
    };

    bool return_val = extractChatPicture(
            mongo_cpp_client,
            chat_message_pic.current_object_oid.to_string(),
            chat_room_id,
            generated_account_oid.to_string(),
            generated_message_uuid,
            pic_successfully_extracted,
            pic_not_found_or_corrupt
    );

    EXPECT_TRUE(return_val);

    EXPECT_FALSE(pic_not_found_or_corrupt_bool);

    auto chat_message_picture = chat_message_pictures_collection.find_one(document{} << finalize);

    ASSERT_TRUE(chat_message_picture);

    EXPECT_EQ(returned_picture_size,
              chat_message_picture->view()[chat_message_pictures_keys::PICTURE_SIZE_IN_BYTES].get_int32().value);
    EXPECT_EQ(returned_picture_height,
              chat_message_picture->view()[chat_message_pictures_keys::HEIGHT].get_int32().value);
    EXPECT_EQ(returned_picture_width,
              chat_message_picture->view()[chat_message_pictures_keys::WIDTH].get_int32().value);
    EXPECT_EQ(picture_byte_string,
              chat_message_picture->view()[chat_message_pictures_keys::PICTURE_IN_BYTES].get_string().value.to_string());
}

TEST_F(UtilityChatFunctions, extractChatPicture_fileCorrupt) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account_doc(generated_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    ChatMessagePictureDoc chat_message_pic = generateRandomChatPicture(chat_room_id);
    chat_message_pic.setIntoCollection();

    const std::string generated_message_uuid = generateUUID();

    generateRandomPictureMessage(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_message_pictures_collection = chat_room_db[collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME];

    auto chat_message_picture = chat_message_pictures_collection.find_one(document{} << finalize);

    ASSERT_TRUE(chat_message_picture);

    bsoncxx::oid chat_message_picture_id = chat_message_picture->view()["_id"].get_oid().value;

    ChatMessagePictureDoc chat_message_picture_doc(chat_message_picture_id);

    //corrupt picture
    chat_message_picture_doc.picture_size_in_bytes--;
    chat_message_picture_doc.setIntoCollection();

    int returned_picture_size = -2;
    int returned_picture_height = -2;
    int returned_picture_width = -2;
    std::string picture_byte_string;

    const auto pic_successfully_extracted = [&](
            const int pictureSize,
            const int pictureHeight,
            const int pictureWidth,
            std::string& pictureByteString
    ) {
        returned_picture_size = pictureSize;
        returned_picture_height = pictureHeight;
        returned_picture_width = pictureWidth;
        picture_byte_string = pictureByteString;
    };

    bool pic_not_found_or_corrupt_bool = false;

    const auto pic_not_found_or_corrupt = [&]() {
        pic_not_found_or_corrupt_bool = true;
    };

    bool return_val = extractChatPicture(
            mongo_cpp_client,
            chat_message_pic.current_object_oid.to_string(),
            chat_room_id,
            generated_account_oid.to_string(),
            generated_message_uuid,
            pic_successfully_extracted,
            pic_not_found_or_corrupt
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(pic_not_found_or_corrupt_bool);

    EXPECT_EQ(returned_picture_size, -2);
    EXPECT_EQ(returned_picture_height, -2);
    EXPECT_EQ(returned_picture_width, -2);
    EXPECT_EQ(picture_byte_string, "");

    ChatRoomMessageDoc chat_room_message_doc(generated_message_uuid, chat_room_id);
    ASSERT_EQ(chat_room_message_doc.message_type, MessageSpecifics::MessageBodyCase::kPictureMessage);
    auto extracted_picture_message = std::static_pointer_cast<ChatRoomMessageDoc::PictureMessageSpecifics>(
            chat_room_message_doc.message_specifics_document);

    EXPECT_EQ(extracted_picture_message->picture_oid, "");
    EXPECT_EQ(extracted_picture_message->picture_image_height, 0);
    EXPECT_EQ(extracted_picture_message->picture_image_width, 0);

    ChatMessagePictureDoc after_chat_message_picture_doc(chat_message_picture_id);
    EXPECT_EQ(after_chat_message_picture_doc.current_object_oid.to_string(), "000000000000000000000000");

    DeletedChatMessagePicturesDoc extracted_deleted_chat_message_pictures_doc(
            chat_message_picture_doc.current_object_oid);

    DeletedChatMessagePicturesDoc generated_deleted_chat_message_pictures_doc(
            chat_message_picture_doc,
            extracted_deleted_chat_message_pictures_doc.timestamp_removed
    );

    EXPECT_EQ(extracted_deleted_chat_message_pictures_doc, generated_deleted_chat_message_pictures_doc);

}

TEST_F(UtilityChatFunctions, getUserAccountInfoForUserJoinedChatRoom_standardCase) {
    //NOTE: This is just a wrapper for a database call followed by a function call.
}

TEST_F(UtilityChatFunctions, getUserAccountInfoForUserJoinedChatRoom_userAccountNotFound) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account_doc(generated_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    auto delete_result = user_accounts_collection.delete_one(
            document{}
                    << "_id" << generated_account_oid
                    << finalize
    );

    EXPECT_EQ(delete_result->result().deleted_count(), 1);

    MemberSharedInfoMessage user_member_info;

    bool return_val = getUserAccountInfoForUserJoinedChatRoom(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            chat_room_id,
            generated_account_oid.to_string(),
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &user_member_info
    );

    EXPECT_TRUE(return_val);

    std::string thumbnail;
    long thumbnail_timestamp = 0;
    int thumbnail_size = 0;

    for (const auto& pic: generated_account_doc.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date picture_timestamp{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    picture_reference,
                    picture_timestamp
            );

            UserPictureDoc picture_doc(picture_reference);

            thumbnail = picture_doc.thumbnail_in_bytes;
            thumbnail_timestamp = create_chat_room_response.last_activity_time_timestamp();
            thumbnail_size = picture_doc.thumbnail_size_in_bytes;
            break;
        }
    }

    //All of this info should be set.
    EXPECT_EQ(user_member_info.account_thumbnail_size(), thumbnail_size);
    EXPECT_EQ(user_member_info.account_thumbnail_timestamp(), thumbnail_timestamp);
    EXPECT_EQ(user_member_info.account_thumbnail(), thumbnail);
    EXPECT_EQ(user_member_info.account_name(), generated_account_doc.first_name);
    EXPECT_EQ(user_member_info.account_oid(), generated_account_oid.to_string());
    EXPECT_NE(user_member_info.current_timestamp(), 0);

    //None of this info should be set.
    EXPECT_EQ(user_member_info.age(), 0);
    EXPECT_EQ(user_member_info.city_name(), "");
    EXPECT_EQ(user_member_info.gender(), "");
    EXPECT_EQ(user_member_info.bio(), "");
    EXPECT_TRUE(user_member_info.activities().empty());
    EXPECT_TRUE(user_member_info.picture().empty());
}

TEST_F(UtilityChatFunctions, isInvalidChatRoomId) {
    EXPECT_TRUE(isInvalidChatRoomId(""));
    EXPECT_FALSE(isInvalidChatRoomId("12345678"));
    EXPECT_TRUE(isInvalidChatRoomId(std::string(chat_room_values::CHAT_ROOM_ID_NUMBER_OF_DIGITS - 2, 'a')));
    EXPECT_TRUE(isInvalidChatRoomId("1234567A"));
    EXPECT_FALSE(isInvalidChatRoomId("1234567a"));
    EXPECT_TRUE(isInvalidChatRoomId("abcdefg*"));
    EXPECT_TRUE(isInvalidChatRoomId(bsoncxx::oid{}.to_string()));
}

TEST_F(LeaveChatRoomTesting, leaveChatRoom_noSession) {
    ReturnStatus return_status;
    std::chrono::milliseconds timestamp_stored;

    auto set_error_response = [&](const ReturnStatus& _return_status,
                                  const std::chrono::milliseconds& _timestamp_stored) {
        return_status = _return_status;
        timestamp_stored = _timestamp_stored;
    };

    bool return_val = leaveChatRoom(
            chat_room_db,
            accounts_db,
            user_accounts_collection,
            nullptr,
            user_account_builder.view(),
            create_chat_room_response.chat_room_id(),
            generated_account_oid,
            currentTimestamp,
            set_error_response
    );

    EXPECT_EQ(return_status, ReturnStatus::LG_ERROR);
    EXPECT_EQ(timestamp_stored, std::chrono::milliseconds{-1L});
    EXPECT_EQ(return_val, false);

    ChatRoomHeaderDoc extracted_chat_room_header(create_chat_room_response.chat_room_id());

    EXPECT_EQ(original_chat_room_header, extracted_chat_room_header);
}

TEST_F(LeaveChatRoomTesting, leaveChatRoom_singleUser) {

    bool return_val;

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session) {
        return_val = runFunction(session);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
        ASSERT_EQ(e.what(), "");
    }

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ +
                                                             create_chat_room_response.chat_room_id()];

    auto chat_message_doc = chat_room_collection.find_one(
            document{}
                    << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kDifferentUserLeftMessage
                    << finalize
    );

    ASSERT_TRUE(chat_message_doc);

    ChatRoomMessageDoc leave_chat_message;
    leave_chat_message.convertDocumentToClass(*chat_message_doc, create_chat_room_response.chat_room_id());

    checkLeaveChatRoomResult(
            currentTimestamp,
            original_chat_room_header,
            generated_account_oid,
            thumbnail_picture.current_object_oid.to_string(),
            thumbnail_picture.thumbnail_in_bytes,
            create_chat_room_response.chat_room_id(),
            return_status,
            timestamp_stored,
            leave_chat_message.shared_properties.timestamp.value,
            return_val
    );

}

TEST_F(LeaveChatRoomTesting, leaveChatRoom_adminUpdates) {

    bsoncxx::oid joined_generated_account_oid = insertRandomAccounts(1, 0);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;
    grpc_chat_commands::JoinChatRoomResponse join_chat_room_response;

    join_chat_room_request.set_chat_room_id(create_chat_room_response.chat_room_id());
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    UserAccountDoc joining_user_account(joined_generated_account_oid);

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            joined_generated_account_oid,
            joining_user_account.logged_in_token,
            joining_user_account.installation_ids.front()
    );

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    original_chat_room_header.getFromCollection(create_chat_room_response.chat_room_id());

    bool return_val;

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session) {
        return_val = runFunction(session);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
        ASSERT_EQ(e.what(), "");
    }

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ +
                                                             create_chat_room_response.chat_room_id()];

    auto chat_message_doc = chat_room_collection.find_one(
            document{}
                    << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kDifferentUserLeftMessage
                    << finalize
    );

    ASSERT_TRUE(chat_message_doc);

    ChatRoomMessageDoc leave_chat_message;
    leave_chat_message.convertDocumentToClass(*chat_message_doc, create_chat_room_response.chat_room_id());

    checkLeaveChatRoomResult(
            currentTimestamp,
            original_chat_room_header,
            generated_account_oid,
            thumbnail_picture.current_object_oid.to_string(),
            thumbnail_picture.thumbnail_in_bytes,
            create_chat_room_response.chat_room_id(),
            return_status,
            timestamp_stored,
            leave_chat_message.shared_properties.timestamp.value,
            return_val
    );

}

TEST_F(LeaveChatRoomTesting, leaveChatRoom_thumbnailUpdates) {

    setfields::SetPictureRequest request;
    setfields::SetFieldResponse response;

    setupUserLoginInfo(
            request.mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    std::string new_picture = "new_picture string";
    std::string new_thumbnail = "thumb string";

    request.set_file_in_bytes(new_picture);
    request.set_file_size((int) new_picture.size());
    request.set_thumbnail_in_bytes(new_thumbnail);
    request.set_thumbnail_size((int) new_thumbnail.size());
    request.set_picture_array_index(0);

    setPicture(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account.getFromCollection(generated_account_oid);

    bsoncxx::oid updated_picture_reference;
    bsoncxx::types::b_date updated_timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    //set index 0 above
    user_account.pictures.front().getPictureReference(
            updated_picture_reference,
            updated_timestamp_stored
    );

    bool return_val;

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session) {
        return_val = runFunction(session);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
        ASSERT_EQ(e.what(), "");
    }

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ +
                                                             create_chat_room_response.chat_room_id()];

    auto chat_message_doc = chat_room_collection.find_one(
            document{}
                    << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kDifferentUserLeftMessage
                    << finalize
    );

    ASSERT_TRUE(chat_message_doc);

    ChatRoomMessageDoc leave_chat_message;
    leave_chat_message.convertDocumentToClass(*chat_message_doc, create_chat_room_response.chat_room_id());

    checkLeaveChatRoomResult(
            currentTimestamp,
            original_chat_room_header,
            generated_account_oid,
            updated_picture_reference.to_string(),
            new_thumbnail,
            create_chat_room_response.chat_room_id(),
            return_status,
            timestamp_stored,
            leave_chat_message.shared_properties.timestamp.value,
            return_val
    );

}

TEST_F(LeaveChatRoomTesting, leaveChatRoomUnMatch) {

    bsoncxx::oid second_generated_account_oid = insertRandomAccounts(1, 0);

    original_chat_room_header.matching_oid_strings = std::make_unique<std::vector<std::string>>(
            std::vector<std::string>{generated_account_oid.to_string(), second_generated_account_oid.to_string()}
    );

    original_chat_room_header.setIntoCollection();

    bsoncxx::types::b_date matching_date = bsoncxx::types::b_date{
            currentTimestamp - std::chrono::milliseconds{10 * 1000}};

    user_account.other_accounts_matched_with.emplace_back(
            UserAccountDoc::OtherAccountMatchedWith(
                    second_generated_account_oid.to_string(),
                    matching_date
            )
    );

    user_account.setIntoCollection();

    UserAccountDoc second_user_account(second_generated_account_oid);

    second_user_account.other_accounts_matched_with.emplace_back(
            UserAccountDoc::OtherAccountMatchedWith(
                    generated_account_oid.to_string(),
                    matching_date
            )
    );

    second_user_account.setIntoCollection();

    bool return_val;

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session) {
        return_val = runFunction(session);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
        ASSERT_EQ(e.what(), "");
    }

    checkLeaveChatRoomResult(
            currentTimestamp,
            original_chat_room_header,
            generated_account_oid,
            thumbnail_picture.current_object_oid.to_string(),
            thumbnail_picture.thumbnail_in_bytes,
            create_chat_room_response.chat_room_id(),
            return_status,
            timestamp_stored,
            std::chrono::milliseconds{currentTimestamp},
            return_val
    );
}

TEST_F(UtilityChatFunctions, saveUserInfoToMemberSharedInfoMessage_noPictureInfo) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(generated_account_oid);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::options::find opts;

    opts.projection(buildUpdateSingleOtherUserProjectionDoc());

    auto find_result = user_accounts_collection.find_one(
            document{} << finalize,
            opts
    );

    ASSERT_TRUE(find_result);

    auto current_timestamp = getCurrentTimestamp();
    MemberSharedInfoMessage passed_user_info;

    bool return_val = saveUserInfoToMemberSharedInfoMessage(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            find_result->view(),
            generated_account_oid,
            &passed_user_info,
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
            current_timestamp
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;

    generated_user_info.set_current_timestamp(current_timestamp.count());
    generated_user_info.set_timestamp_other_user_info_updated(passed_user_info.timestamp_other_user_info_updated());
    generated_user_info.set_account_name(user_account_doc.first_name);
    generated_user_info.set_age(user_account_doc.age);
    generated_user_info.set_gender(user_account_doc.gender);
    generated_user_info.set_city_name(user_account_doc.city);
    generated_user_info.set_bio(user_account_doc.bio);
    generated_user_info.set_account_type(user_account_doc.account_type);
    generated_user_info.set_letsgo_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    bsoncxx::builder::stream::document generated_builder;
    user_account_doc.convertToDocument(generated_builder);

    auto match_categories_element = generated_builder.view()[user_account_keys::CATEGORIES];

    return_val = saveActivitiesToMemberSharedInfoMessage(
            generated_account_oid,
            match_categories_element.get_array().value,
            &generated_user_info,
            generated_builder.view());

    EXPECT_TRUE(return_val);

    compareEquivalentMessages<MemberSharedInfoMessage>(
            passed_user_info,
            generated_user_info
    );
}

TEST_F(UtilityChatFunctions, saveUserInfoToMemberSharedInfoMessage_requestOnlyThumbnail) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(generated_account_oid);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::options::find opts;

    opts.projection(buildUpdateSingleOtherUserProjectionDoc());

    auto find_result = user_accounts_collection.find_one(
            document{} << finalize,
            opts
    );

    ASSERT_TRUE(find_result);

    auto current_timestamp = getCurrentTimestamp();
    MemberSharedInfoMessage passed_user_info;

    bool return_val = saveUserInfoToMemberSharedInfoMessage(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            find_result->view(),
            generated_account_oid,
            &passed_user_info,
            HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
            current_timestamp
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;

    generated_user_info.set_current_timestamp(current_timestamp.count());
    generated_user_info.set_timestamp_other_user_info_updated(passed_user_info.timestamp_other_user_info_updated());
    generated_user_info.set_account_name(user_account_doc.first_name);

    bsoncxx::builder::stream::document generated_builder;
    user_account_doc.convertToDocument(generated_builder);

    std::string thumbnail;
    int returned_thumbnail_size = 0;
    int thumbnailIndex = -1;
    std::chrono::milliseconds thumbnail_timestamp;

    auto setThumbnailValue = [&](
            std::string& _thumbnail,
            int thumbnail_size,
            const std::string& _thumbnail_reference_oid [[maybe_unused]],
            const int index,
            const std::chrono::milliseconds& _thumbnail_timestamp
    ) {
        thumbnail = std::move(_thumbnail);
        returned_thumbnail_size = thumbnail_size;
        thumbnailIndex = index;
        thumbnail_timestamp = _thumbnail_timestamp;
    };

    return_val = extractThumbnailFromUserAccountDoc(
            accounts_db,
            generated_builder.view(),
            generated_account_oid,
            nullptr,
            setThumbnailValue
    );

    EXPECT_TRUE(return_val);

    generated_user_info.set_account_thumbnail(thumbnail);
    generated_user_info.set_account_thumbnail_size(returned_thumbnail_size);
    generated_user_info.set_account_thumbnail_index(thumbnailIndex);
    generated_user_info.set_account_thumbnail_timestamp(thumbnail_timestamp.count());

    generated_user_info.set_age(user_account_doc.age);
    generated_user_info.set_gender(user_account_doc.gender);
    generated_user_info.set_city_name(user_account_doc.city);
    generated_user_info.set_bio(user_account_doc.bio);

    generated_user_info.set_account_type(user_account_doc.account_type);
    generated_user_info.set_letsgo_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    auto match_categories_element = generated_builder.view()[user_account_keys::CATEGORIES];

    return_val = saveActivitiesToMemberSharedInfoMessage(
            generated_account_oid,
            match_categories_element.get_array().value,
            &generated_user_info,
            generated_builder.view());

    EXPECT_TRUE(return_val);

    compareEquivalentMessages<MemberSharedInfoMessage>(
            passed_user_info,
            generated_user_info
    );
}

TEST_F(UtilityChatFunctions, saveUserInfoToMemberSharedInfoMessage_requestAllPictureInfo) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(generated_account_oid);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    mongocxx::options::find opts;

    opts.projection(buildUpdateSingleOtherUserProjectionDoc());

    auto find_result = user_accounts_collection.find_one(
            document{} << finalize,
            opts
    );

    ASSERT_TRUE(find_result);

    auto current_timestamp = getCurrentTimestamp();
    MemberSharedInfoMessage passed_user_info;

    bool return_val = saveUserInfoToMemberSharedInfoMessage(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            find_result->view(),
            generated_account_oid,
            &passed_user_info,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            current_timestamp
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;

    generated_user_info.set_current_timestamp(current_timestamp.count());
    generated_user_info.set_timestamp_other_user_info_updated(passed_user_info.timestamp_other_user_info_updated());
    generated_user_info.set_account_name(user_account_doc.first_name);

    bsoncxx::builder::stream::document generated_builder;
    user_account_doc.convertToDocument(generated_builder);

    //set this to true if all picture info was requested for this user
    generated_user_info.set_pictures_checked_for_updates(true);

    //save member pictures
    return_val = savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures,
            user_accounts_collection,
            current_timestamp,
            generated_builder.view()[user_account_keys::PICTURES].get_array().value,
            generated_builder.view(),
            generated_account_oid,
            &generated_user_info
    );

    EXPECT_TRUE(return_val);

    generated_user_info.set_age(user_account_doc.age);
    generated_user_info.set_gender(user_account_doc.gender);
    generated_user_info.set_city_name(user_account_doc.city);
    generated_user_info.set_bio(user_account_doc.bio);

    generated_user_info.set_account_type(user_account_doc.account_type);
    generated_user_info.set_letsgo_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    auto match_categories_element = generated_builder.view()[user_account_keys::CATEGORIES];

    return_val = saveActivitiesToMemberSharedInfoMessage(
            generated_account_oid,
            match_categories_element.get_array().value,
            &generated_user_info,
            generated_builder.view());

    EXPECT_TRUE(return_val);

    compareEquivalentMessages<MemberSharedInfoMessage>(
            passed_user_info,
            generated_user_info
    );
}

TEST_F(UtilityChatFunctions, saveUserInfoToMemberSharedInfoMessage_requestEvent) {

    createAndStoreEventAdminAccount();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    const auto current_timestamp = getCurrentTimestamp();

    const bsoncxx::oid event_account_oid =
            bsoncxx::oid{
                    generateRandomEventAndEventInfo(
                            chat_room_admin_info,
                            TEMP_ADMIN_ACCOUNT_NAME,
                            current_timestamp
                    ).event_account_oid
            };

    UserAccountDoc user_account_doc(event_account_oid);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    mongocxx::options::find opts;

    opts.projection(buildUpdateSingleOtherUserProjectionDoc());

    auto find_result = user_accounts_collection.find_one(
            document{}
                    << "_id" << event_account_oid
                    << finalize,
            opts
    );

    ASSERT_TRUE(find_result);

    MemberSharedInfoMessage passed_user_info;

    bool return_val = saveUserInfoToMemberSharedInfoMessage(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            find_result->view(),
            event_account_oid,
            &passed_user_info,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            current_timestamp
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;

    generated_user_info.set_current_timestamp(current_timestamp.count());
    generated_user_info.set_timestamp_other_user_info_updated(passed_user_info.timestamp_other_user_info_updated());
    generated_user_info.set_account_name(user_account_doc.first_name);

    bsoncxx::builder::stream::document generated_builder;
    user_account_doc.convertToDocument(generated_builder);

    //set this to true if all picture info was requested for this user
    generated_user_info.set_pictures_checked_for_updates(true);

    //save member pictures
    return_val = savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures,
            user_accounts_collection,
            current_timestamp,
            generated_builder.view()[user_account_keys::PICTURES].get_array().value,
            generated_builder.view(),
            event_account_oid,
            &generated_user_info
    );

    EXPECT_TRUE(return_val);

    generated_user_info.set_age(user_account_doc.age);
    generated_user_info.set_gender(user_account_doc.gender);
    generated_user_info.set_city_name(user_account_doc.city);
    generated_user_info.set_bio(user_account_doc.bio);

    generated_user_info.set_account_type(user_account_doc.account_type);
    generated_user_info.set_letsgo_event_status(LetsGoEventStatus::ONGOING);

    generated_user_info.set_created_by(general_values::APP_NAME);
    generated_user_info.set_event_title(user_account_doc.event_values->event_title);

    auto match_categories_element = generated_builder.view()[user_account_keys::CATEGORIES];

    return_val = saveActivitiesToMemberSharedInfoMessage(
            event_account_oid,
            match_categories_element.get_array().value,
            &generated_user_info,
            generated_builder.view());

    EXPECT_TRUE(return_val);

    compareEquivalentMessages<MemberSharedInfoMessage>(
            passed_user_info,
            generated_user_info
    );
}

TEST_F(UtilityChatFunctions, saveUserInfoToMemberSharedInfoMessage_maxSizeAccount) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(generated_account_oid);

    generateRandomLargestPossibleUserAccount(user_account_doc);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    mongocxx::options::find opts;

    opts.projection(buildUpdateSingleOtherUserProjectionDoc());

    auto find_result = user_accounts_collection.find_one(
            document{} << finalize,
            opts
    );

    ASSERT_TRUE(find_result);

    auto current_timestamp = getCurrentTimestamp();

    //This type is the largest type to use it and therefore will take up the most space.
    findmatches::SingleMatchMessage single_match_message;

    //Any value for these parameters is fine as long as it isn't the default. Protobuf does not
    // encode the default value to the byte string. Need to generate the largest possible message.
    single_match_message.set_point_value(12345);
    single_match_message.set_expiration_time(123);
    single_match_message.set_other_user_match(true);
    single_match_message.set_swipes_remaining(123);
    single_match_message.set_swipes_time_before_reset(123);
    single_match_message.set_timestamp(123);

    bool return_val = saveUserInfoToMemberSharedInfoMessage(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            find_result->view(),
            generated_account_oid,
            single_match_message.mutable_member_info(),
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            current_timestamp
    );

    EXPECT_TRUE(return_val);

    std::cout << "grpc_values::MAX_SEND_MESSAGE_LENGTH   : " << grpc_values::MAX_SEND_MESSAGE_LENGTH << '\n';
    std::cout << "single_match_message                   : " << single_match_message.ByteSizeLong() << '\n';

    //Set to non-default values (same reasons as single_match_message).
    single_match_message.mutable_member_info()->set_account_thumbnail_index(1);
    single_match_message.mutable_member_info()->set_distance(123);

    EXPECT_LT(single_match_message.ByteSizeLong(), grpc_values::MAX_SEND_MESSAGE_LENGTH);
}

TEST_F(UtilityChatFunctions, sendNewChatRoomAndMessages) {

    bsoncxx::oid requesting_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid first_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc requesting_user_account_doc(requesting_account_oid);
    UserAccountDoc first_user_account_doc(first_account_oid);
    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    auto current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    //add some messages
    generateRandomTextMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID()
    );

    generateRandomPictureMessage(
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID()
    );

    generateRandomLocationMessage(
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID()
    );

    bsoncxx::builder::stream::document user_account_doc_builder;
    requesting_user_account_doc.convertToDocument(user_account_doc_builder);

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    bool return_val = sendNewChatRoomAndMessages(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            chat_room_collection,
            user_account_doc_builder.view(),
            chat_room_id,
            current_timestamp,
            current_timestamp,
            requesting_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            false,
            false
    );

    EXPECT_TRUE(return_val);

    ChatRoomHeaderDoc chat_room_header_doc(chat_room_id);

    //make sure chat room header was found
    ASSERT_NE(chat_room_header_doc.chat_room_password, "");

    storeAndSendMessagesToClient.finalCleanup();

    UserAccountDoc after_requesting_user_account_doc(requesting_account_oid);

    ASSERT_EQ(reply_vector.size(), 1);
    ASSERT_EQ(reply_vector[0]->mutable_return_new_chat_message()->messages_list_size(), 10);

    //Messages
    //THIS_USER_JOINED_CHAT_ROOM_START_MESSAGE
    //THIS_USER_JOINED_CHAT_ROOM_MEMBER
    //THIS_USER_JOINED_CHAT_ROOM_MEMBER
    //ChatRoomCapMessage
    //DifferentUserJoinedChatRoomChatMessage (first user when creating chat room)
    //DifferentUserJoinedChatRoomChatMessage (second user when joining the chat room)
    //TextChatMessage
    //PictureChatMessage
    //LocationChatMessage
    //THIS_USER_JOINED_CHAT_ROOM_FINISHED

    ChatMessageToClient response;
    response.set_sent_by_account_id(requesting_account_oid.to_string());

    auto chat_message = response.mutable_message();
    auto this_user_joined_chat_room_start = chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_start_message();
    chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);
    chat_message->mutable_standard_message_info()->set_internal_force_send_message_to_current_user(true);

    auto chat_room_info = this_user_joined_chat_room_start->mutable_chat_room_info();
    chat_room_info->set_chat_room_id(chat_room_id);
    chat_room_info->set_chat_room_last_observed_time(current_timestamp.count());
    chat_room_info->set_chat_room_name(chat_room_header_doc.chat_room_name);
    chat_room_info->set_chat_room_password(chat_room_header_doc.chat_room_password);
    chat_room_info->set_chat_room_last_activity_time(chat_room_header_doc.chat_room_last_active_time.value.count());
    chat_room_info->set_longitude_pinned_location(chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE);
    chat_room_info->set_latitude_pinned_location(chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE);

    compareEquivalentMessages<ChatMessageToClient>(
            reply_vector[0]->mutable_return_new_chat_message()->messages_list()[0],
            response
    );

    auto this_user_joined_chat_room_member = chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_member_message();
    chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);

    int index = 1;
    for (const auto& account: chat_room_header_doc.accounts_in_chat_room) {
        auto chat_room_member_info = this_user_joined_chat_room_member->mutable_member_info();
        chat_room_member_info->Clear();
        auto user_info = chat_room_member_info->mutable_user_info();

        user_info->set_account_oid(account.account_oid.to_string());
        chat_room_member_info->set_account_state(account.state_in_chat_room);
        chat_room_member_info->set_account_last_activity_time(account.last_activity_time.value.count());

        UserAccountDoc member_user_account_doc(account.account_oid);

        bsoncxx::builder::stream::document builder;
        member_user_account_doc.convertToDocument(builder);

        return_val = saveUserInfoToMemberSharedInfoMessage(
                mongo_cpp_client,
                accounts_db,
                user_accounts_collection,
                builder.view(),
                account.account_oid,
                user_info,
                HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
                current_timestamp
        );

        compareEquivalentMessages(
                reply_vector[0]->mutable_return_new_chat_message()->messages_list()[index],
                response
        );

        index++;
    }

    chat_message->mutable_standard_message_info()->set_do_not_update_user_state(false);
    chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_finished_message()->set_match_made_chat_room_oid(
            "");
    chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);

    compareEquivalentMessages(
            reply_vector[0]->mutable_return_new_chat_message()->messages_list()[9],
            response
            );
}

TEST_F(UtilityChatFunctions, sendNewChatRoomAndMessages_userNotInsideChatRoom) {

    bsoncxx::oid requesting_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid first_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc requesting_user_account_doc(requesting_account_oid);
    UserAccountDoc first_user_account_doc(first_account_oid);
    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    //sleep to guarantee time between timestamps in createChatRoom() and joinChatRoom()
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    //sleep to guarantee time between timestamps in joinChatRoom() and leaveChatRoom()
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
    grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

    setupUserLoginInfo(
            leave_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    leave_chat_room_request.set_chat_room_id(chat_room_id);

    leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

    ASSERT_EQ(leave_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    auto current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    bsoncxx::builder::stream::document user_account_doc_builder;
    requesting_user_account_doc.convertToDocument(user_account_doc_builder);

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    bool return_val = sendNewChatRoomAndMessages(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            chat_room_collection,
            user_account_doc_builder.view(),
            chat_room_id,
            current_timestamp,
            current_timestamp,
            requesting_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            false,
            false
    );

    EXPECT_TRUE(return_val);

    ChatRoomHeaderDoc chat_room_header_doc(chat_room_id);

    //make sure chat room header was found
    ASSERT_NE(chat_room_header_doc.chat_room_password, "");

    storeAndSendMessagesToClient.finalCleanup();

    UserAccountDoc after_requesting_user_account_doc(requesting_account_oid);

    ASSERT_EQ(reply_vector.size(), 1);
    ASSERT_EQ(reply_vector[0]->mutable_return_new_chat_message()->messages_list_size(), 8);

    //Messages
    //THIS_USER_JOINED_CHAT_ROOM_START_MESSAGE
    //THIS_USER_JOINED_CHAT_ROOM_MEMBER
    //THIS_USER_JOINED_CHAT_ROOM_MEMBER
    //ChatRoomCapMessage
    //DifferentUserJoinedChatRoomChatMessage (first user when creating chat room)
    //DifferentUserJoinedChatRoomChatMessage (second user when joining the chat room)
    //DifferentUserJoinedLeftRoomChatMessage (second user when leaving the chat room)
    //THIS_USER_JOINED_CHAT_ROOM_FINISHED

    ChatMessageToClient response;
    response.set_sent_by_account_id(requesting_account_oid.to_string());

    auto chat_message = response.mutable_message();
    auto this_user_joined_chat_room_start = chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_start_message();
    chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);
    chat_message->mutable_standard_message_info()->set_internal_force_send_message_to_current_user(true);

    auto chat_room_info = this_user_joined_chat_room_start->mutable_chat_room_info();
    chat_room_info->set_chat_room_id(chat_room_id);
    chat_room_info->set_chat_room_last_observed_time(current_timestamp.count());
    chat_room_info->set_chat_room_name(chat_room_header_doc.chat_room_name);
    chat_room_info->set_chat_room_password(chat_room_header_doc.chat_room_password);
    chat_room_info->set_chat_room_last_activity_time(chat_room_header_doc.chat_room_last_active_time.value.count());
    chat_room_info->set_longitude_pinned_location(chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE);
    chat_room_info->set_latitude_pinned_location(chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE);

    compareEquivalentMessages<ChatMessageToClient>(
            reply_vector[0]->mutable_return_new_chat_message()->messages_list()[0],
            response
    );

    auto this_user_joined_chat_room_member = chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_member_message();
    chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);

    int index = 1;
    for (const auto& account: chat_room_header_doc.accounts_in_chat_room) {
        auto chat_room_member_info = this_user_joined_chat_room_member->mutable_member_info();
        chat_room_member_info->Clear();

        auto user_info = chat_room_member_info->mutable_user_info();

        user_info->set_account_oid(account.account_oid.to_string());
        chat_room_member_info->set_account_state(account.state_in_chat_room);

        if (account.state_in_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
            || account.state_in_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN) {

            chat_room_member_info->set_account_last_activity_time(account.last_activity_time.value.count());

            UserAccountDoc member_user_account_doc(account.account_oid);

            bsoncxx::builder::stream::document builder;
            member_user_account_doc.convertToDocument(builder);

            return_val = saveUserInfoToMemberSharedInfoMessage(
                    mongo_cpp_client,
                    accounts_db,
                    user_accounts_collection,
                    builder.view(),
                    account.account_oid,
                    user_info,
                    HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
                    current_timestamp
            );
        } else { //not in chat room

            bsoncxx::builder::stream::document header_account_doc;
            account.convertToDocument(header_account_doc);

            user_info->set_account_name(account.first_name);
            user_info->set_account_type(UserAccountType::USER_ACCOUNT_TYPE);

            return_val = extractThumbnailFromHeaderAccountDocument(
                    mongo_cpp_client,
                    accounts_db,
                    chat_room_collection,
                    header_account_doc.view(),
                    current_timestamp,
                    user_info,
                    account.account_oid.to_string(),
                    account.thumbnail_size,
                    account.thumbnail_timestamp.value
            );
        }

        EXPECT_TRUE(return_val);

        bool equivalent_messages = google::protobuf::util::MessageDifferencer::Equivalent(
                reply_vector[0]->mutable_return_new_chat_message()->messages_list()[index],
                response
        );

        if (!equivalent_messages) {
            std::cout << "reply_vector\n" << reply_vector[0]->mutable_return_new_chat_message()->messages_list()[index].DebugString() << '\n';
            std::cout << "response\n" << response.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent_messages);

        index++;
    }

    chat_message->mutable_standard_message_info()->set_do_not_update_user_state(false);
    chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_finished_message()->set_match_made_chat_room_oid(
            "");
    chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    reply_vector[0]->mutable_return_new_chat_message()->messages_list()[7],
                    response
            )
    );
}

TEST_F(UtilityChatFunctions, sendNewChatRoomAndMessages_chatRoomNotFound) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    const std::string chat_room_id = generateRandomChatRoomId();
    auto current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account_doc(generated_account_oid);

    user_account_doc.chat_rooms.emplace_back(
            chat_room_id,
            bsoncxx::types::b_date{current_timestamp}
    );

    user_account_doc.setIntoCollection();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    bsoncxx::builder::stream::document dummy_doc;
    dummy_doc
            << "key" << "dummy_user_account_document";

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    bool return_val = sendNewChatRoomAndMessages(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            chat_room_collection,
            dummy_doc.view(),
            chat_room_id,
            current_timestamp,
            current_timestamp,
            generated_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            false,
            false
    );

    EXPECT_TRUE(return_val);

    UserAccountDoc extracted_user_account_doc(generated_account_oid);

    user_account_doc.chat_rooms.pop_back();

    EXPECT_EQ(extracted_user_account_doc, user_account_doc);

}

TEST_F(UtilityChatFunctions, sendNewChatRoomAndMessages_matchingOIDPresent) {

    bsoncxx::oid first_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid second_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc first_user_account_doc(first_account_oid);
    UserAccountDoc second_user_account_doc(second_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            first_account_oid,
            first_user_account_doc.logged_in_token,
            first_user_account_doc.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            second_account_oid,
            second_user_account_doc.logged_in_token,
            second_user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(create_chat_room_response.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    auto current_timestamp = getCurrentTimestamp();

    first_user_account_doc.other_accounts_matched_with.emplace_back(
            second_account_oid.to_string(),
            bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{1000}}
    );
    first_user_account_doc.setIntoCollection();

    second_user_account_doc.other_accounts_matched_with.emplace_back(
            first_account_oid.to_string(),
            bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{1000}}
    );
    second_user_account_doc.setIntoCollection();

    ChatRoomHeaderDoc chat_room_header_doc(chat_room_id);

    ASSERT_NE(chat_room_header_doc.chat_room_password, "");
    chat_room_header_doc.matching_oid_strings = std::make_unique<std::vector<std::string>>(
            std::vector<std::string>{
                    second_account_oid.to_string(),
                    first_account_oid.to_string()
            }
    );
    chat_room_header_doc.setIntoCollection();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    bsoncxx::builder::stream::document user_account_doc_builder;
    first_user_account_doc.convertToDocument(user_account_doc_builder);

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeAndSendMessagesToClient(
            reply_vector);

    bool return_val = sendNewChatRoomAndMessages(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            chat_room_collection,
            user_account_doc_builder.view(),
            chat_room_id,
            current_timestamp,
            current_timestamp,
            first_account_oid.to_string(),
            &storeAndSendMessagesToClient,
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
            AmountOfMessage::COMPLETE_MESSAGE_INFO,
            false,
            false
    );

    EXPECT_TRUE(return_val);

    storeAndSendMessagesToClient.finalCleanup();

    UserAccountDoc after_requesting_user_account_doc(first_account_oid);

    EXPECT_EQ(first_user_account_doc, after_requesting_user_account_doc);

    ASSERT_EQ(reply_vector.size(), 1);
    ASSERT_EQ(reply_vector[0]->mutable_return_new_chat_message()->messages_list_size(), 6);

    //Messages
    //THIS_USER_JOINED_CHAT_ROOM_START_MESSAGE
    //THIS_USER_JOINED_CHAT_ROOM_MEMBER
    //ChatRoomCapMessage
    //DifferentUserJoinedChatRoomChatMessage (first user when creating chat room)
    //DifferentUserJoinedChatRoomChatMessage (second user when joining the chat room)
    //THIS_USER_JOINED_CHAT_ROOM_FINISHED

    ChatMessageToClient response;
    response.set_sent_by_account_id(first_account_oid.to_string());

    auto chat_message = response.mutable_message();
    auto this_user_joined_chat_room_start = chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_start_message();
    chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);
    chat_message->mutable_standard_message_info()->set_internal_force_send_message_to_current_user(true);

    auto chat_room_info = this_user_joined_chat_room_start->mutable_chat_room_info();
    chat_room_info->set_chat_room_id(chat_room_id);
    chat_room_info->set_chat_room_last_observed_time(current_timestamp.count());
    chat_room_info->set_chat_room_name(chat_room_header_doc.chat_room_name);
    chat_room_info->set_chat_room_password(chat_room_header_doc.chat_room_password);
    chat_room_info->set_chat_room_last_activity_time(chat_room_header_doc.chat_room_last_active_time.value.count());
    chat_room_info->set_match_made_chat_room_oid(second_account_oid.to_string());
    chat_room_info->set_longitude_pinned_location(chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE);
    chat_room_info->set_latitude_pinned_location(chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE);

    for (const auto& account: chat_room_header_doc.accounts_in_chat_room) {
        if (account.account_oid == first_account_oid) {
            chat_room_info->set_account_state(account.state_in_chat_room);
            chat_room_info->set_user_last_activity_time(account.last_activity_time.value.count());
            chat_room_info->set_time_joined(account.times_joined_left.back().value.count());
            break;
        }
    }

    compareEquivalentMessages<ChatMessageToClient>(
            reply_vector[0]->mutable_return_new_chat_message()->messages_list()[0],
            response
    );

    auto this_user_joined_chat_room_member = chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_member_message();
    chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);

    int index = 1;
    for (const auto& account: chat_room_header_doc.accounts_in_chat_room) {
        if (account.account_oid != first_account_oid) {
            auto chat_room_member_info = this_user_joined_chat_room_member->mutable_member_info();
            chat_room_member_info->Clear();
            auto user_info = chat_room_member_info->mutable_user_info();

            user_info->set_account_oid(account.account_oid.to_string());
            chat_room_member_info->set_account_state(account.state_in_chat_room);
            chat_room_member_info->set_account_last_activity_time(account.last_activity_time.value.count());

            UserAccountDoc member_user_account_doc(account.account_oid);

            bsoncxx::builder::stream::document builder;
            member_user_account_doc.convertToDocument(builder);

            return_val = saveUserInfoToMemberSharedInfoMessage(
                    mongo_cpp_client,
                    accounts_db,
                    user_accounts_collection,
                    builder.view(),
                    account.account_oid,
                    user_info,
                    HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
                    current_timestamp
            );

            EXPECT_TRUE(
                    google::protobuf::util::MessageDifferencer::Equivalent(
                            reply_vector[0]->mutable_return_new_chat_message()->messages_list()[index],
                            response
                    )
            );
            index++;
        }
    }

    chat_message->mutable_standard_message_info()->set_do_not_update_user_state(false);
    chat_message->mutable_message_specifics()->mutable_this_user_joined_chat_room_finished_message()->set_match_made_chat_room_oid(
            second_account_oid.to_string());
    chat_message->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    reply_vector[0]->mutable_return_new_chat_message()->messages_list()[5],
                    response
            )
    );
}

TEST_F(UtilityChatFunctions, sendNewChatRoomAndMessages_eventChatRoom_validEvent) {

    const auto current_timestamp = getCurrentTimestamp();

    bsoncxx::oid requesting_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid first_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc requesting_user_account_doc(requesting_account_oid);
    UserAccountDoc first_user_account_doc(first_account_oid);

    createAndStoreEventAdminAccount();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    EventRequestMessage event_info = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    auto event_created_return_values = generateRandomEvent(
            chat_room_admin_info,
            TEMP_ADMIN_ACCOUNT_NAME,
            current_timestamp,
            event_info
    );

    setupForEventChatRoom(
            current_timestamp,
            requesting_account_oid,
            first_account_oid,
            requesting_user_account_doc,
            first_user_account_doc,
            event_info,
            event_created_return_values,
            false
    );
}

TEST_F(UtilityChatFunctions, sendNewChatRoomAndMessages_eventChatRoom_canceledEvent) {

    const auto current_timestamp = getCurrentTimestamp();

    bsoncxx::oid requesting_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid first_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc requesting_user_account_doc(requesting_account_oid);
    UserAccountDoc first_user_account_doc(first_account_oid);

    createAndStoreEventAdminAccount();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    EventRequestMessage event_info = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    auto event_created_return_values = generateRandomEvent(
            chat_room_admin_info,
            TEMP_ADMIN_ACCOUNT_NAME,
            current_timestamp,
            event_info
    );

    createTempAdminAccount(AdminLevelEnum::PRIMARY_DEVELOPER);

    user_event_commands::CancelEventRequest request;
    user_event_commands::CancelEventResponse response;

    setupAdminLoginInfo(
            request.mutable_login_info(),
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    request.set_event_oid(event_created_return_values.event_account_oid);

    cancelEvent(&request, &response);

    ASSERT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    setupForEventChatRoom(
            current_timestamp,
            requesting_account_oid,
            first_account_oid,
            requesting_user_account_doc,
            first_user_account_doc,
            event_info,
            event_created_return_values,
            true,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO
    );
}

TEST_F(UtilityChatFunctions, sendNewChatRoomAndMessages_eventChatRoom_expiredEvent) {

    const auto current_timestamp = getCurrentTimestamp();

    bsoncxx::oid requesting_account_oid = insertRandomAccounts(1, 0);
    bsoncxx::oid first_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc requesting_user_account_doc(requesting_account_oid);
    UserAccountDoc first_user_account_doc(first_account_oid);

    createAndStoreEventAdminAccount();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    EventRequestMessage event_info = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    auto event_created_return_values = generateRandomEvent(
            chat_room_admin_info,
            TEMP_ADMIN_ACCOUNT_NAME,
            current_timestamp,
            event_info
    );

    UserAccountDoc event_account(bsoncxx::oid{event_created_return_values.event_account_oid});

    //Set event to expired
    event_account.event_expiration_time = bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{5}};
    event_account.matching_activated = false;
    event_account.setIntoCollection();

    setupForEventChatRoom(
            current_timestamp,
            requesting_account_oid,
            first_account_oid,
            requesting_user_account_doc,
            first_user_account_doc,
            event_info,
            event_created_return_values,
            true,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO
    );
}

TEST_F(ExtractThumbnailFromHeaderAccountDocumentTests, thumbnailSizeZero) {

    ChatRoomHeaderDoc::AccountsInChatRoom account_in_chat_room_header{
            member_account_oid,
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            gen_random_alpha_numeric_string(rand() % 10 + 10),
            bsoncxx::oid{}.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{123}},
            0,
            bsoncxx::types::b_date{std::chrono::milliseconds{456}},
            std::vector<bsoncxx::types::b_date>{}
    };

    bsoncxx::builder::stream::document user_from_chat_room_header_doc_view;
    account_in_chat_room_header.convertToDocument(user_from_chat_room_header_doc_view);

    MemberSharedInfoMessage passed_user_info;

    bool return_val = extractThumbnailFromHeaderAccountDocument(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_from_chat_room_header_doc_view.view(),
            current_timestamp,
            &passed_user_info,
            member_account_oid.to_string(),
            account_in_chat_room_header.thumbnail_size,
            account_in_chat_room_header.thumbnail_timestamp.value
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;
    DeletedThumbnailInfo::saveDeletedThumbnailInfo(&generated_user_info);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_user_info,
                    passed_user_info
            )
    );

}

TEST_F(ExtractThumbnailFromHeaderAccountDocumentTests, thumbnailReferenceEmpty) {

    ChatRoomHeaderDoc::AccountsInChatRoom account_in_chat_room_header{
            member_account_oid,
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            gen_random_alpha_numeric_string(rand() % 10 + 10),
            "",
            bsoncxx::types::b_date{std::chrono::milliseconds{123}},
            123,
            bsoncxx::types::b_date{std::chrono::milliseconds{456}},
            std::vector<bsoncxx::types::b_date>{}
    };

    bsoncxx::builder::stream::document user_from_chat_room_header_doc_view;
    account_in_chat_room_header.convertToDocument(user_from_chat_room_header_doc_view);

    MemberSharedInfoMessage passed_user_info;

    bool return_val = extractThumbnailFromHeaderAccountDocument(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_from_chat_room_header_doc_view.view(),
            current_timestamp,
            &passed_user_info,
            member_account_oid.to_string(),
            account_in_chat_room_header.thumbnail_size,
            account_in_chat_room_header.thumbnail_timestamp.value
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;
    DeletedThumbnailInfo::saveDeletedThumbnailInfo(&generated_user_info);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_user_info,
                    passed_user_info
            )
    );
}

TEST_F(ExtractThumbnailFromHeaderAccountDocumentTests, thumbnailFound_inUserPicturesCollection) {

    UserPictureDoc user_picture;

    user_picture.user_account_reference = member_account_oid;
    user_picture.thumbnail_references.emplace_back(chat_room_id);
    user_picture.timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{123}};
    user_picture.picture_index = 1;
    user_picture.thumbnail_in_bytes = gen_random_alpha_numeric_string(rand() % 100 + 50);
    user_picture.thumbnail_size_in_bytes = (int) user_picture.thumbnail_in_bytes.size();
    user_picture.picture_in_bytes = gen_random_alpha_numeric_string(rand() % 200 + 150);
    user_picture.picture_size_in_bytes = (int) user_picture.picture_in_bytes.size();

    user_picture.setIntoCollection();

    EXPECT_NE(user_picture.current_object_oid.to_string(), "000000000000000000000000");

    ChatRoomHeaderDoc::AccountsInChatRoom account_in_chat_room_header{
            member_account_oid,
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            gen_random_alpha_numeric_string(rand() % 10 + 10),
            user_picture.current_object_oid.to_string(),
            user_picture.timestamp_stored,
            user_picture.thumbnail_size_in_bytes,
            bsoncxx::types::b_date{std::chrono::milliseconds{456}},
            std::vector<bsoncxx::types::b_date>{}
    };

    bsoncxx::builder::stream::document user_from_chat_room_header_doc_view;
    account_in_chat_room_header.convertToDocument(user_from_chat_room_header_doc_view);

    MemberSharedInfoMessage passed_user_info;

    bool return_val = extractThumbnailFromHeaderAccountDocument(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_from_chat_room_header_doc_view.view(),
            current_timestamp,
            &passed_user_info,
            member_account_oid.to_string(),
            account_in_chat_room_header.thumbnail_size,
            account_in_chat_room_header.thumbnail_timestamp.value
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;
    generated_user_info.set_account_thumbnail_size(user_picture.thumbnail_size_in_bytes);
    generated_user_info.set_account_thumbnail_timestamp(account_in_chat_room_header.thumbnail_timestamp.value.count());
    generated_user_info.set_account_thumbnail(user_picture.thumbnail_in_bytes);
    generated_user_info.set_account_thumbnail_index(0);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_user_info,
                    passed_user_info
            )
    );
}

TEST_F(ExtractThumbnailFromHeaderAccountDocumentTests,
       thumbnailFound_inDeletedUserPicturesCollection_noReferencesRemoved) {

    UserPictureDoc user_picture;

    user_picture.current_object_oid = bsoncxx::oid{};
    user_picture.user_account_reference = member_account_oid;
    user_picture.thumbnail_references.emplace_back(chat_room_id);
    user_picture.timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{123}};
    user_picture.picture_index = 1;
    user_picture.thumbnail_in_bytes = gen_random_alpha_numeric_string(rand() % 100 + 50);
    user_picture.thumbnail_size_in_bytes = (int) user_picture.thumbnail_in_bytes.size();
    user_picture.picture_in_bytes = gen_random_alpha_numeric_string(rand() % 200 + 150);
    user_picture.picture_size_in_bytes = (int) user_picture.picture_in_bytes.size();

    DeletedUserPictureDoc deleted_user_picture{
            user_picture,
            nullptr,
            bsoncxx::types::b_date{std::chrono::milliseconds{222}},
            ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
            nullptr
    };

    deleted_user_picture.setIntoCollection();

    ChatRoomHeaderDoc::AccountsInChatRoom account_in_chat_room_header{
            member_account_oid,
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            gen_random_alpha_numeric_string(rand() % 10 + 10),
            user_picture.current_object_oid.to_string(),
            user_picture.timestamp_stored,
            user_picture.thumbnail_size_in_bytes,
            bsoncxx::types::b_date{std::chrono::milliseconds{456}},
            std::vector<bsoncxx::types::b_date>{}
    };

    bsoncxx::builder::stream::document user_from_chat_room_header_doc_view;
    account_in_chat_room_header.convertToDocument(user_from_chat_room_header_doc_view);

    MemberSharedInfoMessage passed_user_info;

    bool return_val = extractThumbnailFromHeaderAccountDocument(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_from_chat_room_header_doc_view.view(),
            current_timestamp,
            &passed_user_info,
            member_account_oid.to_string(),
            account_in_chat_room_header.thumbnail_size,
            account_in_chat_room_header.thumbnail_timestamp.value
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;
    generated_user_info.set_account_thumbnail_size(user_picture.thumbnail_size_in_bytes);
    generated_user_info.set_account_thumbnail_timestamp(account_in_chat_room_header.thumbnail_timestamp.value.count());
    generated_user_info.set_account_thumbnail(user_picture.thumbnail_in_bytes);
    generated_user_info.set_account_thumbnail_index(0);

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            generated_user_info,
            passed_user_info
    );

    if (!equivalent) {
        std::cout << "generated_user_info: " << generated_user_info.DebugString() << '\n';
        std::cout << "passed_user_info: " << passed_user_info.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);

}

TEST_F(ExtractThumbnailFromHeaderAccountDocumentTests,
       thumbnailFound_inDeletedUserPicturesCollection_referencesRemoved) {

    UserPictureDoc user_picture;

    user_picture.current_object_oid = bsoncxx::oid{};
    user_picture.user_account_reference = member_account_oid;
    user_picture.thumbnail_references.emplace_back(chat_room_id);
    user_picture.timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{123}};
    user_picture.picture_index = 1;
    user_picture.thumbnail_in_bytes = gen_random_alpha_numeric_string(rand() % 100 + 50);
    user_picture.thumbnail_size_in_bytes = (int) user_picture.thumbnail_in_bytes.size();
    user_picture.picture_in_bytes = gen_random_alpha_numeric_string(rand() % 200 + 150);
    user_picture.picture_size_in_bytes = (int) user_picture.picture_in_bytes.size();

    DeletedUserPictureDoc deleted_user_picture{
            user_picture,
            std::make_unique<bool>(true),
            bsoncxx::types::b_date{std::chrono::milliseconds{222}},
            ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
            nullptr
    };

    deleted_user_picture.setIntoCollection();

    ChatRoomHeaderDoc::AccountsInChatRoom account_in_chat_room_header{
            member_account_oid,
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            gen_random_alpha_numeric_string(rand() % 10 + 10),
            user_picture.current_object_oid.to_string(),
            user_picture.timestamp_stored,
            user_picture.thumbnail_size_in_bytes,
            bsoncxx::types::b_date{std::chrono::milliseconds{456}},
            std::vector<bsoncxx::types::b_date>{}
    };

    bsoncxx::builder::stream::document user_from_chat_room_header_doc_view;
    account_in_chat_room_header.convertToDocument(user_from_chat_room_header_doc_view);

    MemberSharedInfoMessage passed_user_info;

    bool return_val = extractThumbnailFromHeaderAccountDocument(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_from_chat_room_header_doc_view.view(),
            current_timestamp,
            &passed_user_info,
            member_account_oid.to_string(),
            account_in_chat_room_header.thumbnail_size,
            account_in_chat_room_header.thumbnail_timestamp.value
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;
    DeletedThumbnailInfo::saveDeletedThumbnailInfo(&generated_user_info);

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            generated_user_info,
            passed_user_info
    );

    if (!equivalent) {
        std::cout << "generated_user_info: " << generated_user_info.DebugString() << '\n';
        std::cout << "passed_user_info: " << passed_user_info.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}

TEST_F(ExtractThumbnailFromHeaderAccountDocumentTests, thumbnailCorrupted) {

    UserPictureDoc user_picture;

    user_picture.user_account_reference = member_account_oid;
    user_picture.thumbnail_references.emplace_back(chat_room_id);
    user_picture.timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{123}};
    user_picture.picture_index = 1;
    user_picture.thumbnail_in_bytes = gen_random_alpha_numeric_string(rand() % 100 + 50);
    user_picture.thumbnail_size_in_bytes = (int) user_picture.thumbnail_in_bytes.size() + 1; //corrupt
    user_picture.picture_in_bytes = gen_random_alpha_numeric_string(rand() % 200 + 150);
    user_picture.picture_size_in_bytes = (int) user_picture.picture_in_bytes.size();

    user_picture.setIntoCollection();

    EXPECT_NE(user_picture.current_object_oid.to_string(), "000000000000000000000000");

    ChatRoomHeaderDoc chat_room_header;

    chat_room_header.chat_room_id = chat_room_id;
    chat_room_header.chat_room_name = "name";
    chat_room_header.chat_room_password = "password";
    chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{std::chrono::milliseconds{456}};
    chat_room_header.matching_oid_strings = nullptr;
    chat_room_header.accounts_in_chat_room.emplace_back(
            member_account_oid,
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            gen_random_alpha_numeric_string(rand() % 10 + 10),
            user_picture.current_object_oid.to_string(),
            user_picture.timestamp_stored,
            user_picture.thumbnail_size_in_bytes,
            chat_room_header.chat_room_last_active_time,
            std::vector<bsoncxx::types::b_date>{chat_room_header.chat_room_last_active_time}
    );
    chat_room_header.shared_properties.timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{455}};

    const int account_index = 0;

    bsoncxx::builder::stream::document user_from_chat_room_header_doc_view;
    chat_room_header.accounts_in_chat_room[account_index].convertToDocument(user_from_chat_room_header_doc_view);

    chat_room_header.setIntoCollection();

    MemberSharedInfoMessage passed_user_info;

    bool return_val = extractThumbnailFromHeaderAccountDocument(
            mongo_cpp_client,
            accounts_db,
            chat_room_collection,
            user_from_chat_room_header_doc_view.view(),
            current_timestamp,
            &passed_user_info,
            member_account_oid.to_string(),
            chat_room_header.accounts_in_chat_room[account_index].thumbnail_size,
            chat_room_header.accounts_in_chat_room[account_index].thumbnail_timestamp.value
    );

    EXPECT_TRUE(return_val);

    MemberSharedInfoMessage generated_user_info;
    DeletedThumbnailInfo::saveDeletedThumbnailInfo(&generated_user_info);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_user_info,
                    passed_user_info
            )
    );

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);

    chat_room_header.accounts_in_chat_room[account_index].thumbnail_size = 0;
    chat_room_header.accounts_in_chat_room[account_index].thumbnail_reference.clear();
    chat_room_header.accounts_in_chat_room[account_index].thumbnail_timestamp = bsoncxx::types::b_date{
            current_timestamp};

    EXPECT_EQ(extracted_chat_room_header, chat_room_header);

}
