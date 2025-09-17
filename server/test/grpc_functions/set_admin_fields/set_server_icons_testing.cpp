
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

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetServerIconsTesting : public ::testing::Test {
protected:

    set_admin_fields::SetServerIconRequest request;

    bool successful = true;
    std::string error_message;

    inline static const std::string DUMMY_ICONS_COLLECTION_NAME = "dummy_activities_col";
    const std::string previous_icons_info_collection_name = collection_names::ICONS_INFO_COLLECTION_NAME;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection dummy_icons_collection = accounts_db[DUMMY_ICONS_COLLECTION_NAME];

    void setupValidIconInBytes() {
        request.set_icon_in_bytes(gen_random_alpha_numeric_string(rand() % 10000 + 1));
        request.set_icon_size_in_bytes((int)request.icon_in_bytes().size());
    }

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_push_back(true);
        request.set_icon_active(true);
        setupValidIconInBytes();
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        //don't pollute the actual collection
        collection_names::ICONS_INFO_COLLECTION_NAME = DUMMY_ICONS_COLLECTION_NAME;

        setupValidRequest();
    }

    void TearDown() override {
        //Must be set back BEFORE clearDatabaseAndGlobalsForTesting() is run, otherwise this collection will not
        // be cleared.
        collection_names::ICONS_INFO_COLLECTION_NAME = previous_icons_info_collection_name;

        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        successful = setServerIcon(
                request,
                error_message
        );
    }

    void checkFunctionFailed() {
        EXPECT_FALSE(error_message.empty());
        EXPECT_FALSE(successful);
    }

    void checkSuccessResults() {
        EXPECT_TRUE(error_message.empty());
        EXPECT_TRUE(successful);

        if(!error_message.empty()) {
            std::cout << error_message << '\n';
        }
    }

    IconsInfoDoc setIconAndCheck(int new_index_value) {
        //NOTE: It will move the string out of the request, need to make a copy.
        std::string icon_in_bytes = request.icon_in_bytes();

        runFunction();

        checkSuccessResults();

        IconsInfoDoc generated_info_doc;

        generated_info_doc.index = new_index_value;
        generated_info_doc.icon_in_bytes = icon_in_bytes;
        generated_info_doc.icon_size_in_bytes = (long)icon_in_bytes.size();
        generated_info_doc.icon_active = request.icon_active();

        IconsInfoDoc extracted_info_doc(new_index_value);

        EXPECT_GT(extracted_info_doc.timestamp_last_updated, 0);
        generated_info_doc.timestamp_last_updated = extracted_info_doc.timestamp_last_updated;

        EXPECT_EQ(generated_info_doc, extracted_info_doc);

        return generated_info_doc;
    }
};

TEST_F(SetServerIconsTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                error_message.clear();
                successful = true;

                runFunction();

                EXPECT_FALSE(error_message.empty());
                return successful;
            }
    );
}

TEST_F(SetServerIconsTesting, noAdminPriveledge) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    checkFunctionFailed();
}

TEST_F(SetServerIconsTesting, pushBackAnInactiveIcon) {
    request.set_icon_active(false);

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerIconsTesting, iconActiveAndEmptyBytes) {
    request.set_icon_active(true);
    request.set_icon_in_bytes("");
    request.set_icon_size_in_bytes(0);

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerIconsTesting, corruptIconFile) {
    request.set_icon_active(true);
    request.set_icon_size_in_bytes(request.icon_in_bytes().size() - 1);

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerIconsTesting, pushToEmptyCollection) {
    setIconAndCheck(0);
}

TEST_F(SetServerIconsTesting, pushToNonEmptyCollection) {
    setIconAndCheck(0);

    //icon_in_bytes was moved out of request, regenerate a new one
    setupValidIconInBytes();

    setIconAndCheck(1);
}

TEST_F(SetServerIconsTesting, updateIconToInactive) {

    const int working_index = 0;

    //push new icon to be updated
    setIconAndCheck(working_index);

    request.set_index_number(working_index);
    request.set_push_back(false);
    request.set_icon_active(false);
    request.set_icon_in_bytes("");
    request.set_icon_size_in_bytes(0);

    setIconAndCheck(working_index);
}

TEST_F(SetServerIconsTesting, updateExistingIcon) {
    const int working_index = 0;

    //push new icon to be updated
    setIconAndCheck(working_index);

    request.set_index_number(working_index);
    request.set_push_back(false);

    //icon_in_bytes was moved out of request, regenerate a new one
    setupValidIconInBytes();

    //update icon
    setIconAndCheck(working_index);
}

TEST_F(SetServerIconsTesting, updateNonExistingIcon) {
    const int working_index = 0;

    //push new icon to be updated
    setIconAndCheck(working_index);

    request.set_index_number(working_index + 1);
    request.set_push_back(false);

    //icon_in_bytes was moved out of request, regenerate a new one
    setupValidIconInBytes();

    runFunction();

    checkFunctionFailed();
}

