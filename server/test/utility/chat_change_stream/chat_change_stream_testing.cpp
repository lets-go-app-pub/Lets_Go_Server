//
// Created by jeremiah on 6/18/22.
//
#include <utility_general_functions.h>
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <account_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include <chat_rooms_objects.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include <server_initialization_functions.h>

#include <create_chat_room_helper.h>
#include <chat_stream_container_object.h>
#include <send_messages_implementation.h>
#include <generate_random_messages.h>
#include <chat_stream_container.h>
#include <chat_room_message_keys.h>

#include "build_match_made_chat_room.h"
#include "change_stream_utility.h"
#include "chat_room_commands_helper_functions.h"
#include "event_request_message_is_valid.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "generate_randoms.h"
#include "global_bsoncxx_docs.h"
#include "user_match_options.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class BeginChatChangeStreamTests : public ::testing::Test {
protected:

    std::unique_ptr<std::thread> chatChangeStreamThread = nullptr;

    std::string chat_room_id;
    std::string chat_room_password;

    ChatStreamContainerObject sending_chat_stream_container;
    ChatStreamContainerObject receiving_chat_stream_container;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>* sending_mock_rw = nullptr;
    grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>* receiving_mock_rw = nullptr;

    bsoncxx::oid sending_user_account_oid;
    bsoncxx::oid receiving_user_account_oid;

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc sending_user_account;
    UserAccountDoc receiving_user_account;

    const std::chrono::milliseconds time_to_sleep_for_change_stream{600};

    std::chrono::milliseconds original_chat_change_stream_await_time{-1};
    std::chrono::milliseconds original_delay_for_message_ordering{-1};
    std::chrono::milliseconds original_message_ordering_thread_sleep_time{-1};
    std::chrono::milliseconds original_chat_change_stream_sleep_time{-1};

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        original_chat_change_stream_await_time = chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME;
        original_delay_for_message_ordering = chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING;
        original_message_ordering_thread_sleep_time = chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME;
        original_chat_change_stream_sleep_time = chat_change_stream_values::CHAT_CHANGE_STREAM_SLEEP_TIME;
        testing_delay_for_messages = std::chrono::milliseconds{-1};

        //NOTE:If the time is too long at the end of the change stream it will have to wait for the remaining
        // time for the change stream threads to complete.
        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = std::chrono::milliseconds{400};

        chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = std::chrono::milliseconds{1};
        chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = std::chrono::milliseconds{1};

        chat_change_stream_values::CHAT_CHANGE_STREAM_SLEEP_TIME = std::chrono::milliseconds{0};

        sending_user_account_oid = insertRandomAccounts(1, 0);
        receiving_user_account_oid = insertRandomAccounts(1, 0);

        sending_user_account.getFromCollection(sending_user_account_oid);
        receiving_user_account.getFromCollection(receiving_user_account_oid);

        sending_chat_stream_container.initialization_complete = true;
        sending_chat_stream_container.current_user_account_oid_str = sending_user_account_oid.to_string();
        receiving_chat_stream_container.initialization_complete = true;
        receiving_chat_stream_container.current_user_account_oid_str = receiving_user_account_oid.to_string();

        user_open_chat_streams.insert(
                sending_user_account_oid.to_string(),
                &sending_chat_stream_container
        );

        user_open_chat_streams.insert(
                receiving_user_account_oid.to_string(),
                &receiving_chat_stream_container
        );

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                sending_user_account_oid,
                sending_user_account.logged_in_token,
                sending_user_account.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();
        chat_room_password = create_chat_room_response.chat_room_password();

        grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

        setupUserLoginInfo(
                join_chat_room_request.mutable_login_info(),
                receiving_user_account_oid,
                receiving_user_account.logged_in_token,
                receiving_user_account.installation_ids.front()
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
                EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
            }
        }

        sending_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(sending_chat_stream_container.responder_.get());
        receiving_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(receiving_chat_stream_container.responder_.get());

        insertUserOIDToChatRoomId(
                chat_room_id,
                sending_user_account_oid.to_string(),
                sending_chat_stream_container.getCurrentIndexValue()
        );

        insertUserOIDToChatRoomId(
                chat_room_id,
                receiving_user_account_oid.to_string(),
                receiving_chat_stream_container.getCurrentIndexValue()
        );

        //See inside function for why this needs to be done.
        spinUntilNextSecond();

        //Start chat change stream last AFTER chat room has been created and user's joined it. This way
        // it will not get the new chat room message.
        chatChangeStreamThread = std::make_unique<std::thread>(
                [&]() {
                    beginChatChangeStream();
                }
        );

        EXPECT_EQ(sending_mock_rw->write_params.size(), 0);
        EXPECT_EQ(receiving_mock_rw->write_params.size(), 0);

        //NOTE: If this is uncommented then the chat_change_stream_await_time above will need to be changed (will
        // make the tests longer). This is because at the end of begin_chat_change_stream the thread will sleep for
        // a little before starting the next loop.
        //sleep to allow change stream to start
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    void TearDown() override {
        cancelChatChangeStream();

        chatChangeStreamThread->join();

        setThreadStartedToFalse();

        chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = original_chat_change_stream_await_time;
        chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = original_delay_for_message_ordering;
        chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = original_message_ordering_thread_sleep_time;
        chat_change_stream_values::CHAT_CHANGE_STREAM_SLEEP_TIME = original_chat_change_stream_sleep_time;

        clearDatabaseAndGlobalsForTesting();
    }

    void checkSpecificMessage(
            const std::string& generated_message_uuid,
            const std::string& passed_chat_room_id,
            int index
    ) {

        bsoncxx::builder::stream::document message_doc_builder;
        ChatRoomMessageDoc message_doc(generated_message_uuid, passed_chat_room_id);

        EXPECT_FALSE(isInvalidUUID(message_doc.id));

        message_doc.convertToDocument(message_doc_builder);
        ChatMessageToClient generated_response;

        //These parameters are copied from begin_chat_change_stream.
        convertChatMessageDocumentToChatMessageToClient(
                message_doc_builder,
                passed_chat_room_id,
                receiving_user_account_oid.to_string(),
                false,
                &generated_response,
                AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
                DifferentUserJoinedChatRoomAmount::SKELETON,
                false,
                nullptr
        );

        if(index < (int)receiving_mock_rw->write_params.size()) {
            EXPECT_EQ(receiving_mock_rw->write_params[index].msg.return_new_chat_message().messages_list_size(), 1);
            if(!receiving_mock_rw->write_params[index].msg.return_new_chat_message().messages_list().empty()) {

                bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                        *receiving_mock_rw->write_params[index].msg.return_new_chat_message().messages_list().rbegin(),
                        generated_response
                );

                if(!equivalent) {
                    std::cout << "receiving_mock_rw\n" << receiving_mock_rw->write_params[index].msg.return_new_chat_message().messages_list().rbegin()->DebugString() << '\n';
                    std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
                }

                EXPECT_TRUE(equivalent);
            }
        }
    }

    void sendSimpleMessageCheck(
            const std::string& generated_message_uuid,
            std::string passed_chat_room_id = "",
            int sending_message_list_size_ = 1,
            bool new_update_message = true
    ) {

        if(passed_chat_room_id.empty())
            passed_chat_room_id = chat_room_id;

        //Must sleep while the chat change stream thread receives the message from the database, then
        // processes it.
        std::this_thread::sleep_for(time_to_sleep_for_change_stream);

        EXPECT_EQ(sending_mock_rw->write_params.size(), 1);
        if(sending_mock_rw->write_params.size() == 1 && new_update_message) {
            EXPECT_TRUE(sending_mock_rw->write_params[0].msg.has_return_new_chat_message());
            if(sending_mock_rw->write_params[0].msg.has_return_new_chat_message()) {
                EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), sending_message_list_size_);
                if(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size() == sending_message_list_size_) {
                    int index = 0;
                    if(sending_message_list_size_ > 1) {
                        EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list(index).message().message_specifics().message_body_case(), MessageSpecifics::MessageBodyCase::kChatRoomCapMessage);
                        index++;
                    }
                    EXPECT_EQ(
                            sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list(index).message().message_specifics().message_body_case(),
                            MessageSpecifics::MessageBodyCase::kNewUpdateTimeMessage
                    );
                }
            }
        }
        EXPECT_EQ(receiving_mock_rw->write_params.size(), 1);

        checkSpecificMessage(
                generated_message_uuid,
                passed_chat_room_id,
                0
        );
    }

    void buildAndCompareMatchMade(
            mongocxx::database& accounts_db,
            mongocxx::collection& user_accounts_collection,
            mongocxx::collection& chat_room_collection,
            const std::string& match_chat_room_id,
            const bsoncxx::oid& user_account_oid,
            const bsoncxx::oid& match_account_oid,
            grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>* mock_rw
    ) {

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeMessagesToVector(reply_vector);

        bsoncxx::document::value dummy_user_doc = document{}
                << "matchUserOID" << match_account_oid
                << "NOTE" << "dummy document from begin_chat_change_stream; match_made"
                << finalize;

        sendNewChatRoomAndMessages(
                mongo_cpp_client,
                accounts_db,
                user_accounts_collection,
                chat_room_collection,
                dummy_user_doc.view(),
                match_chat_room_id,
                current_timestamp,
                getCurrentTimestamp(),
                user_account_oid.to_string(),
                &storeMessagesToVector,
                HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
                AmountOfMessage::COMPLETE_MESSAGE_INFO,
                false,
                false
        );

        storeMessagesToVector.finalCleanup();

        EXPECT_EQ(reply_vector.size(), 1);

        EXPECT_EQ((*reply_vector.front()).return_new_chat_message().messages_list_size(), 4);
        EXPECT_EQ(mock_rw->write_params.front().msg.return_new_chat_message().messages_list_size(), 4);

        if((*reply_vector.front()).return_new_chat_message().messages_list_size() == mock_rw->write_params.front().msg.return_new_chat_message().messages_list_size()) {
            for(int i = 0; i < (*reply_vector.front()).return_new_chat_message().messages_list_size(); i++) {

                //make the 'current_timestamp' value line up if this is kThisUserJoinedChatRoomMemberMessage type message
                if((*reply_vector.front()).return_new_chat_message().messages_list()[i].message().message_specifics().message_body_case() == MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomMemberMessage
                   && mock_rw->write_params.front().msg.return_new_chat_message().messages_list()[i].message().message_specifics().message_body_case() == MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomMemberMessage) {
                    (*reply_vector.front()).mutable_return_new_chat_message()->mutable_messages_list(i)->mutable_message()->mutable_message_specifics()->mutable_this_user_joined_chat_room_member_message()->mutable_member_info()->mutable_user_info()->set_current_timestamp(
                            mock_rw->write_params.front().msg.return_new_chat_message().messages_list()[i].message().message_specifics().this_user_joined_chat_room_member_message().member_info().user_info().current_timestamp()
                    );
                }

                EXPECT_TRUE(
                        google::protobuf::util::MessageDifferencer::Equivalent(
                                (*reply_vector.front()).return_new_chat_message().messages_list()[i],
                                mock_rw->write_params.front().msg.return_new_chat_message().messages_list()[i]
                        )
                );
            }
        }
    }

    static void spinForCondition(const std::function<bool()>& condition_lambda) {
        //When all tests run, this needs to be here because the change stream seems to have a cooldown (internally
        // from the database maybe?) before the server properly starts it.
        //During attempting to find this problem, the message was not received by the change stream when the test failed.
        for(int i = 0; condition_lambda(); ++i) {
            if(i == 8) {
                static const timespec ns = {0, 1}; //1 nanosecond
                nanosleep(&ns, nullptr);
                i = 0;
            }
        }
    }

};

TEST_F(BeginChatChangeStreamTests, cancelChatChangeStream) {
    chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = std::chrono::milliseconds{200};

    std::atomic_bool function_completed = false;

    std::thread chatChangeStreamThread{[&]() {
        beginChatChangeStream();
        function_completed = true;
    }};

    //Allow function to initialize.
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    cancelChatChangeStream();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    std::chrono::milliseconds end_time = current_timestamp + std::chrono::milliseconds{5000L};

    while (current_timestamp < end_time) {
        if (function_completed) {
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds{10L});
            current_timestamp = getCurrentTimestamp();
        }
    }

    EXPECT_TRUE(function_completed);

    if (function_completed) {
        chatChangeStreamThread.join();
    } else {
        //NOTE: This will leak the thread if cancelChatChangeStream() failed. However, it should be
        // cleaned up after the tests have completed.
        chatChangeStreamThread.detach();
    }

    chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME = std::chrono::milliseconds{30000};
}

TEST_F(BeginChatChangeStreamTests, textMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomTextMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, pictureMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomPictureMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, locationMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomLocationMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, mimeTypeMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomMimeTypeMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, inviteMessageReturned) {

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    //don't want this empty
    create_chat_room_request.set_chat_room_name(gen_random_alpha_numeric_string(rand() % 50 + 5));

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    std::string generated_message_uuid = generateUUID();

    generateRandomInviteTypeMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid,
            receiving_user_account_oid.to_string(),
            receiving_user_account.first_name,
            create_chat_room_response.chat_room_id(),
            create_chat_room_request.chat_room_name(),
            create_chat_room_response.chat_room_password()
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(
            generated_message_uuid,
            "",
            2
    );
}

TEST_F(BeginChatChangeStreamTests, editedMessageReturned) {

    //spin until the thread begins
    while(!getThreadStarted()) {}

    //cancel chat change stream so that it does not
    cancelChatChangeStream();

    chatChangeStreamThread->join();

    std::string generated_text_message_uuid = generateUUID();

    generateRandomTextMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_text_message_uuid
    );

    //See inside function for why this needs to be done.
    spinUntilNextSecond();

    //Start chat change stream last AFTER text message was generated. This way the change stream
    // will not get the new chat room message.
    chatChangeStreamThread = std::make_unique<std::thread>(
            [&]() {
                beginChatChangeStream();
            }
    );

    std::string generated_message_uuid = generateUUID();

    generateRandomEditedMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid,
            generated_text_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, deletedMessageReturned) {

    //spin until the thread begins
    while(!getThreadStarted()) {}

    //cancel chat change stream so that it does not
    cancelChatChangeStream();

    chatChangeStreamThread->join();

    std::string generated_text_message_uuid = generateUUID();

    generateRandomTextMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_text_message_uuid
    );

    //See inside function for why this needs to be done.
    spinUntilNextSecond();

    //Start chat change stream last AFTER text message was generated. This way the change stream
    // will not get the new chat room message.
    chatChangeStreamThread = std::make_unique<std::thread>(
            [&]() {
                beginChatChangeStream();
            }
    );

    std::string generated_message_uuid = generateUUID();

    generateRandomDeletedMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid,
            generated_text_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, userActivityMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomUserActivityDetectedMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, updateObserverTimeMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomUpdateObservedTimeMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    //Must sleep while the chat change stream thread receives the message from the database, then
    // processes it.
    std::this_thread::sleep_for(time_to_sleep_for_change_stream);

    //observed time message type does not send an actual message from the change stream
    EXPECT_EQ(sending_mock_rw->write_params.size(), 0);
    EXPECT_EQ(receiving_mock_rw->write_params.size(), 0);
}

TEST_F(BeginChatChangeStreamTests, userKickedMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomUserKickedMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid,
            receiving_user_account_oid.to_string()
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, userBannedMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomUserBannedMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid,
            receiving_user_account_oid.to_string()
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, userKickedMessageReturned_otherUser) {

    bsoncxx::oid kicked_user_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc kicked_user_account(kicked_user_account_oid);

    //spin until the thread begins
    while(!getThreadStarted()) {}

    //cancel chat change stream so that it does not
    cancelChatChangeStream();

    chatChangeStreamThread->join();

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            kicked_user_account_oid,
            kicked_user_account.logged_in_token,
            kicked_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    //See inside function for why this needs to be done.
    spinUntilNextSecond();

    //Start chat change stream last AFTER text message was generated. This way the change stream
    // will not get the new chat room message.
    chatChangeStreamThread = std::make_unique<std::thread>(
            [&]() {
                beginChatChangeStream();
            }
    );

    std::string generated_message_uuid = generateUUID();

    generateRandomUserKickedMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid,
            kicked_user_account_oid.to_string()
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, differentUserJoinedMessageReturned) {

    //spin until the thread begins
    while(!getThreadStarted()) {}

    //cancel chat change stream so that it does not
    cancelChatChangeStream();

    chatChangeStreamThread->join();

    grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
    grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

    setupUserLoginInfo(
        leave_chat_room_request.mutable_login_info(),
        sending_user_account_oid,
        sending_user_account.logged_in_token,
        sending_user_account.installation_ids.front()
    );

    leave_chat_room_request.set_chat_room_id(chat_room_id);

    leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

    EXPECT_EQ(leave_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    //See inside function for why this needs to be done.
    spinUntilNextSecond();

    //Start chat change stream last AFTER text message was generated. This way the change stream
    // will not get the new chat room message.
    chatChangeStreamThread = std::make_unique<std::thread>(
            [&]() {
                beginChatChangeStream();
            }
    );

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << -1
                    << finalize
    );

    //get most recent message (different user joined chat room message)
    auto message_document = chat_room_collection.find_one(
            document{} << finalize,
            opts
    );

    EXPECT_TRUE(message_document);

    if(message_document) {

        grpc_stream_chat::ChatToClientResponse generated_chat_to_client_response;

        ChatMessageToClient* responseMsg = generated_chat_to_client_response.mutable_return_new_chat_message()->add_messages_list();

        convertChatMessageDocumentToChatMessageToClient(
                *message_document, chat_room_id,
                chat_stream_container::CHAT_CHANGE_STREAM_PASSED_STRING_TO_CONVERT, false, responseMsg,
                AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
                DifferentUserJoinedChatRoomAmount::SKELETON,
                false, nullptr
        );

        auto userMemberInfo = responseMsg->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info();
        getUserAccountInfoForUserJoinedChatRoom(mongo_cpp_client, accounts_db,
                                                user_accounts_collection,
                                                responseMsg->message().standard_message_info().chat_room_id_message_sent_from(),
                                                responseMsg->sent_by_account_id(),
                                                HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
                                                userMemberInfo);

        //Must sleep while the chat change stream thread receives the message from the database, then
        // processes it.
        std::this_thread::sleep_for(time_to_sleep_for_change_stream);

        EXPECT_EQ(sending_mock_rw->write_params.size(), 1);
        EXPECT_EQ(receiving_mock_rw->write_params.size(), 1);

        if(sending_mock_rw->write_params.size() == 1) {
            EXPECT_TRUE(sending_mock_rw->write_params[0].msg.has_return_new_chat_message());
            if(sending_mock_rw->write_params[0].msg.has_return_new_chat_message()) {
                EXPECT_FALSE(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list().empty());
                if(!sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(
                            sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                            MessageSpecifics::MessageBodyCase::kNewUpdateTimeMessage
                    );
                }
            }
        }

        if(!receiving_mock_rw->write_params.empty()) {
            EXPECT_EQ(receiving_mock_rw->write_params.front().msg.return_new_chat_message().messages_list_size(), 1);
            if(!receiving_mock_rw->write_params.front().msg.return_new_chat_message().messages_list().empty()) {
                EXPECT_EQ(
                        generated_chat_to_client_response.return_new_chat_message().messages_list_size(),
                        receiving_mock_rw->write_params.front().msg.return_new_chat_message().messages_list_size()
                );

                if(generated_chat_to_client_response.return_new_chat_message().messages_list_size() ==
                   receiving_mock_rw->write_params.front().msg.return_new_chat_message().messages_list_size()) {
                    for(int i = 0; i < generated_chat_to_client_response.return_new_chat_message().messages_list_size(); i++) {

                        //make the 'current_timestamp' value line up if this is kDifferentUserJoinedMessage type message
                        if(receiving_mock_rw->write_params.front().msg.return_new_chat_message().messages_list()[i].message().message_specifics().message_body_case() == MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
                           && generated_chat_to_client_response.return_new_chat_message().messages_list()[i].message().message_specifics().message_body_case() == MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage) {
                            generated_chat_to_client_response.mutable_return_new_chat_message()->mutable_messages_list(i)->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info()->set_current_timestamp(
                                    receiving_mock_rw->write_params.front().msg.return_new_chat_message().messages_list()[i].message().message_specifics().different_user_joined_message().member_info().user_info().current_timestamp()
                            );
                        }

                        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                                receiving_mock_rw->write_params.front().msg.return_new_chat_message().messages_list()[i],
                                generated_chat_to_client_response.return_new_chat_message().messages_list()[i]
                        );

                        if(!equivalent) {
                            std::cout << "receiving_mock_rw\n" << receiving_mock_rw->write_params.front().msg.return_new_chat_message().messages_list()[i].DebugString() << '\n';
                            std::cout << "generated_chat_to_client_response\n" << generated_chat_to_client_response.return_new_chat_message().messages_list()[i].DebugString() << '\n';
                        }

                        EXPECT_TRUE(equivalent);
                    }
                }
            }
        }
    }
}

TEST_F(BeginChatChangeStreamTests, differentUserJoinedMessageReturned_userJoinedFromEvent) {

    createAndStoreEventAdminAccount();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    EventRequestMessage event_info = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    //Make sure event can be matched by user.
    event_info.clear_allowed_genders();
    event_info.add_allowed_genders(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);

    event_info.set_min_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
    event_info.set_max_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);

    auto event_created_return_values = generateRandomEvent(
            chat_room_admin_info,
            TEMP_ADMIN_ACCOUNT_NAME,
            current_timestamp,
            event_info
    );

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            receiving_user_account_oid,
            receiving_user_account.logged_in_token,
            receiving_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(event_created_return_values.chat_room_return_info.chat_room_id());
    join_chat_room_request.set_chat_room_password(event_created_return_values.chat_room_return_info.chat_room_password());

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> join_mock_server_writer;

    joinChatRoom(&join_chat_room_request, &join_mock_server_writer);

    EXPECT_EQ(join_mock_server_writer.write_params.size(), 2);
    if (join_mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!join_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(join_mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(join_mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }
    }

    std::string generated_message_uuid = generateUUID();

    generateRandomTextMessage(
            receiving_user_account_oid,
            receiving_user_account.logged_in_token,
            receiving_user_account.installation_ids.front(),
            event_created_return_values.chat_room_return_info.chat_room_id(),
            generated_message_uuid
    );

    //See inside function for why this needs to be done.
    spinUntilNextSecond();

    UserMatchOptionsRequest user_match_option_request;
    UserMatchOptionsResponse user_match_option_response;

    setupUserLoginInfo(
            user_match_option_request.mutable_login_info(),
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front()
    );

    user_match_option_request.set_match_account_id(event_created_return_values.event_account_oid);
    user_match_option_request.set_response_type(ResponseType::USER_MATCH_OPTION_YES);

    receiveMatchYes(&user_match_option_request, &user_match_option_response);

    ASSERT_EQ(user_match_option_response.return_status(), ReturnStatus::SUCCESS);

    std::cout << "starting spinForCondition()" << std::endl;
    spinForCondition([&](){return sending_mock_rw->write_params.empty();});
    std::cout << "finishing spinForCondition()" << std::endl;

    //must sleep while the chat change stream returns the match from the database AND then thread pool runs tasks from injectStreamResponse()
    std::this_thread::sleep_for(time_to_sleep_for_change_stream);

    grpc_stream_chat::ChatToClientResponse reply_;

    EXPECT_EQ(sending_mock_rw->write_params.size(), 1);

    if(sending_mock_rw->write_params.size() == 1) {

        EXPECT_TRUE(sending_mock_rw->write_params[0].msg.has_return_new_chat_message());

        if(sending_mock_rw->write_params[0].msg.has_return_new_chat_message()) {

            EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), 9);

            //Make sure all message types are returned. This is just an extension of sendNewChatRoomAndMessages(), specifics are
            // tested elsewhere.
            if(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size() == 9) {
                EXPECT_EQ(MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomStartMessage, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[0].message().message_specifics().message_body_case());
                EXPECT_EQ(MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomMemberMessage, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[1].message().message_specifics().message_body_case());
                EXPECT_EQ(MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomMemberMessage, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[2].message().message_specifics().message_body_case());
                EXPECT_EQ(MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomMemberMessage, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[3].message().message_specifics().message_body_case());
                EXPECT_EQ(MessageSpecifics::MessageBodyCase::kChatRoomCapMessage, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[4].message().message_specifics().message_body_case());
                EXPECT_EQ(MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[5].message().message_specifics().message_body_case());
                EXPECT_EQ(MessageSpecifics::MessageBodyCase::kTextMessage, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[6].message().message_specifics().message_body_case());
                EXPECT_EQ(MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[7].message().message_specifics().message_body_case());
                EXPECT_EQ(MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomFinishedMessage, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[8].message().message_specifics().message_body_case());

                if(MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomFinishedMessage == sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[8].message().message_specifics().message_body_case()) {
                    //Make sure event_account_oid is properly returned.
                    EXPECT_EQ(event_created_return_values.event_account_oid, sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[8].message().message_specifics().this_user_joined_chat_room_finished_message().yes_swipe_event_oid());
                }
            }
        }
    }

}

TEST_F(BeginChatChangeStreamTests, differentUserLeftMessageReturned) {

    grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
    grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

    setupUserLoginInfo(
            leave_chat_room_request.mutable_login_info(),
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front()
    );

    leave_chat_room_request.set_chat_room_id(chat_room_id);

    leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << -1
                    << finalize
    );

    //get most recent message (different user left chat room message)
    auto message_document = chat_room_collection.find_one(
            document{} << finalize,
            opts
    );

    EXPECT_TRUE(message_document);

    if(message_document) {
        spinForCondition([&](){return sending_mock_rw->write_params.empty();});

        sendSimpleMessageCheck(message_document->view()["_id"].get_string().value.to_string());
    }
}

TEST_F(BeginChatChangeStreamTests, chatRoomNameMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomChatRoomNameUpdatedMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, chatRoomPasswordMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomChatRoomPasswordUpdatedMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, newAdminPromotedMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomNewAdminPromotedUpdatedMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid,
            receiving_user_account_oid.to_string()
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, newPinnedLocationMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    generateRandomNewPinnedLocationMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            chat_room_id,
            generated_message_uuid
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    sendSimpleMessageCheck(generated_message_uuid);
}

TEST_F(BeginChatChangeStreamTests, chatRoomCapMessageReturned) {

    std::string generated_message_uuid = generateUUID();

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    //get most recent message (un_match type chat room message)
    auto insert_result = chat_room_collection.insert_one(
            bsoncxx::builder::stream::document{}
                    << "_id" << generated_message_uuid
                    << chat_room_message_keys::MESSAGE_SENT_BY << sending_user_account_oid
                    << chat_room_message_keys::MESSAGE_TYPE << bsoncxx::types::b_int32{MessageSpecifics::MessageBodyCase::kChatRoomCapMessage}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{current_timestamp}
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << bsoncxx::builder::stream::open_document
                    << bsoncxx::builder::stream::close_document
                    << bsoncxx::builder::stream::finalize
    );

    EXPECT_EQ(insert_result->result().inserted_count(), 1);

    if(insert_result) {
        spinForCondition([&](){return sending_mock_rw->write_params.empty();});

        sendSimpleMessageCheck(generated_message_uuid);
    }
}

TEST_F(BeginChatChangeStreamTests, matchCanceledMessageReturned) {
    //spin until the thread begins
    while(!getThreadStarted()) {}

    //cancel chat change stream so that it does not
    cancelChatChangeStream();

    chatChangeStreamThread->join();

    std::string match_made_chat_room_id = buildMatchMadeChatRoom(
            current_timestamp,
            sending_user_account_oid,
            receiving_user_account_oid,
            sending_user_account,
            receiving_user_account
    );

    sending_user_account.other_accounts_matched_with.emplace_back(
            receiving_user_account_oid.to_string(),
            bsoncxx::types::b_date{current_timestamp}
    );
    sending_user_account.setIntoCollection();

    receiving_user_account.other_accounts_matched_with.emplace_back(
            sending_user_account_oid.to_string(),
            bsoncxx::types::b_date{current_timestamp}
    );
    receiving_user_account.setIntoCollection();

    std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> sending_chat_stream =
            user_open_chat_streams.find(sending_user_account_oid.to_string());

    std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> receiving_chat_stream =
            user_open_chat_streams.find(receiving_user_account_oid.to_string());

    EXPECT_NE(sending_chat_stream, nullptr);
    EXPECT_NE(receiving_chat_stream, nullptr);

    insertUserOIDToChatRoomId(
            match_made_chat_room_id,
            sending_user_account_oid.to_string(),
            sending_chat_stream->ptr()->getCurrentIndexValue()
    );

    insertUserOIDToChatRoomId(
            match_made_chat_room_id,
            receiving_user_account_oid.to_string(),
            receiving_chat_stream->ptr()->getCurrentIndexValue()
    );

    //See inside function for why this needs to be done.
    spinUntilNextSecond();

    //Start chat change stream last AFTER text message was generated. This way the change stream
    // will not get the new chat room message.
    chatChangeStreamThread = std::make_unique<std::thread>(
            [&]() {
                beginChatChangeStream();
            }
    );
    generateRandomMatchCanceledMessage(
            sending_user_account_oid,
            sending_user_account.logged_in_token,
            sending_user_account.installation_ids.front(),
            match_made_chat_room_id,
            receiving_user_account_oid.to_string()
    );

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + match_made_chat_room_id];

    mongocxx::options::find opts;

    opts.sort(
            document{}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << -1
                    << finalize
    );

    //get most recent message (un_match type chat room message)
    auto message_document = chat_room_collection.find_one(
            document{} << finalize,
            opts
    );

    EXPECT_TRUE(message_document);

    if(message_document) {
        spinForCondition([&](){return sending_mock_rw->write_params.empty();});

        sendSimpleMessageCheck(
                message_document->view()["_id"].get_string().value.to_string(),
                match_made_chat_room_id
        );
    }
}

TEST_F(BeginChatChangeStreamTests, matchMadeForBothUsers) {

    //Flow when match made
    // 1) chat change stream receives msg
    // 2) single user is inserted to map_of_chat_rooms_to_users
    // 3) It calls sendMessageToSpecificUser() from the ChatRoomObject it gets from map_of_chat_rooms_to_users. Then it
    // accesses user_open_chat_streams and tries to find the user. It then calls injectStreamResponse() from the
    // ChatStreamContainerObject it receives from user_open_chat_streams. This ultimately will call Write().

    std::string match_made_chat_room_id = buildMatchMadeChatRoom(
            current_timestamp,
            sending_user_account_oid,
            receiving_user_account_oid,
            sending_user_account,
            receiving_user_account
    );

    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    //must sleep while the chat change stream returns the match from the database AND then thread pool runs tasks from injectStreamResponse()
    std::this_thread::sleep_for(time_to_sleep_for_change_stream);

    grpc_stream_chat::ChatToClientResponse reply_;

    EXPECT_EQ(sending_mock_rw->write_params.size(), 1);
    EXPECT_EQ(receiving_mock_rw->write_params.size(), 1);

    if(sending_mock_rw->write_params.size() == 1
       && receiving_mock_rw->write_params.size() == 1) {
        mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

        mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
        mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + match_made_chat_room_id];

        buildAndCompareMatchMade(
                accounts_db,
                user_accounts_collection,
                chat_room_collection,
                match_made_chat_room_id,
                sending_user_account_oid,
                receiving_user_account_oid,
                sending_mock_rw
        );

        buildAndCompareMatchMade(
                accounts_db,
                user_accounts_collection,
                chat_room_collection,
                match_made_chat_room_id,
                receiving_user_account_oid,
                sending_user_account_oid,
                receiving_mock_rw
        );
    }
}

void manuallyInsertTextMessage(
        const std::string& message_uuid,
        const bsoncxx::oid& sending_user_account_oid,
        const std::chrono::milliseconds& timestamp,
        mongocxx::collection chat_room_collection
) {
    auto insert_result = chat_room_collection.insert_one(
            bsoncxx::builder::stream::document{}
                    << "_id" << message_uuid
                    << chat_room_message_keys::MESSAGE_SENT_BY << sending_user_account_oid
                    << chat_room_message_keys::MESSAGE_TYPE << bsoncxx::types::b_int32{MessageSpecifics::MessageBodyCase::kTextMessage}
                    << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{timestamp}
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << bsoncxx::builder::stream::open_document
                    << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << bsoncxx::builder::stream::open_document
                    << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << DeleteType::DELETE_TYPE_NOT_SET
                    << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY <<  bsoncxx::builder::stream::open_array
                    << bsoncxx::builder::stream::close_array
                    << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{}
                    << bsoncxx::builder::stream::close_document
                    << chat_room_message_keys::message_specifics::TEXT_MESSAGE << gen_random_alpha_numeric_string(50)
                    << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << false
                    << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
                    << bsoncxx::builder::stream::close_document
                    << chat_room_message_keys::RANDOM_INT << bsoncxx::types::b_int32{rand()}
                    << bsoncxx::builder::stream::finalize
    );

    EXPECT_EQ(insert_result->result().inserted_count(), 1);
}

TEST_F(BeginChatChangeStreamTests, orderingEnforced) {

    std::chrono::milliseconds delay_time{200};

    chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = delay_time;
    chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = std::chrono::milliseconds{10};

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    std::vector<std::pair<std::string, std::chrono::milliseconds>> uuid_and_timestamps{
            {generateUUID(), std::chrono::milliseconds{500}},
            {generateUUID(), std::chrono::milliseconds{600}},
            {generateUUID(), std::chrono::milliseconds{700}}
    };

    for(const auto& x : uuid_and_timestamps) {
        std::cout << x.first << ' ' << x.second.count() << '\n';
    }

    //Sending the messages in order 3, 1, 2 (forces out of order timestamps). Expecting
    // them to be returned in order

    manuallyInsertTextMessage(
            uuid_and_timestamps[2].first,
            sending_user_account_oid,
            uuid_and_timestamps[2].second,
            chat_room_collection
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{30});

    manuallyInsertTextMessage(
            uuid_and_timestamps[0].first,
            sending_user_account_oid,
            uuid_and_timestamps[0].second,
            chat_room_collection
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{30});

    manuallyInsertTextMessage(
            uuid_and_timestamps[1].first,
            sending_user_account_oid,
            uuid_and_timestamps[1].second,
            chat_room_collection
    );

    std::this_thread::sleep_for(delay_time + std::chrono::milliseconds{1000});

    std::cout << "spinForCondition()" << std::endl;
    spinForCondition([&](){return sending_mock_rw->write_params.empty();});

    EXPECT_EQ(sending_mock_rw->write_params.size(), 1);
    EXPECT_EQ(receiving_mock_rw->write_params.size(), 1);

    std::shared_ptr<
            std::pair<
                    std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> &)>,
                    PushedToQueueFromLocation
            >
    > function_and_type;
    auto handle = receiving_chat_stream_container.writes_waiting_to_process.front_if_not_empty(function_and_type).handle;
    handle.resume();
    handle.destroy();

    int i = 0;
    for(; i < (int)uuid_and_timestamps.size() && function_and_type; i++) {

        EXPECT_EQ(function_and_type->second, PushedToQueueFromLocation::PUSHED_FROM_INJECTION);

        bsoncxx::builder::stream::document message_doc_builder;
        ChatRoomMessageDoc message_doc(uuid_and_timestamps[i].first, chat_room_id);

        EXPECT_FALSE(isInvalidUUID(message_doc.id));

        message_doc.convertToDocument(message_doc_builder);
        ChatMessageToClient generated_response;

        //These parameters are copied from begin_chat_change_stream.
        convertChatMessageDocumentToChatMessageToClient(
                message_doc_builder,
                chat_room_id,
                receiving_user_account_oid.to_string(),
                false,
                &generated_response,
                AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
                DifferentUserJoinedChatRoomAmount::SKELETON,
                false,
                nullptr
        );

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
        function_and_type->first(reply_vector);

        EXPECT_EQ(reply_vector.size(), 1);
        if(!reply_vector.empty()) {
            EXPECT_TRUE(reply_vector[0]->has_return_new_chat_message());
            if(reply_vector[0]->has_return_new_chat_message()) {
                EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 1);
                if(!reply_vector[0]->return_new_chat_message().messages_list().empty()) {

                    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                            reply_vector[0]->return_new_chat_message().messages_list(0),
                            generated_response
                    );

                    if (!equivalent) {
                        std::cout << "reply_vector\n" << reply_vector[0]->return_new_chat_message().messages_list(0).DebugString() << '\n';
                        std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
                    }

                    EXPECT_TRUE(equivalent);

                }
            }
        }

        bool successful;
        handle = receiving_chat_stream_container.writes_waiting_to_process.pop_front(successful).handle;
        handle.resume();
        handle.destroy();

        handle = receiving_chat_stream_container.writes_waiting_to_process.front_if_not_empty(function_and_type).handle;
        handle.resume();
        handle.destroy();
    }

    EXPECT_EQ(i, 3);

}
