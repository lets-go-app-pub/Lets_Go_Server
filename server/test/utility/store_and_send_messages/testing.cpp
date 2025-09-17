//
// Created by jeremiah on 6/21/22.
//
#include <utility_general_functions.h>
#include <fstream>
#include <account_objects.h>
#include <ChatRoomCommands.pb.h>
#include "gtest/gtest.h"
#include <google/protobuf/util/message_differencer.h>

#include <sort_time_frames_and_remove_overlaps.h>
#include <user_account_keys.h>
#include <ChatMessageStream.pb.h>
#include <store_and_send_messages.h>
#include <grpc_values.h>
#include <grpc_mock_stream/mock_stream.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class StoreAndSendMessages : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

//NOTE: Using the implementation StoreAndSendRequestedMessagesToClientBiDiStream to test StoreAndSendMessagesVirtual basic
// functionality.
TEST_F(StoreAndSendMessages, storeAndSendMessagesVirtual_messageListTooLarge) {

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    std::string dummy_chat_room_id = "12345678";

    StoreAndSendRequestedMessagesToClientBiDiStream storeMessagesToVector(
            reply_vector,
            dummy_chat_room_id
            );

    for(int i = 0; i < 9; i++) {
        ChatMessageToClient msg;

        msg.set_message_uuid(std::string(grpc_values::MAXIMUM_SIZE_FOR_SENDING_CHAT_MESSAGE_TO_CLIENT/10, i + 'a'));

        storeMessagesToVector.sendMessage(msg);
    }

    EXPECT_TRUE(reply_vector.empty());

    ChatMessageToClient unique_msg;

    unique_msg.set_message_uuid(std::string(grpc_values::MAXIMUM_SIZE_FOR_SENDING_CHAT_MESSAGE_TO_CLIENT/10, 10 + 'a'));

    storeMessagesToVector.sendMessage(unique_msg);

    ASSERT_EQ(reply_vector.size(), 1);

    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 2);

    ASSERT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), 9);
    ASSERT_EQ(reply_vector[1]->request_full_message_info_response().full_messages().full_message_list_size(), 1);

    for(int i = 0; i < reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(); i++) {
        ChatMessageToClient msg;
        msg.set_message_uuid(std::string(grpc_values::MAXIMUM_SIZE_FOR_SENDING_CHAT_MESSAGE_TO_CLIENT/10, i + 'a'));

        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        msg,
                        reply_vector[0]->request_full_message_info_response().full_messages().full_message_list(i)
                        )
                        );
    }

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    unique_msg,
                    reply_vector[1]->request_full_message_info_response().full_messages().full_message_list(0)
                    )
                    );
}

//NOTE: Using the implementation StoreAndSendRequestedMessagesToClientBiDiStream to test StoreAndSendMessagesVirtual basic
// functionality.
TEST_F(StoreAndSendMessages, storeAndSendMessagesVirtual_singleMessageTooLarge) {

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    std::string dummy_chat_room_id = "12345678";

    StoreAndSendRequestedMessagesToClientBiDiStream storeMessagesToVector(
            reply_vector,
            dummy_chat_room_id
            );

    ChatMessageToClient msg;

    msg.set_message_uuid(generateUUID());
    msg.mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(std::string(grpc_values::MAXIMUM_SIZE_FOR_SENDING_CHAT_MESSAGE_TO_CLIENT, 'a'));

    storeMessagesToVector.sendMessage(msg);

    EXPECT_TRUE(reply_vector.empty());

    storeMessagesToVector.finalCleanup();

    EXPECT_TRUE(reply_vector.empty());
}

TEST_F(StoreAndSendMessages, storeAndSendRequestedMessagesToClientBiDiStream) {

    //Each child has 3 functions that it implements
    // 1) constructor
    // 2) clearMessageList()
    // 3) writeMessageToClient()
    // These are tested through the exposed API.

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;
    std::string dummy_chat_room_id = "12345678";

    StoreAndSendRequestedMessagesToClientBiDiStream storeMessagesToVector(
            reply_vector,
            dummy_chat_room_id
    );

    ChatMessageToClient first_msg;
    ChatMessageToClient second_msg;

    first_msg.set_message_uuid(generateUUID());
    second_msg.set_message_uuid(generateUUID());

    storeMessagesToVector.sendMessage(first_msg);

    EXPECT_TRUE(reply_vector.empty());

    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);

    ASSERT_EQ(reply_vector[0]->request_full_message_info_response().full_messages().full_message_list_size(), 1);

    EXPECT_TRUE(
        google::protobuf::util::MessageDifferencer::Equivalent(
            first_msg,
            reply_vector[0]->request_full_message_info_response().full_messages().full_message_list(0)
        )
    );

    storeMessagesToVector.sendMessage(second_msg);

    EXPECT_EQ(reply_vector.size(), 1);

    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 2);

    ASSERT_EQ(reply_vector[1]->request_full_message_info_response().full_messages().full_message_list_size(), 1);

    EXPECT_TRUE(
        google::protobuf::util::MessageDifferencer::Equivalent(
            second_msg,
            reply_vector[1]->request_full_message_info_response().full_messages().full_message_list(0)
        )
    );

}

TEST_F(StoreAndSendMessages, storeAndSendNewMessageToClientBiDiStream) {

    //Each child has 3 functions that it implements
    // 1) constructor
    // 2) clearMessageList()
    // 3) writeMessageToClient()
    // These are tested through the exposed API.

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> storeMessagesToVector(reply_vector);

    ChatMessageToClient first_msg;
    ChatMessageToClient second_msg;

    first_msg.set_message_uuid(generateUUID());
    second_msg.set_message_uuid(generateUUID());

    storeMessagesToVector.sendMessage(first_msg);

    EXPECT_TRUE(reply_vector.empty());

    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 1);

    ASSERT_EQ(reply_vector[0]->return_new_chat_message().messages_list_size(), 1);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    first_msg,
                    reply_vector[0]->return_new_chat_message().messages_list(0)
                    )
                    );

    storeMessagesToVector.sendMessage(second_msg);

    EXPECT_EQ(reply_vector.size(), 1);

    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 2);

    ASSERT_EQ(reply_vector[1]->return_new_chat_message().messages_list_size(), 1);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    second_msg,
                    reply_vector[1]->return_new_chat_message().messages_list(0)
                    )
                    );

}

TEST_F(StoreAndSendMessages, storeAndSendRequestedMessagesToInitialLoginWithBiDiStream) {

    //Each child has 3 functions that it implements
    // 1) constructor
    // 2) clearMessageList()
    // 3) writeMessageToClient()
    // These are tested through the exposed API.

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE> storeMessagesToVector(reply_vector);

    ChatMessageToClient first_msg;
    ChatMessageToClient second_msg;

    first_msg.set_message_uuid(generateUUID());
    second_msg.set_message_uuid(generateUUID());

    storeMessagesToVector.sendMessage(first_msg);

    EXPECT_TRUE(reply_vector.empty());

    storeMessagesToVector.finalCleanup();

    std::cout << "reply_vector\n" << reply_vector[0]->DebugString() << '\n';

    ASSERT_EQ(reply_vector.size(), 1);

    ASSERT_EQ(reply_vector[0]->initial_connection_messages_response().messages_list_size(), 1);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    first_msg,
                    reply_vector[0]->initial_connection_messages_response().messages_list(0)
                    )
                    );

    storeMessagesToVector.sendMessage(second_msg);
    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(reply_vector.size(), 2);

    ASSERT_EQ(reply_vector[1]->initial_connection_messages_response().messages_list_size(), 1);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    second_msg,
                    reply_vector[1]->initial_connection_messages_response().messages_list(0)
                    )
                    );

}

TEST_F(StoreAndSendMessages, storeAndSendMessagesToJoinChatRoom) {

    //Each child has 3 functions that it implements
    // 1) constructor
    // 2) clearMessageList()
    // 3) writeMessageToClient()
    // These are tested through the exposed API.

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> responseStream;

    StoreAndSendMessagesToJoinChatRoom storeMessagesToVector(&responseStream);

    ChatMessageToClient first_msg;
    ChatMessageToClient second_msg;

    first_msg.set_message_uuid(generateUUID());
    second_msg.set_message_uuid(generateUUID());

    storeMessagesToVector.sendMessage(first_msg);

    EXPECT_TRUE(responseStream.write_params.empty());

    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(responseStream.write_params.size(), 1);
    ASSERT_EQ(responseStream.write_params[0].msg.messages_list_size(), 1);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    first_msg,
                    responseStream.write_params[0].msg.messages_list(0)
                    )
                    );

    storeMessagesToVector.sendMessage(second_msg);

    ASSERT_EQ(responseStream.write_params.size(), 1);

    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(responseStream.write_params.size(), 2);
    ASSERT_EQ(responseStream.write_params[1].msg.messages_list_size(), 1);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    second_msg,
                    responseStream.write_params[1].msg.messages_list(0)
                    )
                    );

}

TEST_F(StoreAndSendMessages, storeAndSendMessagesToUpdateChatRoom) {

    //Each child has 3 functions that it implements
    // 1) constructor
    // 2) clearMessageList()
    // 3) writeMessageToClient()
    // These are tested through the exposed API.

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> responseStream;

    StoreAndSendMessagesToJoinChatRoom storeMessagesToVector(&responseStream);

    ChatMessageToClient first_msg;
    ChatMessageToClient second_msg;

    first_msg.set_message_uuid(generateUUID());
    second_msg.set_message_uuid(generateUUID());

    storeMessagesToVector.sendMessage(first_msg);

    EXPECT_TRUE(responseStream.write_params.empty());

    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(responseStream.write_params.size(), 1);
    ASSERT_EQ(responseStream.write_params[0].msg.messages_list_size(), 1);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    first_msg,
                    responseStream.write_params[0].msg.messages_list(0)
                    )
                    );

    storeMessagesToVector.sendMessage(second_msg);

    ASSERT_EQ(responseStream.write_params.size(), 1);

    storeMessagesToVector.finalCleanup();

    ASSERT_EQ(responseStream.write_params.size(), 2);
    ASSERT_EQ(responseStream.write_params[1].msg.messages_list_size(), 1);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    second_msg,
                    responseStream.write_params[1].msg.messages_list(0)
                    )
                    );

}

