//
// Created by jeremiah on 3/17/23.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "helper_functions/user_match_options_helper_functions.h"
#include "errors_objects.h"
#include "EventRequestMessage.pb.h"
#include "event_request_message_is_valid.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "generate_randoms.h"
#include "UserEventCommands.pb.h"
#include "setup_login_info.h"
#include "user_event_commands.h"
#include "grpc_mock_stream/mock_stream.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class HandleUserSwipedYesOnEventTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid event_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc event_account_doc;

    UserAccountStatisticsDoc user_account_statistics_doc;
    UserAccountStatisticsDoc event_account_statistics_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bsoncxx::builder::stream::document user_document_builder;

    bsoncxx::builder::stream::document user_extracted_array_builder;
    std::optional<bsoncxx::document::view> user_extracted_array_view;

    int testing_result = 0;

    EventRequestMessage event_info;

    CreateEventReturnValues event_created_return_values;

    std::string chat_room_id;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        createAndStoreEventAdminAccount();

        const EventChatRoomAdminInfo chat_room_admin_info(
                UserAccountType::ADMIN_GENERATED_EVENT_TYPE
        );

        event_info = generateRandomEventRequestMessage(
                chat_room_admin_info,
                current_timestamp
        );

        //Make sure event can be matched by user.
        event_info.clear_allowed_genders();
        event_info.add_allowed_genders(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);

        event_info.set_min_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
        event_info.set_max_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);

        event_created_return_values = generateRandomEvent(
                chat_room_admin_info,
                TEMP_ADMIN_ACCOUNT_NAME,
                current_timestamp,
                event_info
        );

        chat_room_id = event_created_return_values.chat_room_return_info.chat_room_id();

        event_account_oid = bsoncxx::oid{event_created_return_values.event_account_oid};
        event_account_doc.getFromCollection(event_account_oid);

        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        user_account_statistics_doc.getFromCollection(user_account_oid);
        event_account_statistics_doc.getFromCollection(event_account_oid);

        user_account_doc.convertToDocument(user_document_builder);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

        mongocxx::client_session::with_transaction_cb transactionCallback = [&](
                mongocxx::client_session* callback_session
        ) {
            bool return_result = handleUserSwipedYesOnEvent(
                    mongo_cpp_client,
                    accounts_db,
                    user_accounts_collection,
                    chat_room_collection,
                    callback_session,
                    user_document_builder.view(),
                    chat_room_id,
                    event_account_oid,
                    user_account_oid,
                    current_timestamp,
                    testing_result
            );

            EXPECT_TRUE(return_result);
        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        //NOTE: this must go before the code below, this will block for the transaction to finish
        try {
            session.with_transaction(transactionCallback);
        } catch (const std::exception& e) {
            EXPECT_EQ(std::string(e.what()), "dummy_string");
        }
    }

    void checkUserAccounts() {
        UserAccountDoc extracted_event_account_doc(event_account_oid);
        EXPECT_EQ(event_account_doc, extracted_event_account_doc);

        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

    void checkUserAccountStatistics() {
        UserAccountStatisticsDoc extracted_event_account_doc(event_account_oid);
        EXPECT_EQ(event_account_statistics_doc, extracted_event_account_doc);

        UserAccountStatisticsDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_statistics_doc, extracted_user_account_doc);
    }
};

TEST_F(HandleUserSwipedYesOnEventTesting, nullSessionPassed) {
    mongocxx::collection chat_room_collection = accounts_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    bool return_result = handleUserSwipedYesOnEvent(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            chat_room_collection,
            nullptr,
            user_document_builder.view(),
            chat_room_id,
            event_account_oid,
            user_account_oid,
            current_timestamp,
            testing_result
    );

    EXPECT_FALSE(return_result);
    EXPECT_EQ(testing_result, 0);

    checkUserAccounts();
}

TEST_F(HandleUserSwipedYesOnEventTesting, eventIsValid) {

    runFunction();

    EXPECT_EQ(testing_result, 4);

    std::chrono::milliseconds inserted_timestamp;

    UserAccountDoc extracted_user_account_doc(user_account_oid);
    EXPECT_EQ(extracted_user_account_doc.chat_rooms.size(), 1);
    if(extracted_user_account_doc.chat_rooms.size() == 1) {
        inserted_timestamp = extracted_user_account_doc.chat_rooms[0].last_time_viewed.value;
        user_account_doc.chat_rooms.emplace_back(
            chat_room_id,
            extracted_user_account_doc.chat_rooms[0].last_time_viewed,
            event_account_oid
        );
    }

    checkUserAccounts();

    user_account_statistics_doc.times_match_occurred.emplace_back(
            event_account_oid,
            chat_room_id,
            std::optional<MatchType>(MatchType::EVENT_MATCH),
            bsoncxx::types::b_date{inserted_timestamp}
    );

    event_account_statistics_doc.times_match_occurred.emplace_back(
            user_account_oid,
            chat_room_id,
            std::optional<MatchType>(MatchType::EVENT_MATCH),
            bsoncxx::types::b_date{inserted_timestamp}
    );

    checkUserAccountStatistics();
}

TEST_F(HandleUserSwipedYesOnEventTesting, eventIsCanceled) {

    createTempAdminAccount(AdminLevelEnum::PRIMARY_DEVELOPER);

    user_event_commands::CancelEventRequest request;
    user_event_commands::CancelEventResponse response;

    setupAdminLoginInfo(
            request.mutable_login_info(),
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD
    );

    request.set_event_oid(event_account_oid.to_string());

    cancelEvent(&request, &response);

    ASSERT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    //Get canceled event info from database.
    event_account_doc.getFromCollection(event_account_oid);

    runFunction();

    EXPECT_EQ(testing_result, 2);

    checkUserAccounts();

    checkUserAccountStatistics();
}

TEST_F(HandleUserSwipedYesOnEventTesting, eventIsExpired) {
    event_account_doc.event_expiration_time = bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{100000}};
    event_account_doc.setIntoCollection();

    runFunction();

    EXPECT_EQ(testing_result, 2);

    checkUserAccounts();

    checkUserAccountStatistics();
}

TEST_F(HandleUserSwipedYesOnEventTesting, userAlreadyInsideChatRoom) {

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
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

    //Request user account doc b/c user joined chat room.
    user_account_doc.getFromCollection(user_account_oid);

    runFunction();

    EXPECT_EQ(testing_result, 3);

    checkUserAccounts();

    checkUserAccountStatistics();
}

TEST_F(HandleUserSwipedYesOnEventTesting, eventNoLongerMatches) {
    event_account_doc.genders_range.clear();
    event_account_doc.genders_range.emplace_back(
            general_values::MALE_GENDER_VALUE
            );
    event_account_doc.setIntoCollection();

    user_account_doc.gender = general_values::FEMALE_GENDER_VALUE;
    user_account_doc.setIntoCollection();
    user_document_builder.clear();
    user_account_doc.convertToDocument(user_document_builder);

    runFunction();

    EXPECT_EQ(testing_result, 2);

    checkUserAccounts();

    checkUserAccountStatistics();
}
