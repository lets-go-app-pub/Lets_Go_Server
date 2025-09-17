
#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "setup_login_info.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "RequestStatistics.pb.h"
#include "compare_equivalent_messages.h"
#include "SetAdminFields.pb.h"
#include "set_admin_fields.h"
#include "connection_pool_global_variable.h"
#include "activities_info_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetServerCategoryOrActivityTesting : public ::testing::Test {
protected:

    set_admin_fields::SetServerActivityOrCategoryRequest request;

    bool successful = true;
    std::string error_message;

    inline static const std::string DUMMY_ACTIVITIES_COLLECTION_NAME = "dummy_activities_col";
    const std::string previous_activities_info_collection_name = collection_names::ACTIVITIES_INFO_COLLECTION_NAME;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection dummy_activities_collection = accounts_db[DUMMY_ACTIVITIES_COLLECTION_NAME];

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_delete_this(false);
        request.set_display_name(
                gen_random_alpha_numeric_string(
                        server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE + rand() % (
                                server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE -
                                server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE
                        )
                )
        );
        request.set_icon_display_name(
                gen_random_alpha_numeric_string(
                        server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE + rand() % (
                                server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE -
                                server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE
                        )
                )
        );
        request.set_min_age(
                server_parameter_restrictions::LOWEST_ALLOWED_AGE + rand() % (
                        server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1 -
                        server_parameter_restrictions::LOWEST_ALLOWED_AGE
                )
        );
        request.set_order_number(rand() % 100);

        //generate a random color code
        std::stringstream ss;
        ss
                << "#"
                << std::setfill('0') << std::setw(6)
                << std::hex << (rand() % (long) std::pow(2, 24));

        request.set_color(ss.str());

        setupUnknownActivityAndCategory();
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        //don't pollute the actual collection
        collection_names::ACTIVITIES_INFO_COLLECTION_NAME = DUMMY_ACTIVITIES_COLLECTION_NAME;

        dummy_activities_collection.insert_one(
            document{}
                << "_id" << activities_info_keys::ID
                        << activities_info_keys::CATEGORIES << open_array
                << close_array
                        << activities_info_keys::ACTIVITIES << open_array
                << close_array
            << finalize
        );

        setupValidRequest();
    }

    void TearDown() override {
        //Must be set back BEFORE clearDatabaseAndGlobalsForTesting() is run, otherwise this collection will not
        // be cleared.
        collection_names::ACTIVITIES_INFO_COLLECTION_NAME = previous_activities_info_collection_name;

        clearDatabaseAndGlobalsForTesting();
    }

    template<bool is_activity>
    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        successful = setServerCategoryOrActivity(
                request,
                error_message,
                is_activity
        );
    }

    void checkFunctionFailed() {
        EXPECT_FALSE(error_message.empty());
        EXPECT_FALSE(successful);
    }

    void checkSuccessResults() {
        std::cout << "error_message: " << error_message << '\n';
        EXPECT_TRUE(error_message.empty());
        EXPECT_TRUE(successful);

        if(!error_message.empty()) {
            std::cout << error_message << '\n';
        }
    }

    template <bool deleted = false>
    void insertCategoryAndCheckResult(ActivitiesInfoDoc& generated_activities_info_doc) {
        runFunction<false>();

        checkSuccessResults();

        successful = true;
        error_message.clear();

        std::string capital_color_code;

        for(char c : request.color()) {
            if(isalpha(c)) {
                capital_color_code += (char)toupper(c);
            } else {
                capital_color_code += c;
            }
        }

        generated_activities_info_doc.categories.emplace_back(
                request.display_name(),
                request.icon_display_name(),
                request.order_number(),
                deleted ? server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1 : (int)request.min_age(),
                convertCategoryActivityNameToStoredName(request.display_name()),
                capital_color_code
        );

        ActivitiesInfoDoc extracted_activities_info_doc;
        extracted_activities_info_doc.getFromCollection();

        EXPECT_EQ(generated_activities_info_doc, extracted_activities_info_doc);
    }

    template <bool deleted = false>
    void insertActivityAndCheckResult(ActivitiesInfoDoc& generated_activities_info_doc) {
        runFunction<true>();

        checkSuccessResults();

        successful = true;
        error_message.clear();

        generated_activities_info_doc.activities.emplace_back(
                request.display_name(),
                request.icon_display_name(),
                deleted ? server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1 : (int)request.min_age(),
                convertCategoryActivityNameToStoredName(request.display_name()),
                request.category_index(),
                request.icon_index()
        );

        ActivitiesInfoDoc extracted_activities_info_doc;
        extracted_activities_info_doc.getFromCollection();

        EXPECT_EQ(generated_activities_info_doc, extracted_activities_info_doc);
    }

    static void setupUnknownActivityAndCategory() {
        ActivitiesInfoDoc activities_info_doc;
        activities_info_doc.id = activities_info_keys::ID;

        activities_info_doc.categories.emplace_back(
                "Unknown",
                "Unknown",
                0.0,
                121,
                "unknown",
                "#000000"
        );

        activities_info_doc.activities.emplace_back(
                "Unknown",
                "Unknown",
                121,
                "unknown",
                0,
                0
        );

        activities_info_doc.setIntoCollection();
    }
};

TEST_F(SetServerCategoryOrActivityTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                error_message.clear();
                successful = true;

                runFunction<false>();

                EXPECT_FALSE(error_message.empty());
                return successful;
            }
    );
}

TEST_F(SetServerCategoryOrActivityTesting, noAdminPriveledge) {
    runFunction<false>(AdminLevelEnum::NO_ADMIN_ACCESS);

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, invalidDisplayName_tooShort) {
    request.set_display_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE - 1));

    runFunction<false>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, invalidDisplayName_tooLong) {
    request.set_display_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE + 1));

    runFunction<false>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, invalidIconDisplayName_tooShort) {
    request.set_icon_display_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE - 1));

    runFunction<false>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, invalidIconDisplayName_tooLong) {
    request.set_display_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE + 1));

    runFunction<false>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, invalidMinAge_tooYoung) {
    request.set_min_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE - 1);

    runFunction<false>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, invalidMinAge_tooOld) {
    request.set_min_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 2);

    runFunction<false>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, attemptToAddAnActivityToUnknownCategory) {
    request.set_category_index(0);

    runFunction<true>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, attemptToModifyUnknownActivity) {
    request.set_display_name("Unknown");
    request.set_category_index(0);

    runFunction<true>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, attemptToUpdateUnknownCategory) {
    request.set_display_name("Unknown");

    runFunction<false>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, invalidColor) {
    request.set_color("1234567");

    runFunction<false>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, iconIndexDoesNotExist) {

    ActivitiesInfoDoc dummy(true);

    //insert category so that activity is valid
    insertCategoryAndCheckResult(dummy);

    request.set_icon_index(ULONG_MAX);
    request.set_category_index(0);

    runFunction<true>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, categoryIndexDoesNotExist) {
    request.set_icon_index(0);
    request.set_category_index(ULONG_MAX);

    runFunction<true>();

    checkFunctionFailed();
}

TEST_F(SetServerCategoryOrActivityTesting, successfullyAddsCategory) {
    ActivitiesInfoDoc generated_activities_info_doc(true);
    insertCategoryAndCheckResult(generated_activities_info_doc);
}

TEST_F(SetServerCategoryOrActivityTesting, successfullyAddsActivity) {
    ActivitiesInfoDoc generated_activities_info_doc(true);
    insertCategoryAndCheckResult(generated_activities_info_doc);

    request.set_category_index(1);
    request.set_icon_index(0);

    insertActivityAndCheckResult(generated_activities_info_doc);
}

TEST_F(SetServerCategoryOrActivityTesting, successfullyUpdatesCategory) {

    //insert category to be updated
    ActivitiesInfoDoc generated_activities_info_doc(true);
    insertCategoryAndCheckResult(generated_activities_info_doc);

    request.set_min_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
    request.set_color("#FFFFFF");
    request.set_order_number(-5.0);

    //remove category from vector (insertCategoryAndCheckResult() will properly add new one)
    ASSERT_FALSE(generated_activities_info_doc.categories.empty());
    generated_activities_info_doc.categories.pop_back();

    insertCategoryAndCheckResult(generated_activities_info_doc);
}

TEST_F(SetServerCategoryOrActivityTesting, successfullyUpdatesActivity) {
    ActivitiesInfoDoc generated_activities_info_doc(true);

    //insert category so activity 'category_index' is valid
    insertCategoryAndCheckResult(generated_activities_info_doc);

    //insert activity to be updated
    request.set_category_index(1);
    request.set_icon_index(0);
    insertActivityAndCheckResult(generated_activities_info_doc);

    request.set_min_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
    request.set_icon_index(1);

    //remove activity from vector (insertActivityAndCheckResult() will properly add new one)
    ASSERT_FALSE(generated_activities_info_doc.activities.empty());
    generated_activities_info_doc.activities.pop_back();

    insertActivityAndCheckResult(generated_activities_info_doc);
}

TEST_F(SetServerCategoryOrActivityTesting, successfullyDeletesCategory) {
    ActivitiesInfoDoc generated_activities_info_doc(true);

    //insert category to be 'deleted'
    insertCategoryAndCheckResult(generated_activities_info_doc);

    request.set_delete_this(true);

    //remove category from vector (insertCategoryAndCheckResult() will properly add new one)
    ASSERT_FALSE(generated_activities_info_doc.categories.empty());
    generated_activities_info_doc.categories.pop_back();

    insertCategoryAndCheckResult<true>(generated_activities_info_doc);
}

TEST_F(SetServerCategoryOrActivityTesting, successfullyDeletesActivity) {
    ActivitiesInfoDoc generated_activities_info_doc(true);

    //insert category so activity 'category_index' is valid
    insertCategoryAndCheckResult(generated_activities_info_doc);

    //insert activity to be 'deleted'
    request.set_category_index(1);
    request.set_icon_index(0);
    insertActivityAndCheckResult(generated_activities_info_doc);

    request.set_delete_this(true);

    //remove activity from vector (insertActivityAndCheckResult() will properly add new one)
    ASSERT_FALSE(generated_activities_info_doc.activities.empty());
    generated_activities_info_doc.activities.pop_back();

    insertActivityAndCheckResult<true>(generated_activities_info_doc);
}
