//
// Created by jeremiah on 10/3/22.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <gtest/gtest.h>
#include "collection_objects/deleted_objects/deleted_objects.h"
#include "utility_general_functions_test.h"
#include "connection_pool_global_variable.h"
#include "setup_login_info.h"
#include "chat_room_commands.h"
#include "generate_randoms.h"
#include "move_user_account_statistics_document_test.h"
#include "report_helper_functions_test.h"
#include "utility_chat_functions_test.h"
#include "build_match_made_chat_room.h"
#include "generate_multiple_random_accounts.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void buildAndCheckDeleteAccount(
        const bsoncxx::oid& generated_account_oid,
        const std::function<bool(std::chrono::milliseconds& /*current_timestamp*/)>& run_delete
        ) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    UserAccountDoc generated_account(generated_account_oid);

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_account.logged_in_token,
            generated_account.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ChatRoomHeaderDoc original_chat_room_header(create_chat_room_response.chat_room_id());

    generated_account.getFromCollection(generated_account_oid);
    UserAccountStatisticsDoc user_account_statistics(generated_account_oid);

    //Set up a matching account, must be done before pictures extracted.
    const bsoncxx::oid matching_account_oid = insertRandomAccounts(1, 0);
    UserAccountDoc matching_user_account(matching_account_oid);

    const std::string matching_chat_room_id = buildMatchMadeChatRoom(
            current_timestamp,
            generated_account_oid,
            matching_account_oid,
            generated_account,
            matching_user_account
    );

    std::vector<UserPictureDoc> pictures_before_delete;
    std::string thumbnail;
    std::string thumbnail_reference_oid;

    for (const UserAccountDoc::PictureReference& picture_obj : generated_account.pictures) {
        if (picture_obj.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};

            picture_obj.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            pictures_before_delete.emplace_back(UserPictureDoc(picture_reference));

            if (thumbnail.empty()) {
                thumbnail = pictures_before_delete.back().thumbnail_in_bytes;
                thumbnail_reference_oid = pictures_before_delete.back().current_object_oid.to_string();
            }
        }
    }

    //generate outstanding reports to make sure they are removed
    OutstandingReports outstanding_reports = generateRandomOutstandingReports(
            generated_account_oid,
            current_timestamp
    );

    outstanding_reports.setIntoCollection();

    //must extract handled reports before the function runs
    HandledReports handled_reports(generated_account_oid);

    InfoStoredAfterDeletionDoc original_info_stored_after_deletion_doc(generated_account.phone_number);

    EmailVerificationDoc email_verification_doc;
    email_verification_doc.user_account_reference = generated_account_oid;
    email_verification_doc.verification_code = gen_random_alpha_numeric_string(
            general_values::EMAIL_VERIFICATION_AND_ACCOUNT_RECOVERY_VERIFICATION_CODE_LENGTH
    );
    email_verification_doc.time_verification_code_generated = bsoncxx::types::b_date{
            current_timestamp - std::chrono::milliseconds{5L * 24L * 60L * 60L * 1000L}
    };
    email_verification_doc.address_being_verified = generated_account.email_address;
    email_verification_doc.setIntoCollection();

    AccountRecoveryDoc account_recovery_doc;
    account_recovery_doc.verification_code = gen_random_alpha_numeric_string(
            general_values::EMAIL_VERIFICATION_AND_ACCOUNT_RECOVERY_VERIFICATION_CODE_LENGTH
    );
    account_recovery_doc.phone_number = generated_account.phone_number;
    account_recovery_doc.time_verification_code_generated = bsoncxx::types::b_date{
            current_timestamp - std::chrono::milliseconds{5L * 24L * 60L * 60L * 1000L}
    };;
    account_recovery_doc.number_attempts = 0;
    account_recovery_doc.user_account_oid = generated_account_oid;
    account_recovery_doc.setIntoCollection();

    //run serverInternalDeleteAccount()
    bool successful = run_delete(current_timestamp);

    EXPECT_TRUE(successful);

    original_info_stored_after_deletion_doc.time_sms_can_be_sent_again = generated_account.time_sms_can_be_sent_again;
    original_info_stored_after_deletion_doc.time_email_can_be_sent_again = generated_account.time_email_can_be_sent_again;
    original_info_stored_after_deletion_doc.number_swipes_remaining = generated_account.number_swipes_remaining;
    original_info_stored_after_deletion_doc.swipes_last_updated_time = generated_account.swipes_last_updated_time;

    InfoStoredAfterDeletionDoc after_info_stored_after_deletion_doc(generated_account.phone_number);

    EXPECT_EQ(original_info_stored_after_deletion_doc, after_info_stored_after_deletion_doc);

    EmailVerificationDoc after_email_verification_doc(email_verification_doc.current_object_oid);

    EXPECT_EQ(after_email_verification_doc.current_object_oid.to_string(), "000000000000000000000000");

    AccountRecoveryDoc after_account_recovery_doc(account_recovery_doc.current_object_oid);

    EXPECT_EQ(after_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");

    checkMoveUserAccountStatisticsDocumentResult(
            generated_account_oid,
            current_timestamp,
            user_account_statistics,
            false
    );

    checkMoveOutstandingReportsToHandledResult(
            current_timestamp,
            generated_account_oid,
            "",
            ReportHandledMoveReason::REPORT_HANDLED_REASON_REPORTS_DELETED,
            DisciplinaryActionTypeEnum(-1),
            outstanding_reports,
            true,
            handled_reports
    );

    int dummy_deleted_account_thumbnail_size = 0;

    checkDeleteAccountPicturesResults(
            true,
            pictures_before_delete,
            current_timestamp,
            user_pictures_collection,
            dummy_deleted_account_thumbnail_size,
            "",
            ""
    );

    checkLeaveChatRoomResult(
            current_timestamp,
            original_chat_room_header,
            generated_account_oid,
            thumbnail_reference_oid,
            thumbnail,
            create_chat_room_response.chat_room_id()
    );

    //make sure match has been canceled
    ChatRoomHeaderDoc match_header_doc(matching_chat_room_id);
    EXPECT_EQ(match_header_doc.matching_oid_strings, nullptr);

    matching_user_account.other_accounts_matched_with.clear();
    matching_user_account.chat_rooms.clear();
    UserAccountDoc matching_user_account_afterwards(matching_account_oid);

    EXPECT_EQ(matching_user_account, matching_user_account_afterwards);

    //test if account was deleted
    UserAccountDoc user_account_afterwards(generated_account_oid);

    EXPECT_EQ(user_account_afterwards.current_object_oid, bsoncxx::oid{"000000000000000000000000"});

    DeletedAccountsDoc generated_deleted_account(
            generated_account,
            bsoncxx::types::b_date{current_timestamp}
    );

    DeletedAccountsDoc extracted_deleted_account(generated_account_oid);

    EXPECT_EQ(generated_deleted_account, extracted_deleted_account);

}