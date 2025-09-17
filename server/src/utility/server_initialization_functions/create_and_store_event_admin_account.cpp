//
// Created by jeremiah on 3/16/23.
//
#include <fstream>

#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include "connection_pool_global_variable.h"

#include "database_names.h"
#include "collection_names.h"
#include "event_admin_values.h"
#include "create_user_account.h"
#include "assert_macro.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void createAndStoreEventAdminAccount() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const AccountCreationParameterPack parameter_pack{
            event_admin_values::OID,
            current_timestamp,
            event_admin_values::INSTALLATION_ID,
            event_admin_values::PHONE_NUMBER
    };

    bsoncxx::oid extracted_user_account_oid;

    assert_msg(
            createUserAccount(
                    accounts_db,
                    user_accounts_collection,
                    nullptr,
                    current_timestamp,
                    AccountLoginType::LOGIN_TYPE_VALUE_NOT_SET,
                    parameter_pack
            ),
            std::string("An error occurred when creating the user account")
    );

    const bsoncxx::oid single_picture_oid;
    bsoncxx::stdx::optional<mongocxx::result::insert_one> insert_pictures_success;
    try {

        std::ifstream thumbnail_fin(general_values::PATH_TO_SRC + "/resources/logo_256x256_filled_white.png");
        std::ifstream picture_fin(general_values::PATH_TO_SRC + "/resources/logo_2048x2048_filled_white.png");

        auto* thumbnail_in_bytes = new std::string();
        int thumbnail_size_in_bytes = 0;
        auto* picture_in_bytes = new std::string();
        int picture_size_in_bytes = 0;

        for(char c; thumbnail_fin.get(c);) {
            *thumbnail_in_bytes += c;
            thumbnail_size_in_bytes++;
        }

        thumbnail_fin.close();

        for(char c; picture_fin.get(c);) {
            picture_size_in_bytes++;
            *picture_in_bytes += c;
        }

        picture_fin.close();

        const bsoncxx::document::value single_picture_document =
                createUserPictureDoc(
                        single_picture_oid,
                        parameter_pack.user_account_oid,
                        current_timestamp,
                        0,
                        thumbnail_in_bytes,
                        thumbnail_size_in_bytes,
                        picture_in_bytes,
                        picture_size_in_bytes
                );

        delete thumbnail_in_bytes;
        delete picture_in_bytes;

        //insert new user picture
        insert_pictures_success = user_pictures_collection.insert_one(
                single_picture_document.view()
        );
    }
    catch (const mongocxx::logic_error& e) {
        std::cout << "Exception occurred when insert event admin account picture.\nMessage: " << e.what() << '\n';
        throw e;
    }

    assert_msg(
            insert_pictures_success && insert_pictures_success->result().inserted_count() > 0,
            std::string("Inserting picture for event admin account failed.")
    );

    bsoncxx::builder::basic::array pictures_array;

    pictures_array.append(
        document{}
            << user_account_keys::pictures::OID_REFERENCE << single_picture_oid
            << user_account_keys::pictures::TIMESTAMP_STORED << bsoncxx::types::b_date{current_timestamp}
        << finalize
    );

    for(int i = 1; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; ++i) {
        pictures_array.append(bsoncxx::types::b_null{});
    }

    mongocxx::stdx::optional<mongocxx::result::update> update_event_admin_account;
    try {
        bsoncxx::types::b_date current_mongodb_time{current_timestamp};
        update_event_admin_account = user_accounts_collection.update_one(
            document{}
                    << "_id" << event_admin_values::OID
                << finalize,
            document{}
                << "$set" << open_document
                    << user_account_keys::FIRST_NAME << event_admin_values::FIRST_NAME
                    << user_account_keys::GENDER << event_admin_values::GENDER
                    << user_account_keys::BIO << event_admin_values::BIO
                    << user_account_keys::BIRTH_YEAR << event_admin_values::BIRTH_YEAR
                    << user_account_keys::BIRTH_MONTH << event_admin_values::BIRTH_MONTH
                    << user_account_keys::BIRTH_DAY_OF_MONTH << event_admin_values::BIRTH_DAY_OF_MONTH
                    << user_account_keys::BIRTH_DAY_OF_YEAR << event_admin_values::BIRTH_DAY_OF_YEAR
                    << user_account_keys::AGE << event_admin_values::AGE
                    << user_account_keys::EMAIL_ADDRESS << event_admin_values::EMAIL_ADDRESS
                    << user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION << event_admin_values::EMAIL_ADDRESS_REQUIRES_VERIFICATION
                    << user_account_keys::PICTURES << pictures_array
                    << user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED << current_mongodb_time
                    << user_account_keys::BIRTHDAY_TIMESTAMP << current_mongodb_time
                    << user_account_keys::GENDER_TIMESTAMP << current_mongodb_time
                    << user_account_keys::FIRST_NAME_TIMESTAMP << current_mongodb_time
                    << user_account_keys::BIO_TIMESTAMP << current_mongodb_time
                    << user_account_keys::CITY_NAME_TIMESTAMP << current_mongodb_time
                    << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << current_mongodb_time
                    << user_account_keys::EMAIL_TIMESTAMP << current_mongodb_time
                    << user_account_keys::CATEGORIES_TIMESTAMP << current_mongodb_time
                << close_document
            << finalize
        );
    }
    catch (const mongocxx::exception& e) {
        std::cout << "Exception occurred when checking event admin account.\nMessage: " << e.what() << '\n';
        throw e;
    }

    assert_msg(
            update_event_admin_account && update_event_admin_account->result().modified_count() > 0,
            std::string(
                    "When updating the event admin accounts basic values, the operation failed.\nmodified_count: " +
                    (update_event_admin_account ? std::to_string(update_event_admin_account->result().modified_count()) : "-1")
            )
    );

}