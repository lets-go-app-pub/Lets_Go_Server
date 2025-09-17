//
// Created by jeremiah on 5/31/22.
//

#include <move_user_account_statistics_document.h>
#include <info_for_statistics_objects.h>
#include <gtest/gtest.h>

void checkMoveUserAccountStatisticsDocumentResult(
        const bsoncxx::oid& account_oid,
        const std::chrono::milliseconds& currentTimestamp,
        const UserAccountStatisticsDoc& user_account_statistics,
        bool create_new_statistics_document
) {

    UserAccountStatisticsDocumentsCompletedDoc copy_statistics_documents_completed(
            user_account_statistics,
            account_oid,
            currentTimestamp
    );

    UserAccountStatisticsDocumentsCompletedDoc extracted_statistics_documents_completed(true);

    //old current_object_oid will be invalid
    copy_statistics_documents_completed.current_object_oid = extracted_statistics_documents_completed.current_object_oid;

    UserAccountStatisticsDoc generated_user_account_statistics(account_oid);

    if(create_new_statistics_document) {
        UserAccountStatisticsDoc empty_user_account_statistics{};
        empty_user_account_statistics.current_object_oid = account_oid;
        EXPECT_EQ(generated_user_account_statistics, empty_user_account_statistics);
    } else {
        //the document should not exist to have been extracted
        EXPECT_EQ(generated_user_account_statistics.current_object_oid.to_string(), "000000000000000000000000");
    }

    EXPECT_EQ(copy_statistics_documents_completed, extracted_statistics_documents_completed);
}