//
// Created by jeremiah on 3/19/21.
//

#include "get_user_info_timestamps.h"

#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"

enum class TypeOfTimestamp {
    BIRTHDAY_TIMESTAMP,
    EMAIL_TIMESTAMP,
    GENDER_TIMESTAMP,
    NAME_TIMESTAMP,
    PICTURES_TIMESTAMP,
    CATEGORIES_TIMESTAMP
};

inline long long getTimestampsHelper(
        TypeOfTimestamp type_of_timestamp,
        const bsoncxx::document::view& user_account_doc,
        const std::string& timestamp_key,
        google::protobuf::RepeatedField<google::protobuf::int64>* picture_timestamps = nullptr
) {

    std::chrono::milliseconds timestamp;
    const auto timestamp_element = user_account_doc[timestamp_key];
    if (type_of_timestamp == TypeOfTimestamp::PICTURES_TIMESTAMP) { //pictures
        timestamp = std::chrono::milliseconds{-1};

        if (timestamp_element
            && timestamp_element.type() == bsoncxx::type::k_array) { //if timestamp exists and is type array
            const bsoncxx::array::view timestamp_array = timestamp_element.get_array().value;

            picture_timestamps->Clear();

            for (const auto& ele : timestamp_array) {

                if (ele.type() == bsoncxx::type::k_document) {
                    const bsoncxx::document::view picture_document = ele.get_document().value;

                    const auto picture_timestamp_element = picture_document[user_account_keys::pictures::TIMESTAMP_STORED];
                    if (picture_timestamp_element
                        && picture_timestamp_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
                        const std::chrono::milliseconds extracted_timestamp = picture_timestamp_element.get_date().value;

                        //add in the elements to the list
                        picture_timestamps->Add(extracted_timestamp.count());
                        if (extracted_timestamp.count() > 0) {
                            timestamp = std::chrono::milliseconds{1};
                        }
                    } else { //if element does not exist or is not type date
                        logElementError(
                                __LINE__, __FILE__,
                                picture_timestamp_element, picture_document,
                                bsoncxx::type::k_date, user_account_keys::pictures::TIMESTAMP_STORED,
                                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                        );
                    }
                } else if (ele.type() == bsoncxx::type::k_null) {
                    //add in the elements to the list
                    picture_timestamps->Add(-1L);
                } else {
                    logElementError(
                            __LINE__, __FILE__,
                            timestamp_element, user_account_doc,
                            bsoncxx::type::k_null, timestamp_key,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );
                }
            }
        } else {
            logElementError(
                    __LINE__, __FILE__,
                    timestamp_element, user_account_doc,
                    bsoncxx::type::k_array, timestamp_key,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
        }

    }
    else { //any other timestamp

        if (timestamp_element &&
            timestamp_element.type() == bsoncxx::type::k_date) { //if timestamp exists and is type date
            timestamp = timestamp_element.get_date().value;
        } else { //if timestamp does not exist or is not type date
            logElementError(
                    __LINE__, __FILE__,
                    timestamp_element, user_account_doc,
                    bsoncxx::type::k_date, timestamp_key,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            timestamp = std::chrono::milliseconds{-1};
        }

    }

    return timestamp.count();
}

//checks mandatory info for set timestamps
//returns true if it needs more info
//returns false if all mandatory info is set
bool getUserInfoTimestamps(
        const bsoncxx::document::view& user_account_doc_view,
        PreLoginTimestampsMessage* pre_login_timestamps,
        google::protobuf::RepeatedField<google::protobuf::int64>* picture_timestamps
) {

    //set birthday
    pre_login_timestamps->set_birthday_timestamp(
            getTimestampsHelper(
                    TypeOfTimestamp::BIRTHDAY_TIMESTAMP,
                    user_account_doc_view,
                    user_account_keys::BIRTHDAY_TIMESTAMP)
            );

    //set email
    pre_login_timestamps->set_email_timestamp(
            getTimestampsHelper(
                    TypeOfTimestamp::EMAIL_TIMESTAMP,
                    user_account_doc_view,
                    user_account_keys::EMAIL_TIMESTAMP)
            );

    //set gender
    pre_login_timestamps->set_gender_timestamp(
            getTimestampsHelper(
                    TypeOfTimestamp::GENDER_TIMESTAMP,
                    user_account_doc_view,
                    user_account_keys::GENDER_TIMESTAMP)
            );

    //set name
    pre_login_timestamps->set_name_timestamp(
            getTimestampsHelper(
                    TypeOfTimestamp::NAME_TIMESTAMP,
                    user_account_doc_view,
                    user_account_keys::FIRST_NAME_TIMESTAMP)
            );

    //set categories
    pre_login_timestamps->set_categories_timestamp(
            getTimestampsHelper(
                    TypeOfTimestamp::CATEGORIES_TIMESTAMP,
                    user_account_doc_view,
                    user_account_keys::CATEGORIES_TIMESTAMP)
            );

    getTimestampsHelper(
            TypeOfTimestamp::PICTURES_TIMESTAMP,
            user_account_doc_view,
            user_account_keys::PICTURES,
            picture_timestamps
    );

    //NOTE: Pictures do not need to be checked because they can be empty if an admin deleted a picture.
    if (
            pre_login_timestamps->birthday_timestamp() == -1
            || pre_login_timestamps->email_timestamp() == -1
            || pre_login_timestamps->gender_timestamp() == -1
            || pre_login_timestamps->name_timestamp() == -1
            || pre_login_timestamps->categories_timestamp() == -1) {
        return true;
    }

    return false;

}
