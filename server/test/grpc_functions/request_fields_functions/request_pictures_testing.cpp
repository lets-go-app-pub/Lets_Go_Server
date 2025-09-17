//
// Created by jeremiah on 10/4/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "chat_room_shared_keys.h"
#include "setup_login_info.h"
#include "connection_pool_global_variable.h"
#include "ManageServerCommands.pb.h"
#include "request_fields_functions.h"
#include "generate_multiple_random_accounts.h"
#include "grpc_mock_stream/mock_stream.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RequestPicturesTesting : public ::testing::Test {
protected:

    request_fields::PictureRequest request;
    grpc::testing::MockServerWriterVector<request_fields::PictureResponse> response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    void setupValidRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; ++i) {
            request.add_requested_indexes(i);
        }
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(user_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        requestPictures(&request, &response);
    }

    void comparePictureAtIndex(const int index) {
        request_fields::PictureResponse single_picture_response;

        if(user_account.pictures[index].pictureStored()) {
            bsoncxx::oid pic_reference;
            bsoncxx::types::b_date pic_timestamp{std::chrono::milliseconds{-1}};

            user_account.pictures[index].getPictureReference(
                    pic_reference,
                    pic_timestamp
            );

            UserPictureDoc user_picture(pic_reference);

            single_picture_response.mutable_picture_info()->set_file_size(user_picture.picture_size_in_bytes);
            single_picture_response.mutable_picture_info()->set_file_in_bytes(user_picture.picture_in_bytes);
            single_picture_response.mutable_picture_info()->set_index_number(index);
            single_picture_response.mutable_picture_info()->set_timestamp_picture_last_updated(pic_timestamp.value.count());
            single_picture_response.set_return_status(SUCCESS);
        } else {
            single_picture_response.mutable_picture_info()->set_file_size(0);
            single_picture_response.mutable_picture_info()->set_file_in_bytes("");
            single_picture_response.mutable_picture_info()->set_index_number(index);
            single_picture_response.set_return_status(SUCCESS);
        }

        EXPECT_GT(response.write_params[index].msg.timestamp(), 0);
        single_picture_response.set_timestamp(response.write_params[index].msg.timestamp());

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                single_picture_response,
                response.write_params[index].msg
        );

        if(!equivalent) {

            //remove the byte strings for printing
            single_picture_response.mutable_picture_info()->clear_file_in_bytes();
            auto response_copy = response.write_params[index].msg;
            response_copy.mutable_picture_info()->clear_file_in_bytes();

            std::cout << "single_picture_response\n" << single_picture_response.DebugString() << '\n';
            std::cout << "response\n" << response_copy.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }
};

TEST_F(RequestPicturesTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            user_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                request.mutable_login_info()->CopyFrom(login_info);

                response.write_params.clear();

                runFunction();

                EXPECT_EQ(response.write_params.size(), 1);
                if(response.write_params.size() == 1) {
                    return response.write_params[0].msg.return_status();
                } else {
                    return ReturnStatus::UNKNOWN;
                }
            }
    );
}

TEST_F(RequestPicturesTesting, noIndexesPassed) {
    request.clear_requested_indexes();

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    EXPECT_EQ(response.write_params[0].msg.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(RequestPicturesTesting, tooManyIndexesPassed) {
    request.clear_requested_indexes();

    for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT + 1; ++i) {
        request.add_requested_indexes(i);
    }

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    EXPECT_EQ(response.write_params[0].msg.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(RequestPicturesTesting, invalidIndexPassed) {
    request.clear_requested_indexes();

    request.add_requested_indexes(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT);

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    EXPECT_EQ(response.write_params[0].msg.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(RequestPicturesTesting, requestSinglePicture) {
    const int requested_index = 0;

    request.clear_requested_indexes();
    request.add_requested_indexes(requested_index);

    runFunction();

    ASSERT_EQ(1, response.write_params.size());

    comparePictureAtIndex(requested_index);
}

TEST_F(RequestPicturesTesting, requestAllPictures) {
    runFunction();

    ASSERT_EQ(user_account.pictures.size(), response.write_params.size());

    //There is no guarantee of order in returned pictures, sort them by index.
    std::sort(response.write_params.begin(), response.write_params.end(), [](
            const auto& l, const auto& r
            ) -> bool {
        return l.msg.picture_info().index_number() <  r.msg.picture_info().index_number();
    });

    for(int i = 0; i < (int)user_account.pictures.size(); ++i) {
        comparePictureAtIndex(i);
    }
}

