//
// Created by jeremiah on 9/9/22.
//

#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "helper_functions/build_update_doc_for_response_type.h"
#include "helper_functions/user_match_options_helper_functions.h"
#include "helper_functions/check_for_no_elements_in_array.h"
#include "errors_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class CheckForNoElementsInArrayTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    int addElementsAndReturn(
            int number_elements,
            bool store_error_when_elements_found
            ) {
        for(int i = 0; i < number_elements; ++i) {
            MatchingElement matching_element;
            matching_element.generateRandomValues();

            user_account_doc.other_users_matched_accounts_list.emplace_back(matching_element);
        }
        user_account_doc.setIntoCollection();

        bsoncxx::builder::stream::document doc_builder;
        bsoncxx::document::view doc_builder_view;

        user_account_doc.convertToDocument(doc_builder);
        doc_builder_view = doc_builder.view();

        if(store_error_when_elements_found) {
            return checkForNoElementsInArray<true>(
                    &doc_builder_view,
                    user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST
            );
        } else {
            return checkForNoElementsInArray<false>(
                    &doc_builder_view,
                    user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST
            );
        }
    }

    static void runChecks(
            int num_elements_returned,
            int num_elements_added,
            bool error_exists
            ) {
        EXPECT_EQ(num_elements_returned, num_elements_added);

        FreshErrorsDoc error;
        error.getFromCollection();

        if(error_exists) {
            EXPECT_NE(error.current_object_oid.to_string(), "000000000000000000000000");
        } else {
            EXPECT_EQ(error.current_object_oid.to_string(), "000000000000000000000000");
        }
    }
};

TEST_F(CheckForNoElementsInArrayTesting, false_multipleElementsInArray) {

    static const int num_elements_to_add = 5;

    int num_elements = addElementsAndReturn(
            num_elements_to_add,
            false
    );

    runChecks(
            num_elements,
            num_elements_to_add,
            false
    );
}

TEST_F(CheckForNoElementsInArrayTesting, false_noElementsInArray) {
    static const int num_elements_to_add = 0;

    int num_elements = addElementsAndReturn(
            num_elements_to_add,
            false
    );

    runChecks(
            num_elements,
            num_elements_to_add,
            false
    );
}

TEST_F(CheckForNoElementsInArrayTesting, true_singleElementInArray) {

    static const int num_elements_to_add = 1;

    int num_elements = addElementsAndReturn(
            num_elements_to_add,
            true
    );

    runChecks(
            num_elements,
            num_elements_to_add,
            true
    );
}

TEST_F(CheckForNoElementsInArrayTesting, true_noElementsInArray) {
    static const int num_elements_to_add = 0;

    int num_elements = addElementsAndReturn(
            num_elements_to_add,
            true
    );

    runChecks(
            num_elements,
            num_elements_to_add,
            false
    );
}
