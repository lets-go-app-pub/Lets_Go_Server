//
// Created by jeremiah on 8/24/22.
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

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "utility_general_functions.h"
#include "generate_multiple_random_accounts.h"
#include "grpc_mock_stream/mock_stream.h"
#include "generate_random_messages.h"
#include "connection_pool_global_variable.h"
#include "chat_room_message_keys.h"
#include "update_single_other_user.h"

#include "request_information_helper_functions.h"
#include "event_request_message_is_valid.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "generate_randoms.h"
#include "event_admin_values.h"
#include "compare_equivalent_messages.h"
#include "UserEventCommands.pb.h"
#include "user_event_commands.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GrpcUpdateChatRoomTesting : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;

    UserAccountDoc generated_account_doc;

    std::string chat_room_id;
    std::string chat_room_name;
    std::string chat_room_password;

    double longitude = chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE;
    double latitude = chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE;

    std::string event_id = chat_room_values::EVENT_ID_DEFAULT;
    std::string qr_code_image_bytes = chat_room_values::QR_CODE_DEFAULT;
    std::string qr_code_message = chat_room_values::QR_CODE_MESSAGE_DEFAULT;
    long qr_code_time_updated = chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT;

    const std::string message_uuid = generateUUID();

    ChatRoomHeaderDoc original_chat_room_header;

    UserPictureDoc thumbnail_picture;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        generated_account_doc.getFromCollection(generated_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                generated_account_oid,
                generated_account_doc.logged_in_token,
                generated_account_doc.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();
        chat_room_name = create_chat_room_response.chat_room_name();
        chat_room_password = create_chat_room_response.chat_room_password();

        original_chat_room_header.getFromCollection(chat_room_id);
        generated_account_doc.getFromCollection(generated_account_oid);

        //get UserPictureDoc object for the generated user's thumbnail
        for (const auto& pic: generated_account_doc.pictures) {
            if (pic.pictureStored()) {
                bsoncxx::oid picture_reference;
                bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

                pic.getPictureReference(
                        picture_reference,
                        timestamp_stored
                );

                thumbnail_picture.getFromCollection(picture_reference);

                break;
            }
        }

        //sleep to guarantee any new timestamps generated are unique
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    //Will store only the admin account. Will store all other information as most recent info.
    void appendSimpleUpdateChatRoomRequest(
            grpc_chat_commands::UpdateChatRoomRequest& update_chat_room_request,
            const std::chrono::milliseconds current_timestamp
            ) {

        setupUserLoginInfo(
                update_chat_room_request.mutable_login_info(),
                generated_account_oid,
                generated_account_doc.logged_in_token,
                generated_account_doc.installation_ids.front()
        );

        update_chat_room_request.set_chat_room_id(chat_room_id);
        update_chat_room_request.set_chat_room_name(chat_room_name);
        update_chat_room_request.set_chat_room_password(chat_room_password);
        update_chat_room_request.set_chat_room_last_activity_time(
                original_chat_room_header.chat_room_last_active_time.value.count());
        update_chat_room_request.set_chat_room_last_updated_time(current_timestamp.count());

        update_chat_room_request.set_pinned_location_longitude(longitude);
        update_chat_room_request.set_pinned_location_latitude(latitude);

        update_chat_room_request.set_qr_code_last_timestamp(qr_code_time_updated);

        update_chat_room_request.set_event_oid(event_id);

        auto current_user = update_chat_room_request.add_chat_room_member_info();
        current_user->set_account_oid(generated_account_oid.to_string());
        current_user->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN);
        current_user->set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
        current_user->set_thumbnail_index_number(thumbnail_picture.picture_index);
        current_user->set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);
        current_user->set_first_name(generated_account_doc.first_name);
        current_user->set_age(generated_account_doc.age);
        current_user->set_member_info_last_updated_timestamp(current_timestamp.count());
        current_user->set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

        int picture_index = 0;
        for (const auto& pic: generated_account_doc.pictures) {
            if (pic.pictureStored()) {
                bsoncxx::oid picture_reference;
                bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

                pic.getPictureReference(
                        picture_reference,
                        timestamp_stored
                );

                auto pic_last_updated = current_user->add_pictures_last_updated_timestamps();
                pic_last_updated->set_index_number(picture_index);
                pic_last_updated->set_last_updated_timestamp(timestamp_stored.value.count());
            }
            picture_index++;
        }
    }

    static void appendEventChatRoomInfoToRequest(
            grpc_chat_commands::UpdateChatRoomRequest& update_chat_room_request,
            const std::chrono::milliseconds& current_timestamp,
            const std::string& event_account_oid
    ) {
        UserAccountDoc event_admin_doc(event_admin_values::OID);

        auto event_admin_user = update_chat_room_request.add_chat_room_member_info();
        event_admin_user->set_account_oid(event_admin_values::OID.to_string());
        event_admin_user->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN);
        event_admin_user->set_first_name(event_admin_values::FIRST_NAME);
        event_admin_user->set_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);
        event_admin_user->set_member_info_last_updated_timestamp(current_timestamp.count());
        event_admin_user->set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

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

                auto pic_last_updated = event_admin_user->add_pictures_last_updated_timestamps();
                pic_last_updated->set_index_number(picture_index);
                pic_last_updated->set_last_updated_timestamp(timestamp_stored.value.count());

                if (!thumbnail_found) {
                    thumbnail_found = true;
                    UserPictureDoc event_thumbnail_picture(picture_reference);

                    event_admin_user->set_thumbnail_size_in_bytes(event_thumbnail_picture.thumbnail_size_in_bytes);
                    event_admin_user->set_thumbnail_index_number(event_thumbnail_picture.picture_index);
                    event_admin_user->set_thumbnail_timestamp(event_thumbnail_picture.timestamp_stored);
                }
            }
            picture_index++;
        }

        UserAccountDoc event_doc(bsoncxx::oid{event_account_oid});

        auto event_account_user = update_chat_room_request.add_chat_room_member_info();
        event_account_user->set_account_oid(event_account_oid);
        event_account_user->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_EVENT);
        event_account_user->set_first_name(event_doc.first_name);
        event_account_user->set_age(event_doc.age);
        event_account_user->set_member_info_last_updated_timestamp(current_timestamp.count());
        event_account_user->set_event_status(LetsGoEventStatus::ONGOING);
        event_account_user->set_event_title(event_doc.event_values->event_title);

        picture_index = 0;
        thumbnail_found = false;
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
    }

    //Will store only the admin account. Will store all other information as most recent info.
    grpc_chat_commands::UpdateChatRoomRequest setupSimpleUpdateChatRoomRequest() {

        const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
        grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request;

        appendSimpleUpdateChatRoomRequest(
                update_chat_room_request,
                current_timestamp
        );

        return update_chat_room_request;
    }

    //Will store only the admin account. Will store all other information as most recent info.
    grpc_chat_commands::UpdateChatRoomRequest setupEventUpdateChatRoomRequest(
            const std::string& event_account_oid
            ) {
        const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
        grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request;

        appendSimpleUpdateChatRoomRequest(
                update_chat_room_request,
                current_timestamp
        );

        appendEventChatRoomInfoToRequest(
                update_chat_room_request,
                current_timestamp,
                event_account_oid
        );

        return update_chat_room_request;
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void checkForValidChatRoomInfoMessage(const grpc_chat_commands::UpdateChatRoomResponse& msg) {
        ASSERT_TRUE(msg.has_chat_room_info());
        EXPECT_EQ(msg.chat_room_info().chat_room_name(), chat_room_name);
        EXPECT_EQ(msg.chat_room_info().chat_room_password(), chat_room_password);
        EXPECT_EQ(msg.chat_room_info().chat_room_last_activity_time(),
                  original_chat_room_header.chat_room_last_active_time);
        EXPECT_EQ(msg.chat_room_info().pinned_location_longitude(), longitude);
        EXPECT_EQ(msg.chat_room_info().pinned_location_latitude(), latitude);
        EXPECT_EQ(msg.chat_room_info().event_oid(), event_id);
    }

    void checkForValidUpdateQrCodeMessage(
            const grpc_chat_commands::UpdateChatRoomResponse& msg,
            bool defaults = false
            ) {
        ASSERT_TRUE(msg.has_qr_code());
        if(defaults) {
            EXPECT_EQ(msg.qr_code().qr_code_image_bytes(), chat_room_values::QR_CODE_DEFAULT);
            EXPECT_EQ(msg.qr_code().qr_code_message(), chat_room_values::QR_CODE_MESSAGE_DEFAULT);
            EXPECT_EQ(msg.qr_code().qr_code_time_updated(), chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT);
        } else {
            EXPECT_EQ(msg.qr_code().qr_code_image_bytes(), qr_code_image_bytes);
            EXPECT_EQ(msg.qr_code().qr_code_message(), qr_code_message);
            EXPECT_EQ(msg.qr_code().qr_code_time_updated(), qr_code_time_updated);
        }
    }

    static void checkForValidUserActivityMessage(const grpc_chat_commands::UpdateChatRoomResponse& msg) {
        ASSERT_TRUE(msg.has_user_activity_message());
        EXPECT_TRUE(msg.user_activity_message().user_exists_in_chat_room());
    }

    static void checkForValidAddendumMessage(
            const grpc_chat_commands::UpdateChatRoomResponse& msg,
            const long user_activity_message_stored_time
    ) {
        ASSERT_TRUE(msg.has_response_addendum());
        EXPECT_EQ(msg.response_addendum().current_timestamp(), user_activity_message_stored_time);
    }

    //Will check if updateChatRoom() properly updated.
    // 1) Message sent (the timestamp of which is last_activity time).
    // 2) Inside chat room header user last activity time of sending user updated.
    // 3) User account document chat room last activity time.
    void checkIfChatRoomProperlyUpdated(const long user_activity_message_stored_time) {

        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
        mongocxx::collection chat_room_collection = chat_rooms_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

        size_t message_doc_exists = chat_room_collection.count_documents(
                document{}
                        << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{
                        std::chrono::milliseconds{user_activity_message_stored_time}}
                        << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kUserActivityDetectedMessage
                        << finalize
        );

        ASSERT_TRUE(message_doc_exists);

        bool found = false;
        for (auto& account: original_chat_room_header.accounts_in_chat_room) {
            if (account.account_oid == generated_account_oid) {
                account.last_activity_time = bsoncxx::types::b_date{
                        std::chrono::milliseconds{user_activity_message_stored_time}};
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);

        ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
        EXPECT_EQ(original_chat_room_header, extracted_chat_room_header);

        found = false;
        for (auto& chat_room: generated_account_doc.chat_rooms) {
            if (chat_room.chat_room_id == chat_room_id) {
                chat_room.last_time_viewed = bsoncxx::types::b_date{
                        std::chrono::milliseconds{user_activity_message_stored_time}};
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);

        UserAccountDoc extracted_account_doc(generated_account_oid);
        EXPECT_EQ(generated_account_doc, extracted_account_doc);
    }

    OtherUserInfoForUpdates insertRandomUserToChatRoom() {
        bsoncxx::oid new_chat_room_user_oid = insertRandomAccounts(1, 0);

        UserAccountDoc new_chat_room_user_oid_doc(new_chat_room_user_oid);

        grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

        setupUserLoginInfo(
                join_chat_room_request.mutable_login_info(),
                new_chat_room_user_oid,
                new_chat_room_user_oid_doc.logged_in_token,
                new_chat_room_user_oid_doc.installation_ids.front()
        );

        join_chat_room_request.set_chat_room_id(chat_room_id);
        join_chat_room_request.set_chat_room_password(chat_room_password);

        grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

        //sleep to guarantee timestamps are unique
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

        joinChatRoom(&join_chat_room_request, &mock_server_writer);

        EXPECT_FALSE(mock_server_writer.write_params.empty());

        if (!mock_server_writer.write_params.empty()) {
            EXPECT_EQ(mock_server_writer.write_params.front().msg.messages_list_size(), 1);
            if (mock_server_writer.write_params.front().msg.messages_list_size() >= 1) {
                EXPECT_EQ(mock_server_writer.write_params.front().msg.messages_list()[0].primer(), true);
                EXPECT_EQ(mock_server_writer.write_params.front().msg.messages_list()[0].return_status(),
                          ReturnStatus::SUCCESS);
            }
        }

        UserPictureDoc new_user_thumbnail_doc;

        //get UserPictureDoc object for the generated user's thumbnail
        for (const auto& pic: new_chat_room_user_oid_doc.pictures) {
            if (pic.pictureStored()) {
                bsoncxx::oid picture_reference;
                bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

                pic.getPictureReference(
                        picture_reference,
                        timestamp_stored
                );

                new_user_thumbnail_doc.getFromCollection(picture_reference);

                break;
            }
        }

        OtherUserInfoForUpdates new_user_for_updates;
        new_user_for_updates.set_account_oid(new_chat_room_user_oid.to_string());
        new_user_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);
        new_user_for_updates.set_thumbnail_size_in_bytes(new_user_thumbnail_doc.thumbnail_size_in_bytes);
        new_user_for_updates.set_thumbnail_index_number(new_user_thumbnail_doc.picture_index);
        new_user_for_updates.set_thumbnail_timestamp(new_user_thumbnail_doc.timestamp_stored);
        new_user_for_updates.set_first_name(new_chat_room_user_oid_doc.first_name);
        new_user_for_updates.set_age(new_chat_room_user_oid_doc.age);
        new_user_for_updates.set_member_info_last_updated_timestamp(getCurrentTimestamp().count());

        return new_user_for_updates;
    }

    void leaveChatRoomWithUser(const std::string& account_oid) {
        grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
        grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

        UserAccountDoc account_doc(bsoncxx::oid{account_oid});

        setupUserLoginInfo(
                leave_chat_room_request.mutable_login_info(),
                bsoncxx::oid{account_oid},
                account_doc.logged_in_token,
                account_doc.installation_ids.front()
        );

        leave_chat_room_request.set_chat_room_id(chat_room_id);

        leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

        EXPECT_EQ(leave_chat_room_response.return_status(), ReturnStatus::SUCCESS);
    }

    static void build_saveUserInfoToMemberSharedInfoMessage_comparison(
            long user_activity_message_stored_time,
            const std::string& user_account_oid,
            const grpc_chat_commands::UpdateChatRoomResponse& update_chat_room_response_msg
    ) {

        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
        mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

        grpc_chat_commands::UpdateChatRoomResponse generated_response;
        UpdateOtherUserResponse* streamResponseForMember = generated_response.mutable_member_response();
        streamResponseForMember->set_timestamp_returned(user_activity_message_stored_time);
        streamResponseForMember->mutable_user_info()->set_account_oid(user_account_oid);

        auto user_account_doc = user_accounts_collection.find_one(
                document{}
                        << "_id" << bsoncxx::oid{user_account_oid}
                        << finalize
        );

        ASSERT_TRUE(user_account_doc);

        if (!saveUserInfoToMemberSharedInfoMessage(mongo_cpp_client, accounts_db,
                                                   user_accounts_collection,
                                                   user_account_doc->view(),
                                                   bsoncxx::oid{user_account_oid},
                                                   streamResponseForMember->mutable_user_info(),
                                                   HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
                                                   std::chrono::milliseconds{user_activity_message_stored_time})
                ) {

            EXPECT_TRUE(false);
        }

        streamResponseForMember->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);
        streamResponseForMember->set_account_last_activity_time(user_activity_message_stored_time);
        streamResponseForMember->set_return_status(ReturnStatus::SUCCESS);

        ASSERT_TRUE(update_chat_room_response_msg.has_member_response());
        generated_response.mutable_member_response()->set_account_last_activity_time(
                update_chat_room_response_msg.member_response().account_last_activity_time());

        compareEquivalentMessages(
                update_chat_room_response_msg,
                generated_response
        );
    }

    void local_build_updateSingleChatRoomMemberNotInChatRoom_comparison(
            long user_activity_message_stored_time,
            const std::string& user_account_oid,
            const ChatRoomHeaderDoc& header_doc,
            const grpc_chat_commands::UpdateChatRoomResponse& update_chat_room_response_msg,
            const OtherUserInfoForUpdates& client_info = buildDummyClientInfo()
    ) {
        ASSERT_TRUE(update_chat_room_response_msg.has_member_response());
        build_updateSingleChatRoomMemberNotInChatRoom_comparison(
                user_activity_message_stored_time,
                user_account_oid,
                chat_room_id,
                header_doc,
                update_chat_room_response_msg.member_response(),
                client_info
        );
    }

    void local_build_updateSingleOtherUser_comparison(
            long user_activity_message_stored_time,
            const std::string& user_account_oid,
            const ChatRoomHeaderDoc& header_doc,
            const grpc_chat_commands::UpdateChatRoomResponse& update_chat_room_response_msg,
            const OtherUserInfoForUpdates& other_user,
            HowToHandleMemberPictures handle_member_pictures = HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO
    ) {
        ASSERT_TRUE(update_chat_room_response_msg.has_member_response());
        build_updateSingleOtherUserOrEvent_comparison(
                user_activity_message_stored_time,
                user_account_oid,
                chat_room_id,
                header_doc,
                update_chat_room_response_msg.member_response(),
                other_user,
                handle_member_pictures
        );
    }

    void local_build_updateSingleOtherEvent_comparison(
            long user_activity_message_stored_time,
            const std::string& user_account_oid,
            const ChatRoomHeaderDoc& header_doc,
            const grpc_chat_commands::UpdateChatRoomResponse& update_chat_room_response_msg,
            const OtherUserInfoForUpdates& other_user,
            HowToHandleMemberPictures handle_member_pictures = HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO
    ) {
        ASSERT_TRUE(update_chat_room_response_msg.has_event_response());
        build_updateSingleOtherUserOrEvent_comparison(
                user_activity_message_stored_time,
                user_account_oid,
                chat_room_id,
                header_doc,
                update_chat_room_response_msg.event_response(),
                other_user,
                handle_member_pictures
        );
    }

    static void local_build_buildBasicUpdateOtherUserResponse_comparison(
            long user_activity_message_stored_time,
            AccountStateInChatRoom user_account_state,
            const std::string& user_account_oid,
            long user_last_activity_time,
            const grpc_chat_commands::UpdateChatRoomResponse& update_chat_room_response_msg
    ) {
        ASSERT_TRUE(update_chat_room_response_msg.has_member_response());
        build_buildBasicUpdateOtherUserResponse_comparison(
                user_activity_message_stored_time,
                user_account_state,
                user_account_oid,
                user_last_activity_time,
                update_chat_room_response_msg.member_response()
        );
    }

    void setupAndJoinEventChatRoom(std::string& event_oid) {

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

        event_oid = event_created_return_values.event_account_oid;

        chat_room_id = event_created_return_values.chat_room_return_info.chat_room_id();
        chat_room_name = event_created_return_values.chat_room_return_info.chat_room_name();
        chat_room_password = event_created_return_values.chat_room_return_info.chat_room_password();

        grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

        setupUserLoginInfo(
                join_chat_room_request.mutable_login_info(),
                generated_account_oid,
                generated_account_doc.logged_in_token,
                generated_account_doc.installation_ids.front()
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

        generated_account_doc.getFromCollection(generated_account_oid);
        original_chat_room_header.getFromCollection(chat_room_id);

        ASSERT_TRUE(original_chat_room_header.event_id.operator bool());
        ASSERT_TRUE(original_chat_room_header.qr_code.operator bool());
        ASSERT_TRUE(original_chat_room_header.qr_code_message.operator bool());
        ASSERT_TRUE(original_chat_room_header.qr_code_time_updated.operator bool());
        ASSERT_TRUE(original_chat_room_header.pinned_location.operator bool());

        event_id = original_chat_room_header.event_id->to_string();

        qr_code_image_bytes = *original_chat_room_header.qr_code;
        qr_code_message = *original_chat_room_header.qr_code_message;
        qr_code_time_updated = original_chat_room_header.qr_code_time_updated->value.count();

        longitude = original_chat_room_header.pinned_location->longitude;
        latitude = original_chat_room_header.pinned_location->latitude;

        //sleep to guarantee any new timestamps generated are unique
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
};

TEST_F(GrpcUpdateChatRoomTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {
                grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request;
                grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

                update_chat_room_request.mutable_login_info()->CopyFrom(login_info);

                update_chat_room_request.set_chat_room_id(chat_room_id);
                update_chat_room_request.set_chat_room_name(chat_room_name);
                update_chat_room_request.set_chat_room_password(chat_room_password);
                update_chat_room_request.set_chat_room_last_activity_time(123);
                update_chat_room_request.set_chat_room_last_updated_time(456);

                updateChatRoom(&update_chat_room_request, &update_chat_room_response);

                EXPECT_EQ(update_chat_room_response.write_params.size(), 1);
                if (!update_chat_room_response.write_params.empty()) {
                    EXPECT_TRUE(update_chat_room_response.write_params[0].msg.has_return_status());
                    if (update_chat_room_response.write_params[0].msg.has_return_status()) {
                        return update_chat_room_response.write_params[0].msg.return_status();
                    }
                }

                return ReturnStatus::UNKNOWN;
            }
    );

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    ChatRoomHeaderDoc original_chat_room_header_after(chat_room_id);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
    EXPECT_EQ(original_chat_room_header_after, original_chat_room_header);
}

TEST_F(GrpcUpdateChatRoomTesting, invalidChatRoomId) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request;
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    setupUserLoginInfo(
            update_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_request.set_chat_room_id("chat_room_id");
    update_chat_room_request.set_chat_room_name(chat_room_name);
    update_chat_room_request.set_chat_room_password(chat_room_password);
    update_chat_room_request.set_chat_room_last_activity_time(123);
    update_chat_room_request.set_chat_room_last_updated_time(456);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 1);
    ASSERT_TRUE(update_chat_room_response.write_params[0].msg.has_return_status());
    EXPECT_EQ(update_chat_room_response.write_params[0].msg.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(GrpcUpdateChatRoomTesting, invalidChatRoomName) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request;
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    setupUserLoginInfo(
            update_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_request.set_chat_room_id(chat_room_id);
    update_chat_room_request.set_chat_room_name(gen_random_alpha_numeric_string(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 + rand() % 100));
    update_chat_room_request.set_chat_room_password(chat_room_password);
    update_chat_room_request.set_chat_room_last_activity_time(123);
    update_chat_room_request.set_chat_room_last_updated_time(456);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 1);
    ASSERT_TRUE(update_chat_room_response.write_params[0].msg.has_return_status());
    EXPECT_EQ(update_chat_room_response.write_params[0].msg.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(GrpcUpdateChatRoomTesting, invalidChatRoomPassword) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request;
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    setupUserLoginInfo(
            update_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_request.set_chat_room_id(chat_room_id);
    update_chat_room_request.set_chat_room_name(chat_room_name);
    update_chat_room_request.set_chat_room_password(gen_random_alpha_numeric_string(
            server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 + rand() % 100));
    update_chat_room_request.set_chat_room_last_activity_time(123);
    update_chat_room_request.set_chat_room_last_updated_time(456);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 1);
    ASSERT_TRUE(update_chat_room_response.write_params[0].msg.has_return_status());
    EXPECT_EQ(update_chat_room_response.write_params[0].msg.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(GrpcUpdateChatRoomTesting, invalidChatRoomLastActivityTime) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    update_chat_room_request.set_chat_room_last_activity_time(-123785);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    long user_activity_message_stored_time = update_chat_room_response.write_params[1].msg.user_activity_message().timestamp_returned();

    checkForValidChatRoomInfoMessage(update_chat_room_response.write_params[0].msg);
    checkForValidUserActivityMessage(update_chat_room_response.write_params[1].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, invalidChatRoomLastUpdatedTime) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    update_chat_room_request.set_chat_room_last_updated_time(166144505000000);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 2);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[1].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, requiredUpdate_chatRoomName) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    update_chat_room_request.set_chat_room_name(chat_room_name + 'a');

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    long user_activity_message_stored_time = update_chat_room_response.write_params[1].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[1].msg);
    checkForValidChatRoomInfoMessage(update_chat_room_response.write_params[0].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, requiredUpdate_chatRoomPassword) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    update_chat_room_request.set_chat_room_name(chat_room_password + 'a');

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    long user_activity_message_stored_time = update_chat_room_response.write_params[1].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[1].msg);
    checkForValidChatRoomInfoMessage(update_chat_room_response.write_params[0].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);

    checkIfChatRoomProperlyUpdated(
            update_chat_room_response.write_params[1].msg.user_activity_message().timestamp_returned());
}

TEST_F(GrpcUpdateChatRoomTesting, requiredUpdate_chatRoomLastActivityTime) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    update_chat_room_request.set_chat_room_last_activity_time(original_chat_room_header.chat_room_last_active_time - 1);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    long user_activity_message_stored_time = update_chat_room_response.write_params[1].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[1].msg);
    checkForValidChatRoomInfoMessage(update_chat_room_response.write_params[0].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, requiredUpdate_missingMessages) {

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    //Sleep to guarantee message timestamps are after the last_updated_time passed to server.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    auto [text_message_request, text_message_response] = generateRandomTextMessage(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID(),
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            )
    );

    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

    //Sleep to guarantee message timestamps are different and so are sent back in a predictable order.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    auto [picture_message_request, picture_message_response] = generateRandomPictureMessage(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
            chat_room_id,
            generateUUID(),
            gen_random_alpha_numeric_string(
                    rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE + 1
            )
    );

    EXPECT_EQ(picture_message_response.return_status(), ReturnStatus::SUCCESS);
    original_chat_room_header.chat_room_last_active_time = bsoncxx::types::b_date{
            std::chrono::milliseconds{picture_message_response.timestamp_stored()}};

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 4);

    long user_activity_message_stored_time = update_chat_room_response.write_params[2].msg.user_activity_message().timestamp_returned();

    checkForValidChatRoomInfoMessage(update_chat_room_response.write_params[0].msg);

    ASSERT_TRUE(update_chat_room_response.write_params[1].msg.has_chat_messages_list());
    ASSERT_EQ(update_chat_room_response.write_params[1].msg.chat_messages_list().messages_list_size(), 2);

    EXPECT_EQ(update_chat_room_response.write_params[1].msg.chat_messages_list().messages_list(0).message_uuid(),
              text_message_request.message_uuid());
    EXPECT_EQ(update_chat_room_response.write_params[1].msg.chat_messages_list().messages_list(1).message_uuid(),
              picture_message_request.message_uuid());

    checkForValidUserActivityMessage(update_chat_room_response.write_params[2].msg);

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[3].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, requiredUpdate_location) {

    grpc_chat_commands::SetPinnedLocationRequest set_pinned_location_request;
    grpc_chat_commands::SetPinnedLocationResponse set_pinned_location_response;

    setupUserLoginInfo(
            set_pinned_location_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    //10e4 will give 4 decimal places of precision (NOTE: This method will not work for more because of
    // the precision of doubles).
    int precision = 10e4;

    //latitude must be a number -90 to 90
    double generated_latitude = rand() % (90*precision);
    if(rand() % 2) {
        generated_latitude *= -1;
    }
    generated_latitude /= precision;

    //longitude must be a number -180 to 180
    double generated_longitude = rand() % (180*precision);
    if(rand() % 2) {
        generated_longitude *= -1;
    }
    generated_longitude /= precision;

    set_pinned_location_request.set_chat_room_id(chat_room_id);

    set_pinned_location_request.set_new_pinned_longitude(generated_longitude);
    set_pinned_location_request.set_new_pinned_latitude(generated_latitude);

    set_pinned_location_request.set_message_uuid(generateUUID());

    setPinnedLocation(
            &set_pinned_location_request,
            &set_pinned_location_response
            );

    ASSERT_EQ(set_pinned_location_response.return_status(), ReturnStatus::SUCCESS);

    //Values updated after setPinnedLocation() run.
    original_chat_room_header.getFromCollection(chat_room_id);

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    //This must be set after the request for updateChatRoom() is setup.
    longitude = generated_longitude;
    latitude = generated_latitude;

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    long user_activity_message_stored_time = update_chat_room_response.write_params[1].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[1].msg);
    checkForValidChatRoomInfoMessage(update_chat_room_response.write_params[0].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, requiredUpdate_eventOid) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    update_chat_room_request.set_event_oid(chat_room_values::EVENT_ID_DEFAULT + "a");

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    long user_activity_message_stored_time = update_chat_room_response.write_params[1].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[1].msg);
    checkForValidChatRoomInfoMessage(update_chat_room_response.write_params[0].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, requiredUpdate_qrCodeRequiresUpdate) {
    std::string event_oid;

    setupAndJoinEventChatRoom(event_oid);

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupEventUpdateChatRoomRequest(event_oid);
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    //Request qr code update.
    update_chat_room_request.set_qr_code_last_timestamp(-1);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 4);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);
    checkForValidUpdateQrCodeMessage(update_chat_room_response.write_params[2].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[3].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, eventOnClient_eventValid_requiresUpdate_eventStatus) {
    std::string event_oid;

    setupAndJoinEventChatRoom(event_oid);

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupEventUpdateChatRoomRequest(event_oid);
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    //Set event_status of event to be incorrect.
    update_chat_room_request.mutable_chat_room_member_info(2)->set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 4);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    local_build_updateSingleOtherEvent_comparison(
            user_activity_message_stored_time,
            event_oid,
            ChatRoomHeaderDoc(chat_room_id),
            update_chat_room_response.write_params[2].msg,
            update_chat_room_request.chat_room_member_info(2)
    );

    //checkForValidUpdateQrCodeMessage(update_chat_room_response.write_params[2].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[3].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, eventOnClient_eventValid_requiresUpdate_eventTitle) {
    std::string event_oid;

    setupAndJoinEventChatRoom(event_oid);

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupEventUpdateChatRoomRequest(event_oid);
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    //Set event_title of event to be incorrect.
    update_chat_room_request.mutable_chat_room_member_info(2)->set_event_title("");

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 4);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    local_build_updateSingleOtherEvent_comparison(
            user_activity_message_stored_time,
            event_oid,
            ChatRoomHeaderDoc(chat_room_id),
            update_chat_room_response.write_params[2].msg,
            update_chat_room_request.chat_room_member_info(2)
    );

    //checkForValidUpdateQrCodeMessage(update_chat_room_response.write_params[2].msg);
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[3].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, eventOnClient_eventCanceled) {
    std::string event_oid;

    setupAndJoinEventChatRoom(event_oid);

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupEventUpdateChatRoomRequest(event_oid);
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    user_event_commands::CancelEventRequest cancel_event_request;
    user_event_commands::CancelEventResponse cancel_event_response;

    createTempAdminAccount(AdminLevelEnum::PRIMARY_DEVELOPER);

    setupAdminLoginInfo(
            cancel_event_request.mutable_login_info(),
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    cancel_event_request.set_event_oid(event_oid);

    cancelEvent(&cancel_event_request, &cancel_event_response);

    ASSERT_EQ(cancel_event_response.return_status(), ReturnStatus::SUCCESS);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    //if a qr code was not generated, then no qr code exists to have a qr code update message sent back.
    const bool qr_code_requires_update = qr_code_time_updated != chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT;

    int event_index;
    int addendum_index;
    if(qr_code_requires_update) {
        event_index = 3;
        addendum_index = 4;
        ASSERT_EQ(update_chat_room_response.write_params.size(), 5);
        checkForValidUpdateQrCodeMessage(update_chat_room_response.write_params[2].msg, true);
    } else {
        event_index = 2;
        addendum_index = 3;
        ASSERT_EQ(update_chat_room_response.write_params.size(), 4);
    }

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    local_build_updateSingleOtherEvent_comparison(
            user_activity_message_stored_time,
            event_oid,
            ChatRoomHeaderDoc(chat_room_id),
            update_chat_room_response.write_params[event_index].msg,
            update_chat_room_request.chat_room_member_info(2),
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[addendum_index].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, eventOnClient_eventExpired) {
    std::string event_oid;

    setupAndJoinEventChatRoom(event_oid);

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupEventUpdateChatRoomRequest(event_oid);
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    //Set event to expired
    UserAccountDoc event_account(bsoncxx::oid{event_oid});
    event_account.event_expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() - std::chrono::milliseconds{1000000}};
    event_account.setIntoCollection();

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    //if a qr code was not generated, then no qr code exists to have a qr code update message sent back.
    const bool qr_code_requires_update = qr_code_time_updated != chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT;

    int event_index;
    int addendum_index;
    if(qr_code_requires_update) {
        event_index = 3;
        addendum_index = 4;
        ASSERT_EQ(update_chat_room_response.write_params.size(), 5);
        checkForValidUpdateQrCodeMessage(update_chat_room_response.write_params[2].msg, true);
    } else {
        event_index = 2;
        addendum_index = 3;
        ASSERT_EQ(update_chat_room_response.write_params.size(), 4);
    }

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    local_build_updateSingleOtherEvent_comparison(
            user_activity_message_stored_time,
            event_oid,
            ChatRoomHeaderDoc(chat_room_id),
            update_chat_room_response.write_params[event_index].msg,
            update_chat_room_request.chat_room_member_info(2),
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[addendum_index].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, eventOnClient_eventAdminRequiresUpdate) {
    std::string event_oid;

    setupAndJoinEventChatRoom(event_oid);

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupEventUpdateChatRoomRequest(event_oid);
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    //Set account_state of event admin to be incorrect.
    update_chat_room_request.mutable_chat_room_member_info(1)->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 4);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    local_build_updateSingleOtherUser_comparison(
            user_activity_message_stored_time,
            event_admin_values::OID.to_string(),
            original_chat_room_header,
            update_chat_room_response.write_params[2].msg,
            update_chat_room_request.chat_room_member_info(1)
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[3].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, memberInHeader_memberOnClient_notCurrentUser_accountStateInChatRoom) {
    //stored as REQUIRES_UPDATE in updateChatRoom()

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    OtherUserInfoForUpdates other_user = insertRandomUserToChatRoom();

    //Sleep so getCurrentTimestamp() result is guaranteed to not request any messages.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    original_chat_room_header.getFromCollection(chat_room_id);

    update_chat_room_request.set_chat_room_last_activity_time(original_chat_room_header.chat_room_last_active_time);
    update_chat_room_request.set_chat_room_last_updated_time(getCurrentTimestamp().count());

    update_chat_room_request.mutable_chat_room_member_info()->Add(OtherUserInfoForUpdates{other_user});

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    local_build_updateSingleOtherUser_comparison(
            user_activity_message_stored_time,
            other_user.account_oid(),
            ChatRoomHeaderDoc(chat_room_id),
            update_chat_room_response.write_params[1].msg,
            other_user
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, memberInHeader_memberOnClient_notCurrentUser_accountStateNotInChatRoom) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    OtherUserInfoForUpdates other_user = insertRandomUserToChatRoom();

    leaveChatRoomWithUser(other_user.account_oid());

    //Sleep so getCurrentTimestamp() result is guaranteed to not request any messages.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    original_chat_room_header.getFromCollection(chat_room_id);

    update_chat_room_request.set_chat_room_last_activity_time(original_chat_room_header.chat_room_last_active_time);
    update_chat_room_request.set_chat_room_last_updated_time(getCurrentTimestamp().count());

    update_chat_room_request.mutable_chat_room_member_info()->Add(OtherUserInfoForUpdates{other_user});

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    local_build_updateSingleChatRoomMemberNotInChatRoom_comparison(
            user_activity_message_stored_time,
            other_user.account_oid(),
            ChatRoomHeaderDoc(chat_room_id),
            update_chat_room_response.write_params[1].msg,
            other_user
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, memberInHeader_memberOnClient_currentUser_accountStateDoesNotMatch) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    update_chat_room_request.mutable_chat_room_member_info(0)->set_account_state(
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    local_build_buildBasicUpdateOtherUserResponse_comparison(
            user_activity_message_stored_time,
            AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
            generated_account_oid.to_string(),
            user_activity_message_stored_time,
            update_chat_room_response.write_params[1].msg
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, memberNotInHeader_memberOnClient_notCurrentUser_accountStateInChatRoom) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    auto current_user = update_chat_room_request.add_chat_room_member_info();
    current_user->set_account_oid(bsoncxx::oid{}.to_string());
    current_user->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);
    current_user->set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    current_user->set_thumbnail_index_number(thumbnail_picture.picture_index);
    current_user->set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);
    current_user->set_first_name(generated_account_doc.first_name);
    current_user->set_age(generated_account_doc.age);
    current_user->set_member_info_last_updated_timestamp(getCurrentTimestamp().count());

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    ASSERT_TRUE(update_chat_room_response.write_params[1].msg.has_member_response());
    ASSERT_EQ(update_chat_room_response.write_params[1].msg.member_response().account_state(),
              AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM);
    ASSERT_EQ(update_chat_room_response.write_params[1].msg.member_response().user_info().account_oid(),
              current_user->account_oid());
    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, memberNotInHeader_memberOnClient_notCurrentUser_accountStateNotInChatRoom) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    auto current_user = update_chat_room_request.add_chat_room_member_info();
    current_user->set_account_oid(bsoncxx::oid{}.to_string());
    current_user->set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM);
    current_user->set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    current_user->set_thumbnail_index_number(thumbnail_picture.picture_index);
    current_user->set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);
    current_user->set_first_name(generated_account_doc.first_name);
    current_user->set_age(generated_account_doc.age);
    current_user->set_member_info_last_updated_timestamp(getCurrentTimestamp().count());

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 2);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[1].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, memberNotInHeader_memberOnClient_currentUser) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    original_chat_room_header.accounts_in_chat_room.pop_back();
    original_chat_room_header.setIntoCollection();

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 1);
    ASSERT_TRUE(update_chat_room_response.write_params[0].msg.has_user_activity_message());
    EXPECT_EQ(update_chat_room_response.write_params[0].msg.user_activity_message().user_exists_in_chat_room(), false);
}

TEST_F(GrpcUpdateChatRoomTesting, memberInHeader_memberNotOnClient_notCurrentUser_accountStateInChatRoom) {
    //stored as REQUIRES_ADDED in updateChatRoom()

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    OtherUserInfoForUpdates other_user = insertRandomUserToChatRoom();

    //Sleep so getCurrentTimestamp() result is guaranteed to not request any messages.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    original_chat_room_header.getFromCollection(chat_room_id);

    update_chat_room_request.set_chat_room_last_activity_time(original_chat_room_header.chat_room_last_active_time);
    update_chat_room_request.set_chat_room_last_updated_time(getCurrentTimestamp().count());

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    build_saveUserInfoToMemberSharedInfoMessage_comparison(
            user_activity_message_stored_time,
            other_user.account_oid(),
            update_chat_room_response.write_params[1].msg
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, memberInHeader_memberNotOnClient_notCurrentUser_accountStateNotInChatRoom) {

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    OtherUserInfoForUpdates other_user = insertRandomUserToChatRoom();

    leaveChatRoomWithUser(other_user.account_oid());

    //Sleep so getCurrentTimestamp() result is guaranteed to not request any messages.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    original_chat_room_header.getFromCollection(chat_room_id);

    update_chat_room_request.set_chat_room_last_activity_time(original_chat_room_header.chat_room_last_active_time);
    update_chat_room_request.set_chat_room_last_updated_time(getCurrentTimestamp().count());

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    local_build_updateSingleChatRoomMemberNotInChatRoom_comparison(
            user_activity_message_stored_time,
            other_user.account_oid(),
            ChatRoomHeaderDoc(chat_room_id),
            update_chat_room_response.write_params[1].msg
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, memberInHeader_memberNotOnClient_currentUser) {
    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    update_chat_room_request.mutable_chat_room_member_info()->erase(
            update_chat_room_request.mutable_chat_room_member_info()->begin()
    );

    //Sleep so getCurrentTimestamp() result is guaranteed to not request any messages.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    local_build_buildBasicUpdateOtherUserResponse_comparison(
            user_activity_message_stored_time,
            AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
            generated_account_oid.to_string(),
            user_activity_message_stored_time,
            update_chat_room_response.write_params[1].msg
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, memberUpdatesRequested_userAccountDocumentDoesNotExist) {

    //This test is TEST_F(memberInHeader_memberNotOnClient_notCurrentUser_accountStateInChatRoom) with no
    // user account document.

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    OtherUserInfoForUpdates other_user = insertRandomUserToChatRoom();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    auto delete_result = user_accounts_collection.delete_one(
            document{}
                    << "_id" << bsoncxx::oid{other_user.account_oid()}
                    << finalize
    );

    ASSERT_TRUE(delete_result);
    EXPECT_EQ(delete_result->deleted_count(), 1);

    //Sleep so getCurrentTimestamp() result is guaranteed to not request any messages.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    original_chat_room_header.getFromCollection(chat_room_id);

    update_chat_room_request.set_chat_room_last_activity_time(original_chat_room_header.chat_room_last_active_time);
    update_chat_room_request.set_chat_room_last_updated_time(getCurrentTimestamp().count());

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 3);

    checkForValidUserActivityMessage(update_chat_room_response.write_params[0].msg);

    long user_activity_message_stored_time = update_chat_room_response.write_params[0].msg.user_activity_message().timestamp_returned();

    local_build_buildBasicUpdateOtherUserResponse_comparison(
            user_activity_message_stored_time,
            AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
            other_user.account_oid(),
            -1,
            update_chat_room_response.write_params[1].msg
    );

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[2].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}

TEST_F(GrpcUpdateChatRoomTesting, multipleMembers_variousAccountStates_requireDifferentUpdates) {

    //This is not meant to be complete here. Looking to make sure that larger numbers of members work correctly
    // AND that howToHandleMemberPictures is properly utilized for more than MAXIMUM_NUMBER_USERS_IN_CHAT_ROOM_TO_REQUEST_ALL_INFO.

    grpc_chat_commands::UpdateChatRoomRequest update_chat_room_request = setupSimpleUpdateChatRoomRequest();
    grpc::testing::MockServerWriterVector<grpc_chat_commands::UpdateChatRoomResponse> update_chat_room_response;

    //pair<account_oid, function to check user>
    std::unordered_map<
            std::string,
            std::function<void(
                    const grpc_chat_commands::UpdateChatRoomResponse& /*msg*/,
                    long /*user_activity_message_stored_time*/)>
    > run_checks_on_users;

    unsigned long number_user_in_chat_room = 0;
    size_t number_users_to_join = 0;

    for (size_t i = 0;
         number_user_in_chat_room <= chat_room_values::MAXIMUM_NUMBER_USERS_IN_CHAT_ROOM_TO_REQUEST_ALL_INFO * 2; ++i) {

        OtherUserInfoForUpdates other_user = insertRandomUserToChatRoom();

        if (rand() % 2) {
            leaveChatRoomWithUser(other_user.account_oid());

            //Sleep so getCurrentTimestamp() result is guaranteed to not request any messages. A
            std::this_thread::sleep_for(std::chrono::milliseconds{10});

            other_user.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);
            other_user.set_member_info_last_updated_timestamp(getCurrentTimestamp().count());

            update_chat_room_request.mutable_chat_room_member_info()->Add(OtherUserInfoForUpdates{other_user});

            run_checks_on_users.insert(
                    std::make_pair<std::string, std::function<void(
                            const grpc_chat_commands::UpdateChatRoomResponse& /*msg*/,
                            long /*user_activity_message_stored_time*/)>
                    >(
                            std::string(other_user.account_oid()),
                            [this, _other_user = other_user](
                                    const grpc_chat_commands::UpdateChatRoomResponse& msg,
                                    long user_activity_message_stored_time
                            ) {
                                local_build_updateSingleChatRoomMemberNotInChatRoom_comparison(
                                        user_activity_message_stored_time,
                                        _other_user.account_oid(),
                                        ChatRoomHeaderDoc(chat_room_id),
                                        msg,
                                        _other_user
                                );
                            }
                    )
            );

        } else {

            ++number_user_in_chat_room;

            update_chat_room_request.mutable_chat_room_member_info()->Add(OtherUserInfoForUpdates{other_user});

            run_checks_on_users.insert(
                    std::make_pair<std::string, std::function<void(
                            const grpc_chat_commands::UpdateChatRoomResponse& /*msg*/,
                            long /*user_activity_message_stored_time*/)>
                    >(
                            std::string(other_user.account_oid()),
                            [this, _other_user = other_user](
                                    const grpc_chat_commands::UpdateChatRoomResponse& msg,
                                    long user_activity_message_stored_time
                            )mutable {
                                local_build_updateSingleOtherUser_comparison(
                                        user_activity_message_stored_time,
                                        _other_user.account_oid(),
                                        ChatRoomHeaderDoc(chat_room_id),
                                        msg,
                                        _other_user,
                                        HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL
                                );
                            }
                    )
            );
        }

        //Sleep to guarantee unique timestamps.
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        number_users_to_join++;
    }

    original_chat_room_header.getFromCollection(chat_room_id);

    update_chat_room_request.set_chat_room_last_activity_time(original_chat_room_header.chat_room_last_active_time);
    update_chat_room_request.set_chat_room_last_updated_time(getCurrentTimestamp().count());

    updateChatRoom(&update_chat_room_request, &update_chat_room_response);

    ASSERT_EQ(update_chat_room_response.write_params.size(), 2 + number_users_to_join);

    int curr_index = 0;

    checkForValidUserActivityMessage(update_chat_room_response.write_params[curr_index].msg);

    long user_activity_message_stored_time = update_chat_room_response.write_params[curr_index].msg.user_activity_message().timestamp_returned();

    ++curr_index;

    for (size_t i = 0; i < number_users_to_join; ++i) {
        ASSERT_TRUE(update_chat_room_response.write_params[curr_index].msg.has_member_response());

        auto ptr = run_checks_on_users.find(
                update_chat_room_response.write_params[curr_index].msg.member_response().user_info().account_oid());
        EXPECT_NE(ptr, run_checks_on_users.end());
        if (ptr != run_checks_on_users.end()) {
            ptr->second(update_chat_room_response.write_params[curr_index].msg, user_activity_message_stored_time);
        }
        ++curr_index;
    }

    checkForValidAddendumMessage(
            update_chat_room_response.write_params[curr_index].msg,
            user_activity_message_stored_time
    );

    checkIfChatRoomProperlyUpdated(user_activity_message_stored_time);
}


