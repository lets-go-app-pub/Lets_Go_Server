
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <ctime>
#include <iomanip>
#include <regex>

#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bson/bson.h>

#include <boost/uuid/uuid_io.hpp>
#include <global_bsoncxx_docs.h>
#include <extract_thumbnail_from_verified_doc.h>
#include <AccountCategoryEnum.grpc.pb.h>
#include <report_handled_move_reason.h>
#include <report_helper_functions.h>
#include <move_user_account_statistics_document.h>
#include <extract_data_from_bsoncxx.h>
#include <session_to_run_functions.h>
#include <server_values.h>
#include <android_specific_values.h>
#include <deleted_user_pictures_keys.h>
#include <utility_testing_functions.h>
#include <deleted_accounts_keys.h>
#include <random>
#include <boost/uuid/random_generator.hpp>

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "utility_chat_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "user_account_keys.h"
#include "user_pictures_keys.h"
#include "info_stored_after_deletion_keys.h"
#include "email_verification_keys.h"
#include "account_recovery_keys.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"
#include "chat_room_header_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void logElementError(
        const int& lineNumber,
        const std::string& fileName,
        const bsoncxx::document::element& errorElement,
        const bsoncxx::document::view& documentView,
        const bsoncxx::type& type,
        const std::string& key,
        const std::string& databaseName,
        const std::string& collectionName
) {

    std::string typeString = convertBsonTypeToString(type);
    std::string errorString;
    std::optional<std::string> dummy;

    if (errorElement) { //if element exists but the type does not match

        errorString = "the element '" + key + "' inside the '"
                      + databaseName + "' '" + collectionName + "' document is not type '" + typeString + "'";

        storeMongoDBErrorAndException(
            lineNumber, fileName,
            dummy, errorString,
            "failedDocument", documentView
        );
    } else { //if element does not exist

        errorString = "the element '" + key + "' does not exist in the '"
                      + databaseName + "' '" + collectionName + "' document";

        storeMongoDBErrorAndException(
            lineNumber, fileName,
            dummy, errorString,
            "failedDocument", documentView
        );
    }
}

bool isInvalidGender(const std::string& gender) {

    //Allowing chars other than the alphabet in gender names
    // and also allowing different use of capitalization.
    //Do not want to allow the EVENT_GENDER_VALUE. This is used as part of the matching algorithm when searching events.
    return (gender.empty() || gender == general_values::EVENT_GENDER_VALUE || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES < gender.size());
}

bool isInvalidLocation(const double& longitude, const double& latitude) {

    if (longitude < -180.0 || 180.0 < longitude ||
        latitude <= -90.0 || 90.0 <= latitude) {
        return true;
    }

    return false;
}

bool isValidPhoneNumber(const std::string& phoneNumber) {

    //+ sign(1), extension(1), area code(3), number body(7) total: (12)
    //should be the format
    //should start with +1
    //area code cannot start with 0 or 1
    //area code cannot be 555
    if (phoneNumber.length() != 12
        || phoneNumber[0] != '+'
        || phoneNumber[1] != '1'
        || phoneNumber[2] == '0'
        || phoneNumber[2] == '1'
        || (
                phoneNumber[2] == '5'
                && phoneNumber[3] == '5'
                && phoneNumber[4] == '5'
            )
        ) {
        return false;
    }

    for (size_t i = 2; i < phoneNumber.length(); i++) {
        if (!isdigit(phoneNumber[i])) {
            return false;
        }
    }

    return true;
}

bool isInvalidBirthday(
        const std::chrono::milliseconds& current_timestamp,
        int birth_year,
        int birth_month,
        int birth_day_of_month
) {

    const time_t time_object = current_timestamp.count() / 1000;

    tm date_time_struct{};
    gmtime_r(&time_object, &date_time_struct);
    int year = date_time_struct.tm_year + 1900;

    bool invalid_birthday = false;

    //simply checks if the year is a year value
    //NOTE: Does not incorporate max and min age on purpose, that is meant to be checked later.
    if (
            (birth_year < 1900 || year < birth_year)
            ||
            (birth_month < 1 || 12 < birth_month) //this should be a value 1-12
            ||
            (birth_day_of_month < 1 || 31 < birth_day_of_month) //this should be a value 1-31
            ) {
        invalid_birthday = true;
    }

    return invalid_birthday;
}

std::string documentViewToJson(const bsoncxx::document::view& view) {
    std::string ret;

    if (view.data() == nullptr) {
        return "{null}";
    }

    if (view.begin() == view.end() || view.empty()) {
        return "{ }";
    }

    try {
        bson_t bson;
        bson_init_static(&bson, view.data(), view.length());

        size_t size;
        auto result = bson_as_relaxed_extended_json(&bson, &size);
        if (!result)
            return R"({"Error": "Error when converting to json"})";

        ret = std::string(result);

        bson_free(result);

    } catch (std::exception& e) {
        ret = "{\"Exception\":\"Exception when running documentViewToJson()\n";
        ret += e.what();
        ret += "\"}";
    }

    return ret;
}

AgeRangeDataObject calculateAgeRangeFromUserAge(int user_age) {

    AgeRangeDataObject age_range;

    if (13 <= user_age && user_age <= 15) { //if user is between 13 and 15
        age_range.min_age = 13;
        age_range.max_age = user_age + general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_CHILDREN;
    } else if (16 <= user_age && user_age <= 19) { //if user is 16 to 19
        age_range.min_age = user_age - general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_CHILDREN;
        age_range.max_age = user_age + general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_CHILDREN;
    } else if (20 <= user_age
               && user_age <= 18 +
                              general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS) { //if user is 20 or older match 18 to age + DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS
        age_range.min_age = 18;
        age_range.max_age = user_age + general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS;
    } else if (19 + general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS <= user_age
               && user_age <= server_parameter_restrictions::HIGHEST_ALLOWED_AGE -
                              general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS) { //if user is 24 or older match with +/- DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS years
        age_range.min_age = user_age - general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS;
        age_range.max_age = user_age + general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS;
    } else if (1 + server_parameter_restrictions::HIGHEST_ALLOWED_AGE -
               general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS <= user_age &&
               user_age <= server_parameter_restrictions::HIGHEST_ALLOWED_AGE) { //if user is ~116 or older
        age_range.min_age = user_age - general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS;
        age_range.max_age = server_parameter_restrictions::HIGHEST_ALLOWED_AGE;
    }

    if (age_range.min_age > android_specific_values::HIGHEST_DISPLAYED_AGE) {
        //The device restricts displaying ages to HIGHEST_DISPLAYED_AGE, so for consistency
        // this function will do the same thing.
        age_range.min_age = android_specific_values::HIGHEST_DISPLAYED_AGE;
    }
    if (age_range.max_age >= android_specific_values::HIGHEST_DISPLAYED_AGE) {
        //Want HIGHEST_ALLOWED_AGE so users can match up to 120 not just up to 80, when the user
        // selects 80+ inside the device it will return 120 as well.
        age_range.max_age = server_parameter_restrictions::HIGHEST_ALLOWED_AGE;
    }

    return age_range;
}

std::string makePrettyJson(const bsoncxx::document::view& document_view) {

    std::string ugly_string = documentViewToJson(document_view);
    std::string returnString;

    //adjust to desired spaces of indentation
    const int INDENT_NUMBER = 6;

    int indent = 0;
    bool modifyCloseBracket = false;

    for (size_t i = 0; i < ugly_string.length(); i++) {

        if (ugly_string[i] == '{' || ugly_string[i] == '[') {
            returnString.push_back(ugly_string[i]);

            //if the '{' is part of the displayed OID don't print it
            if ((i + 7) < ugly_string.length() &&
                !(ugly_string[i + 1] == ' '
                  && ugly_string[i + 2] == '"'
                  && ugly_string[i + 3] == '$'
                  && ugly_string[i + 4] == 'o'
                  && ugly_string[i + 5] == 'i'
                  && ugly_string[i + 6] == 'd'
                  && ugly_string[i + 7] == '"')) {
                returnString.push_back('\n');
                indent++;

                for (int j = 0; j < (INDENT_NUMBER * indent) - 1; j++) {
                    returnString.push_back(' ');
                }
            } else {
                modifyCloseBracket = true;
            }

        } else if (ugly_string[i] == ',') {
            returnString.push_back(ugly_string[i]);
            returnString.push_back('\n');

            for (int j = 0; j < (INDENT_NUMBER * indent) - 1; j++) {
                returnString.push_back(' ');
            }
        } else if (ugly_string[i] == '}' || ugly_string[i] == ']') {

            if (modifyCloseBracket) {
                modifyCloseBracket = false;
            } else {
                returnString.push_back('\n');
                indent--;
                for (int j = 0; j < (INDENT_NUMBER * indent); j++) {
                    returnString.push_back(' ');
                }
            }
            returnString.push_back(ugly_string[i]);
        } else if (ugly_string[i] == '\"') {

            int string_indentation = 0;
            for (long j = (long) returnString.size() - 1L; j >= 0L; j--) {
                if (returnString[j] == '\n') {
                    string_indentation = (int) ((long) returnString.size() - 1L - j);
                    break;
                }
            }

            returnString.push_back(ugly_string[i]);
            i++;

            while (i < ugly_string.length()) {

                if (ugly_string[i] == '\\' && i < ugly_string.length() - 1) {

                    i++;
                    switch (ugly_string[i]) {
                        case '\'':
                            returnString.push_back('\'');
                            break;
                        case '"':
                            returnString.push_back('\"');
                            break;
                        case '?':
                            returnString.push_back('\?');
                            break;
                        case '\\':
                            returnString.push_back('\\');
                            break;
                        case 'a':
                            returnString.push_back('\a');
                            break;
                        case 'b':
                            returnString.push_back('\b');
                            break;
                        case 'f':
                            returnString.push_back('\f');
                            break;
                        case 'n':
                            returnString.push_back('\n');
                            for (int j = 0; j < string_indentation; j++) {
                                returnString.push_back(' ');
                            }
                            break;
                        case 'r':
                            returnString.push_back('\r');
                            break;
                        case 't':
                            returnString.push_back('\t');
                            break;
                        case 'v':
                            returnString.push_back('\v');
                            for (int j = 0; j < string_indentation; j++) {
                                returnString.push_back(' ');
                            }
                            break;
                        default:
                            returnString.push_back(ugly_string[i]);
                            break;
                    }

                } else if (ugly_string[i] == '\"') {
                    returnString.push_back(ugly_string[i]);
                    break;
                } else {
                    returnString.push_back(ugly_string[i]);
                }
                i++;
            }

        } else {
            returnString.push_back(ugly_string[i]);
        }
    }

    return returnString;
}

bool serverInternalDeleteAccount(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& userAccountsCollection,
        mongocxx::client_session* session,
        const bsoncxx::oid& userAccountOID,
        const std::chrono::milliseconds& currentTimestamp
) {

    if (session == nullptr) {
        const std::string error_string = "serverInternalDeleteAccount() was called when session was set to nullptr.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "user_OID", userAccountOID
        );
        return false;
    }

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account;
    try {
        //find user account document
        find_user_account = userAccountsCollection.find_one_and_delete(
                *session,
                document{}
                        << "_id" << userAccountOID
                << finalize
        );
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        return false;
    }

    if (find_user_account) { //if user account was found

        mongocxx::database deletedDB = mongoCppClient[database_names::DELETED_DATABASE_NAME];
        mongocxx::collection deletedAccountsCollection = deletedDB[collection_names::DELETED_DELETED_ACCOUNTS_COLLECTION_NAME];

        //copy documents to 'deleted' collection
        bsoncxx::document::view userAccountDocumentView = *find_user_account;

        bsoncxx::stdx::optional<mongocxx::result::insert_one> insertedDocument;
        try {
            bsoncxx::builder::stream::document builder_document;

            builder_document
                    << bsoncxx::builder::concatenate(userAccountDocumentView)
                    << deleted_accounts_keys::TIMESTAMP_REMOVED << bsoncxx::types::b_date{currentTimestamp};

            //insert document into 'deleted documents' collection
            insertedDocument = deletedAccountsCollection.insert_one(*session, builder_document.view());
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), e.what(),
                    "database", database_names::DELETED_DATABASE_NAME,
                    "collection", collection_names::DELETED_DELETED_ACCOUNTS_COLLECTION_NAME,
                    "user_account_to_be_deleted", userAccountDocumentView
            );
            return false;
        }

        if (!insertedDocument || insertedDocument->result().inserted_count() != 1) {
            const std::string error_string = "Failed to upsert user account document to deleted collection.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::DELETED_DATABASE_NAME,
                    "collection", collection_names::DELETED_DELETED_ACCOUNTS_COLLECTION_NAME,
                    "user_account_to_be_deleted", userAccountDocumentView
            );
            return false;
        }

        std::string phoneNumber;
        int numberSwipesRemaining;
        long long swipesLastUpdatedTime;
        std::chrono::milliseconds timeSMSCanBeSentAgain;
        std::chrono::milliseconds timeEmailCanBeSentAgain;

        auto phoneNumberElement = userAccountDocumentView[user_account_keys::PHONE_NUMBER];
        if (phoneNumberElement
            && phoneNumberElement.type() == bsoncxx::type::k_utf8
                ) { //if it exists and is type utf8
            phoneNumber = phoneNumberElement.get_string().value.to_string();
        } else { //if it does not exist or is not type utf8
            logElementError(__LINE__, __FILE__,
                            phoneNumberElement, userAccountDocumentView,
                            bsoncxx::type::k_utf8, user_account_keys::PHONE_NUMBER,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);
            return false;
        }

        auto numberSwipesRemainingElement = userAccountDocumentView[user_account_keys::NUMBER_SWIPES_REMAINING];
        if (numberSwipesRemainingElement
            && numberSwipesRemainingElement.type() == bsoncxx::type::k_int32
                ) { //if it exists and is type int32
            numberSwipesRemaining = numberSwipesRemainingElement.get_int32().value;
        } else { //if it does not exist or is not type int32
            logElementError(__LINE__, __FILE__,
                            numberSwipesRemainingElement, userAccountDocumentView,
                            bsoncxx::type::k_int32, user_account_keys::NUMBER_SWIPES_REMAINING,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);
            return false;
        }

        auto swipesLastUpdatedTimeElement = userAccountDocumentView[user_account_keys::SWIPES_LAST_UPDATED_TIME];
        if (swipesLastUpdatedTimeElement
            && swipesLastUpdatedTimeElement.type() == bsoncxx::type::k_int64
                ) { //if it exists and is type int64
            swipesLastUpdatedTime = swipesLastUpdatedTimeElement.get_int64().value;
        } else { //if it does not exist or is not type int64
            logElementError(__LINE__, __FILE__,
                            swipesLastUpdatedTimeElement, userAccountDocumentView,
                            bsoncxx::type::k_int64, user_account_keys::SWIPES_LAST_UPDATED_TIME,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);
            return false;
        }

        auto timeSMSCanBeSentAgainElement = userAccountDocumentView[user_account_keys::TIME_SMS_CAN_BE_SENT_AGAIN];
        if (timeSMSCanBeSentAgainElement
            && timeSMSCanBeSentAgainElement.type() == bsoncxx::type::k_date
                ) { //if it exists and is type int64
            timeSMSCanBeSentAgain = timeSMSCanBeSentAgainElement.get_date().value;
        } else { //if it does not exist or is not type date
            logElementError(__LINE__, __FILE__,
                            timeSMSCanBeSentAgainElement, userAccountDocumentView,
                            bsoncxx::type::k_date, user_account_keys::TIME_SMS_CAN_BE_SENT_AGAIN,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME);
            return false;
        }

        auto timeEmailCanBeSentAgainElement = userAccountDocumentView[user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN];
        if (timeEmailCanBeSentAgainElement
            && timeEmailCanBeSentAgainElement.type() == bsoncxx::type::k_date
                ) { //if it exists and is type date
            timeEmailCanBeSentAgain = timeEmailCanBeSentAgainElement.get_date().value;
        } else { //if it does not exist or is not type date
            logElementError(__LINE__, __FILE__,
                            timeEmailCanBeSentAgainElement, userAccountDocumentView,
                            bsoncxx::type::k_date, user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME);
            return false;
        }

        mongocxx::collection infoStoredAfterDeletionCollection = accountsDB[collection_names::INFO_STORED_AFTER_DELETION_COLLECTION_NAME];

        //update separate info document
        bsoncxx::stdx::optional<mongocxx::result::update> updateInfoStoredAfterDeletion;
        try {
            updateInfoStoredAfterDeletion = infoStoredAfterDeletionCollection.update_one(
                *session,
                document{}
                    << info_stored_after_deletion_keys::PHONE_NUMBER << phoneNumber
                    << finalize,
                document{}
                    << "$set" << open_document
                    << info_stored_after_deletion_keys::TIME_SMS_CAN_BE_SENT_AGAIN << bsoncxx::types::b_date{timeSMSCanBeSentAgain}
                    << info_stored_after_deletion_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << bsoncxx::types::b_date{timeEmailCanBeSentAgain}
                    << info_stored_after_deletion_keys::NUMBER_SWIPES_REMAINING << numberSwipesRemaining
                    << info_stored_after_deletion_keys::SWIPES_LAST_UPDATED_TIME << bsoncxx::types::b_int64{swipesLastUpdatedTime}
                    << close_document
                << finalize
            );
        } catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), e.what(),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", userAccountOID,
                    "phoneNumber", phoneNumber
            );

            return false;
        }

        if (!updateInfoStoredAfterDeletion || updateInfoStoredAfterDeletion->matched_count() != 1) {
            const std::string error_string = "SeparateInfo collection document should exist for every user after the first login, however it was not found.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", userAccountOID,
                    "phoneNumber", phoneNumber
            );

            //NOTE: OK to continue here, if they make another account with this phone number it should create the separate info account
        }

        bsoncxx::array::view userChatRooms;

        auto chatRoomsUserIsPartOfElement = userAccountDocumentView[user_account_keys::CHAT_ROOMS];
        if (chatRoomsUserIsPartOfElement
            && chatRoomsUserIsPartOfElement.type() == bsoncxx::type::k_array
                ) { //if element exists and is type utf8
            userChatRooms = chatRoomsUserIsPartOfElement.get_array().value;
        } else { //if element does not exist or is not type utf8
            logElementError(
                    __LINE__, __FILE__,
                    chatRoomsUserIsPartOfElement, userAccountDocumentView,
                    bsoncxx::type::k_array, user_account_keys::CHAT_ROOMS,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            return false;
        }

        if (userChatRooms.begin() != userChatRooms.end()) { //if user is a member of chat rooms
            mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];

            //set all chat rooms to deleted status
            for (const auto& chatRoom: userChatRooms) {
                if (chatRoom.type() == bsoncxx::type::k_document) {

                    bsoncxx::document::view chatRoomDocView = chatRoom.get_document().value;
                    std::string chatRoomId;

                    auto chatRoomsIdElement = chatRoomDocView[user_account_keys::chat_rooms::CHAT_ROOM_ID];
                    if (chatRoomsIdElement
                        && chatRoomsIdElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                        chatRoomId = chatRoomsIdElement.get_string().value.to_string();
                    } else { //if element does not exist or is not type utf8
                        logElementError(__LINE__, __FILE__, chatRoomsIdElement,
                                        chatRoomDocView, bsoncxx::type::k_utf8,
                                        user_account_keys::chat_rooms::CHAT_ROOM_ID,
                                        database_names::ACCOUNTS_DATABASE_NAME,
                                        collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                        return false;
                    }

                    std::chrono::milliseconds temp_timestamp = currentTimestamp;
                    //this has an error attached to it, however if it fails just continue
                    leaveChatRoom(
                            chatRoomDB,
                            accountsDB,
                            userAccountsCollection,
                            session,
                            userAccountDocumentView,
                            chatRoomId,
                            userAccountOID,
                            temp_timestamp
                    );
                } else {
                    logElementError(
                            __LINE__, __FILE__,
                            chatRoomsUserIsPartOfElement, userAccountDocumentView,
                            bsoncxx::type::k_array, user_account_keys::CHAT_ROOMS,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );

                    //NOTE: ok to continue here
                }
            }
        }

        //NOTE: Pictures must be deleted AFTER chat rooms are left so references can be properly updated.
        //delete pictures associated with the account
        // and extract the user thumbnail
        if (!deleteAccountPictures(
                mongoCppClient,
                accountsDB,
                session,
                currentTimestamp,
                userAccountDocumentView)
                ) { //if delete pictures fails
            return false;
        }

        //delete the user account statistics document
        mongocxx::collection user_account_statistics_collection = accountsDB[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];

        moveUserAccountStatisticsDocument(
                mongoCppClient,
                userAccountOID,
                user_account_statistics_collection,
                currentTimestamp,
                false,
                session
        );

        mongocxx::collection email_verification_collection = accountsDB[collection_names::EMAIL_VERIFICATION_COLLECTION_NAME];
        mongocxx::collection account_recovery_collection = accountsDB[collection_names::ACCOUNT_RECOVERY_COLLECTION_NAME];

        //remove email verification document if it exists
        try {
            email_verification_collection.delete_one(
                    *session,
                    document{}
                            << email_verification_keys::USER_ACCOUNT_REFERENCE << userAccountOID
                            << finalize
            );
        } catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), e.what(),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", userAccountOID,
                    "phoneNumber", phoneNumber);
            return false;
        }

        //remove account recovery document if it exists
        try {
            account_recovery_collection.delete_one(
                    *session,
                    document{}
                            << account_recovery_keys::PHONE_NUMBER << phoneNumber
                            << finalize
            );
        } catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), e.what(),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", userAccountOID,
                    "phoneNumber", phoneNumber
            );
            return false;
        }

        moveOutstandingReportsToHandled(
                mongoCppClient,
                currentTimestamp,
                userAccountOID,
                "",
                ReportHandledMoveReason::REPORT_HANDLED_REASON_REPORTS_DELETED,
                session,
                DisciplinaryActionTypeEnum(-1)
        );

    } else {} //if user account was not found

    return true;
}

bool deleteAccountPictures(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::client_session* session,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::document::view& user_account_doc_view
) {

    auto pictures_element = user_account_doc_view[user_account_keys::PICTURES];

    if (pictures_element && pictures_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
        bsoncxx::array::view user_account_pictures_list = pictures_element.get_array().value;

        mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

        for (const auto& ele: user_account_pictures_list) {
            if (ele && ele.type() == bsoncxx::type::k_document) { //the array element is a document
                bsoncxx::document::view picture_doc = ele.get_document().value;

                auto picture_oid_element = picture_doc[user_account_keys::pictures::OID_REFERENCE];
                if (picture_oid_element &&
                    picture_oid_element.type() == bsoncxx::type::k_oid) { // a picture exists for this index
                    auto pictureOid = picture_oid_element.get_oid().value;

                    //delete old picture
                    if (!findAndDeletePictureDocument(
                            mongo_cpp_client,
                            user_pictures_collection,
                            pictureOid,
                            currentTimestamp,
                            session)
                            ) {
                        return false;
                    }
                } else {
                    logElementError(__LINE__, __FILE__, pictures_element,
                                    user_account_doc_view, bsoncxx::type::k_oid,
                                    user_account_keys::pictures::OID_REFERENCE,
                                    database_names::ACCOUNTS_DATABASE_NAME,
                                    collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                    return false;
                }
            } else if (ele && ele.type() == bsoncxx::type::k_null) {} // simply means no index at this array
            else {
                logElementError(__LINE__, __FILE__, pictures_element,
                                user_account_doc_view, bsoncxx::type::k_document, user_account_keys::PICTURES,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                return false;
            }
        }
    } else { //if element does not exist or is not type array
        logElementError(__LINE__, __FILE__, pictures_element,
                        user_account_doc_view, bsoncxx::type::k_array, user_account_keys::PICTURES,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    return true;
}

bool findAndDeletePictureDocument(
        mongocxx::client& mongoCppClient,
        mongocxx::collection& picturesCollection,
        const bsoncxx::oid& picturesDocumentOID,
        const std::chrono::milliseconds& currentTimestamp,
        mongocxx::client_session* session
) {

    bool successfullyCompleted = true;

    bsoncxx::stdx::optional<bsoncxx::document::value> find_picture_document;
    try {

        //find picture document
        find_picture_document = find_one_optional_session(
                session,
                picturesCollection,
                document{}
                        << "_id" << picturesDocumentOID
                        << finalize
        );

    }
    catch (const mongocxx::logic_error& e) {
        std::optional<std::string> exceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exceptionString, std::string(e.what()),
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME);
        successfullyCompleted = false;
    }

    if (find_picture_document) {

        bsoncxx::document::view picture_document_view = find_picture_document->view();

        mongocxx::database deleted_db = mongoCppClient[database_names::DELETED_DATABASE_NAME];
        mongocxx::collection deleted_pictures_collection = deleted_db[collection_names::DELETED_USER_PICTURES_COLLECTION_NAME];

        successfullyCompleted = deletePictureDocument(
                picture_document_view,
                picturesCollection,
                deleted_pictures_collection,
                picturesDocumentOID,
                currentTimestamp,
                ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
                "",
                session
        );

    }

    return successfullyCompleted;
}

bool deletePictureDocument(
        const bsoncxx::document::view& pictureDocumentView,
        mongocxx::collection& picturesCollection,
        mongocxx::collection& deletedPicturesCollection,
        const bsoncxx::oid& picturesDocumentOID,
        const std::chrono::milliseconds& currentTimestamp,
        ReasonPictureDeleted reason_picture_deleted,
        const std::string& admin_name, //only used if ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE
        mongocxx::client_session* session
) {

    bool successfullyCompleted = true;

    //NOTE: Not sure if doing it this way with bsoncxx::builder::concatenate() (it will make
    // a copy) or calling update_one() is faster. Doing it this way to save a database call.
    bsoncxx::builder::stream::document builder;

    builder
            << bsoncxx::builder::concatenate(pictureDocumentView)
            << deleted_user_pictures_keys::TIMESTAMP_REMOVED << bsoncxx::types::b_date{currentTimestamp}
            << deleted_user_pictures_keys::REASON_REMOVED << reason_picture_deleted;

    if (reason_picture_deleted == ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE) {
        builder
                << deleted_user_pictures_keys::ADMIN_NAME << admin_name;
    }

    //upsert picture into delete collection to save a copy before deleting it
    bsoncxx::stdx::optional<mongocxx::result::insert_one> insertedDocument;
    std::optional<std::string> inserted_document_exception_string;
    try {
        //upsert document into 'deleted documents' collection
        //NOTE: the deleted pictures collection can be searched and so want the _id to remain the same
        // as it was in the user pictures collection for indexing
        insertedDocument = insert_one_optional_session(
                session,
                deletedPicturesCollection,
                builder.view()
        );
    } catch (const mongocxx::logic_error& e) {
        inserted_document_exception_string = std::string(e.what());
    }

    if (insertedDocument) { //if upsert was successful
        bsoncxx::stdx::optional<mongocxx::result::delete_result> deletePictureSuccess;
        std::optional<std::string> delete_picture_exception_string;
        try {

            //delete picture document
            deletePictureSuccess = delete_one_optional_session(
                    session,
                    picturesCollection,
                    document{}
                            << "_id" << picturesDocumentOID
                            << finalize
            );
        }
        catch (const mongocxx::logic_error& e) {
            delete_picture_exception_string = std::string(e.what());
        }

        if (!deletePictureSuccess) {
            std::string errorString = "Failed to delete document from collection.";

            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                          delete_picture_exception_string,
                                          errorString,
                                          "database", database_names::ACCOUNTS_DATABASE_NAME,
                                          "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                                          "ObjectID_used", picturesDocumentOID);

            successfullyCompleted = false;

        }

    } else { //if upsert failed

        std::string errorString = "Failed to upsert document to collection.";

        successfullyCompleted = false;

        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      inserted_document_exception_string, errorString,
                                      "database", database_names::DELETED_DATABASE_NAME,
                                      "collection", collection_names::DELETED_USER_PICTURES_COLLECTION_NAME,
                                      "OidSearchedFor", picturesDocumentOID);

    }

    return successfullyCompleted;
}

bool removePictureArrayElement(
        int index_of_array,
        mongocxx::collection& user_accounts_collection,
        const bsoncxx::document::view& user_account_doc_view,
        const bsoncxx::oid& user_account_oid,
        mongocxx::client_session* session
) {

    if (index_of_array >= general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, std::optional<std::string>(),
                std::string("Passed picture index value is too large."),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "indexOfArray", std::to_string(index_of_array),
                "NUMBER_PICTURES_STORED_PER_ACCOUNT", std::to_string(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT)
        );

        return false;
    }

    std::optional<std::string> exception_string;
    bsoncxx::stdx::optional<mongocxx::result::update> update_user_account;
    try {

        //update the array element to null
        update_user_account = update_one_optional_session(
                session,
                user_accounts_collection,
                document{}
                        << "_id" << user_account_oid
                        << finalize,
                document{}
                        << "$set" << open_document
                        << user_account_keys::PICTURES + "." +
                           std::to_string(index_of_array) << bsoncxx::types::b_null{}
                        << close_document
                        << finalize
        );

    }
    catch (const mongocxx::logic_error& e) {
        exception_string = std::string(e.what());
    }

    if (!update_user_account || update_user_account->matched_count() < 1) {
        const std::string error_string = std::string("updating picture array '")
                .append(user_account_keys::PICTURES)
                .append(".")
                .append(std::to_string(index_of_array))
                .append("' failed.");

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "verifiedAccountDocument", user_account_doc_view,
                "Searched_OID", user_account_oid);

        return false;
    }

    return true;
}

bool extractMinMaxAgeRange(
        const bsoncxx::document::view& matching_account_view,
        int& min_age_range,
        int& max_age_range
) {

    try {

        bsoncxx::document::view age_range_doc = extractFromBsoncxx_k_document(
                matching_account_view,
                user_account_keys::AGE_RANGE
        );

        min_age_range = extractFromBsoncxx_k_int32(
                age_range_doc,
                user_account_keys::age_range::MIN
        );

        max_age_range = extractFromBsoncxx_k_int32(
                age_range_doc,
                user_account_keys::age_range::MAX
        );

    } catch (const ErrorExtractingFromBsoncxx& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "doc_view", matching_account_view
        );
        return false;
    }

    if (min_age_range < 1 || max_age_range < 1) {
        const std::string error_string = "min or max age range value was too low after it should be set";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "document", matching_account_view
        );

        return false;
    }

    return true;
}

//this function will check if the user has "everyone" selected as their genders to match with
//returning false here means an error occurred, the bool passed is the parameter
MatchesWithEveryoneReturnValue checkIfUserMatchesWithEveryone(
        const bsoncxx::array::view& genders_to_match_mongo_db_array,
        const bsoncxx::document::view& user_account_doc
) {

    MatchesWithEveryoneReturnValue return_value = MatchesWithEveryoneReturnValue::MATCHES_WITH_EVERYONE_ERROR_OCCURRED;

    //if gender range size is 1 and first element is MATCH_EVERYONE_GENDER_RANGE_VALUE
    long genderRangeSize = std::distance(genders_to_match_mongo_db_array.begin(),
                                         genders_to_match_mongo_db_array.end());
    if (genderRangeSize == 1) {
        auto gender = genders_to_match_mongo_db_array.find(0);

        if ((*gender) && gender->type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
            if (gender->get_string().value.to_string() == general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE) {
                return_value = MatchesWithEveryoneReturnValue::USER_MATCHES_WITH_EVERYONE;
            } else {
                return_value = MatchesWithEveryoneReturnValue::USER_DOES_NOT_MATCH_WITH_EVERYONE;
            }
        } else { //if element does not exist or is not type utf8

            std::string errorString = "gender was not type utf8";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, std::optional<std::string>(), errorString,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "document", user_account_doc,
                    "type", (*gender) ? convertBsonTypeToString(gender->type()) : "nullptr",
                    "gender_range_arr", makePrettyJson(genders_to_match_mongo_db_array)
            );

            return_value = MatchesWithEveryoneReturnValue::MATCHES_WITH_EVERYONE_ERROR_OCCURRED;
        }

    } else {
        return_value = MatchesWithEveryoneReturnValue::USER_DOES_NOT_MATCH_WITH_EVERYONE;
    }

    return return_value;
}

tm initializeTmByDate(int birthYear, int birthMonth, int birthDayOfMonth) {

    tm setupTime{};

    setupTime.tm_sec = 0;
    setupTime.tm_min = 0;
    setupTime.tm_hour = 0;
    setupTime.tm_mday = birthDayOfMonth;
    setupTime.tm_mon = birthMonth - 1;
    setupTime.tm_year = birthYear - 1900;
    setupTime.tm_isdst = -1;

    //NOTE: gmtime() (gmtime_r() for thread safety) should be used with timegm() for GMT time zone.
    // If necessary, localtime() should be used with mktime() for local time zone.
    //Using GMT time for calculating dates universally in the program, otherwise odd problems
    // could occur where

    time_t lTimeEpoch = timegm(&setupTime);

    tm dateTimeStruct{};
    gmtime_r(&lTimeEpoch, &dateTimeStruct);

    return dateTimeStruct;
}

//year needs to be a (4 digit most likely) value ex: 20xx
//month needs to be a value 1-12
//day needs to be a value 1-31
//NOTE: returns -1 if function fails
int calculateAge(const std::chrono::milliseconds& currentTime, int birthYear, int birthMonth, int birthDayOfMonth) {

    return calculateAge(
            currentTime,
            birthYear,
            initializeTmByDate(birthYear, birthMonth, birthDayOfMonth).tm_yday
    );
}

//year needs to be a (4 digit most likely) value ex: 20xx
//day of year needs to be a value [0-365] (same as tm_yday, ideally extracted from tm_yday)
//NOTE: returns -1 if function fails
int calculateAge(const std::chrono::milliseconds& currentTime, int birthYear, int birthDayOfYear) {

    const time_t timeObject = currentTime.count() / 1000;

    tm dateTimeStruct{};
    gmtime_r(&timeObject, &dateTimeStruct);

    if (dateTimeStruct.tm_year == 0) { //failed to calculate proper date time
        //handle the error in calling function
        return -1;
    }

    int currentYear = dateTimeStruct.tm_year + 1900;

    if (birthYear < 1900 || currentYear < birthYear
        || birthDayOfYear < 0 || 365 < birthDayOfYear) {
        return -1;
    }

    //LEAP YEARS
    //if a year is divisible by 4
    //AND it is not divisible by 100
    //UNLESS it is divisible by 400
    //it is a leap year
    //NOTE: 2000 is a leap year and its not like we are going to have multiple centuries this thing spans... but I will leave the logic line below for completion sake
    //if((today.get(Calendar.YEAR) % 4 == 0 && today.get(Calendar.YEAR) % 100 != 0) || (today.get(Calendar.YEAR) % 4 == 0 && today.get(Calendar.YEAR) % 400 == 0))

    int age = currentYear - birthYear;

    if (0 > dateTimeStruct.tm_yday - birthDayOfYear) {
        age--;
    }

    return age;
}

//returns false if version is valid, true if not valid
bool isInvalidLetsGoDesktopInterfaceVersion(const unsigned int letsGoVersion) {

    //callerVersion should never be 0 because it is the default value from gRPC
    if (letsGoVersion == 0
        || letsGoVersion <
           general_values::MINIMUM_ACCEPTED_DESKTOP_INTERFACE_VERSION) { //check if meets minimum version requirement
        return true;
    }

    return false;
}

bool validAdminInfo(
        const std::string& name,
        const std::string& password,
        const std::function<void(bsoncxx::stdx::optional<bsoncxx::document::value>& admin_info_doc,
                                 const std::string& error_message)>& store_admin_info
) {

    if (!store_admin_info) { //if store admin was not set
        return false;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection adminAccountsCollection = accountsDB[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc;
    try {
        mongocxx::options::find opts;

        opts.projection(
                document{}
                        << admin_account_key::PASSWORD << 0
                        << finalize
        );

        admin_info_doc = adminAccountsCollection.find_one(document{}
                                                                  << admin_account_key::NAME << name
                                                                  << admin_account_key::PASSWORD << password
                                                                  << finalize,
                                                          opts);

    } catch (const mongocxx::logic_error& e) {
        store_admin_info(admin_info_doc, e.what());
        return false;
    }

    if (!admin_info_doc) {
        store_admin_info(admin_info_doc, "Admin account does not exist.");
        return false;
    }

    store_admin_info(admin_info_doc, "");
    return true;
}

ReturnStatus isLoginToServerBasicInfoValid(
        LoginTypesAccepted login_types_accepted,
        const LoginToServerBasicInfo& login_info,
        std::string& user_account_oid,
        std::string& logged_in_token_str,
        std::string& installation_id,
        const std::function<void(bsoncxx::stdx::optional<bsoncxx::document::value>& admin_info_doc,
                                 const std::string& error_message)>& store_admin_info
) {
    user_account_oid = login_info.current_account_id();

    if (
            (login_types_accepted == LOGIN_TYPE_ONLY_CLIENT
             && login_info.admin_info_used())
            ||
            (login_types_accepted == LOGIN_TYPE_ONLY_ADMIN
             && !login_info.admin_info_used())
            ) {
#ifdef LG_TESTING
        std::cout << "isLoginToServerBasicInfoValid(): INVALID_PARAM\n";
#endif
        return ReturnStatus::INVALID_PARAMETER_PASSED;
    }

    if (login_info.admin_info_used()) {

        if (isInvalidLetsGoDesktopInterfaceVersion(login_info.lets_go_version())) {
            return ReturnStatus::OUTDATED_VERSION;
        } else if (validAdminInfo(login_info.admin_name(), login_info.admin_password(), store_admin_info)) {
            return ReturnStatus::SUCCESS;
        } else {
            return ReturnStatus::LG_ERROR;
        }

    } else {
        if (isInvalidOIDString(user_account_oid)) { //check if account OID is valid
            return ReturnStatus::INVALID_USER_OID;
        }

        if (isInvalidLetsGoAndroidVersion(login_info.lets_go_version())) { //check if meets minimum version requirement
            return ReturnStatus::OUTDATED_VERSION;
        }

        logged_in_token_str = login_info.logged_in_token();

        if (isInvalidOIDString(logged_in_token_str)) { //check if logged in token is 24 size
            return ReturnStatus::INVALID_LOGIN_TOKEN;
        }

        installation_id = login_info.installation_id();

        if (isInvalidUUID(installation_id)) { //check if installation_id is valid
            return ReturnStatus::INVALID_INSTALLATION_ID;
        }

    }

    //NOTE: this is meant to show isLoginToServerBasicInfoValid() found no errors, not to
    // show that the calling function was successful
    return ReturnStatus::SUCCESS;
}

//returns false if OID is valid, true if not valid
bool isInvalidOIDString(const std::string& stringOID) {

    //documentation for bsoncxx::oid says throws exception if not an oid sized hex string
    // so this should be the same thing w/o having to use an exception
    if (stringOID.length() == 24 && std::all_of(stringOID.begin(), stringOID.end(), ::isxdigit)) {
        return false;
    }

    return true;
}

//returns false if version is valid, true if not valid
bool isInvalidLetsGoAndroidVersion(const unsigned int letsGoVersion) {

    //callerVersion should never be 0 because it is the default value from gRPC
    if (letsGoVersion == 0
        ||
        letsGoVersion < general_values::MINIMUM_ACCEPTED_ANDROID_VERSION) { //check if meets minimum version requirement
        return true;
    }

    return false;
}

//returns a UUID in string form
std::string generateUUID() {
    //random_generator is not thread safe
    static thread_local boost::uuids::random_generator gen;
    return boost::uuids::to_string(gen());
}

//returns false if installationID is valid, true if not valid
//example of a valid UUID string is below
//4b66a009-df40-42e6-87f6-570efab19941
bool isInvalidUUID(const std::string& uuid_as_string) {

    static const int UUID_STRING_LENGTH = 36;

    //The size of a UUID.
    if (uuid_as_string.size() != UUID_STRING_LENGTH) {
        return true;
    }

    //NOTE: The below code is needed, because if an invalid char such as '$' makes it into
    // an aggregation pipeline (as an installationId specifically), it can cause an exception and crash the server.
    static const std::array<int, 5> segments = {8, 4, 4, 4, 12};

    for (int i = 0, j = 0; i < UUID_STRING_LENGTH; i++, j++) {
        for (int k = 0; k < segments[j]; k++, i++) {
            if (!isalnum(uuid_as_string[i])) {
                return true;
            }
        }
        if (i < UUID_STRING_LENGTH && uuid_as_string[i] != '-') {
            return true;
        }
    }

    return false;
}

//returns false if email is valid, true if not valid
bool isInvalidEmailAddress(const std::string& email) {

    if (email.size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES || email.empty()
            ) { //if email size is over allowed char limit or empty
        return true;
    }

    static const std::regex e(general_values::EMAIL_AUTHORIZATION_REGEX);

    return !std::regex_match(email, e);

}

//this takes in a MemberSharedInfoMessage and sets the CategoryActivityMessage to the values inside matchCategories
//it assumes that except for the first one each stop time has a start time immediately before it
//NOTE: This does no cleaning of the match account.
bool saveActivitiesToMemberSharedInfoMessage(
        const bsoncxx::oid& matchAccountOID,
        const bsoncxx::array::view& matchCategories,
        MemberSharedInfoMessage* responseMessage,
        const bsoncxx::document::view& matchingAccountDocument
) {

    int num_activities = 0;
    try {
        for (auto activity_element: matchCategories) {

            const bsoncxx::document::view activity_doc = extractFromBsoncxxArrayElement_k_document(
                    activity_element
            );

            const auto category_or_activity_type =
                    AccountCategoryType(
                            extractFromBsoncxx_k_int32(
                                    activity_doc,
                                    user_account_keys::categories::TYPE
                            )
                    );

            //exclude categories from return
            if (category_or_activity_type != AccountCategoryType::ACTIVITY_TYPE) {
                continue;
            }

            num_activities++;
            auto response_activity = responseMessage->add_activities();

            response_activity->set_activity_index(
                    extractFromBsoncxx_k_int32(
                            activity_doc,
                            user_account_keys::categories::INDEX_VALUE
                    )
            );

            const bsoncxx::array::view activity_time_frames = extractFromBsoncxx_k_array(
                    activity_doc,
                    user_account_keys::categories::TIMEFRAMES
            );

            int time_frame_index = 0;
            CategoryTimeFrameMessage* time_frame_pointer = nullptr;
            for (auto time_frame_element: activity_time_frames) {

                const bsoncxx::document::view time_frame_doc = extractFromBsoncxxArrayElement_k_document(
                        time_frame_element
                );

                const long long time = extractFromBsoncxx_k_int64(
                        time_frame_doc,
                        user_account_keys::categories::timeframes::TIME
                );

                const int start_stop_value = extractFromBsoncxx_k_int32(
                        time_frame_doc,
                        user_account_keys::categories::timeframes::START_STOP_VALUE
                );

                if (time_frame_index == 0
                    && start_stop_value == -1) { //first element is a stop time then upsert a start time of -1 first
                    time_frame_pointer = response_activity->add_time_frame_array();
                    time_frame_pointer->set_start_time_frame(-1);
                    time_frame_pointer->set_stop_time_frame(time);
                } else if (start_stop_value == -1
                           && time_frame_pointer != nullptr) { //element is a stop time and is not first element
                    time_frame_pointer->set_stop_time_frame(time);
                } else if (start_stop_value == 1) { //element is a start time
                    time_frame_pointer = response_activity->add_time_frame_array();
                    time_frame_pointer->set_start_time_frame(time);
                } else { //this means is not first element, is a stop time and timeFramePointer == nullptr
                    const std::string error_string = "time frame is not first element, is a stop time and timeFramePointer == nullptr.";

                    std::string additional_info_string = "timeFrameIndex: ";
                    additional_info_string += std::to_string(time_frame_index);
                    additional_info_string += " startStopTime: ";
                    additional_info_string += std::to_string(start_stop_value);
                    additional_info_string += "time: ";
                    additional_info_string += std::to_string(time);
                    additional_info_string += '\n';

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection",
                            collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "document", matchingAccountDocument,
                            "time_frame_element", additional_info_string,
                            "oID_Used", matchAccountOID
                    );

                    return false;
                }

                time_frame_index++;
            }

            if (time_frame_index / 2 >
                server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY) { //if there are more time frames than should be allowed stored in the activity

                const std::string error_string = "Too many time frame array elements in activity.";

                std::string time_frames_string;
                for (auto i: activity_time_frames) {
                    time_frames_string +=
                            "time: " +
                            getDateTimeStringFromTimestamp(std::chrono::milliseconds{
                                    i[user_account_keys::categories::timeframes::TIME].get_int64().value});
                    time_frames_string += " startStopTime: " + std::to_string(
                            i[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value);
                    time_frames_string += '\n';
                }

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "list_of_time_frames", time_frames_string,
                        "document", matchingAccountDocument,
                        "oID_Used", matchAccountOID
                );

                //NOTE: no need to end here this is recoverable
            }
        }
    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        return false;
    }

    if (num_activities == 0) { //if no categories were found

        //these are mandatory to fill out so there should always be at least 1
        const std::string error_string =
                "No categories were found in the matching account document even though the field was found.";

        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            std::optional<std::string>(), error_string,
            "database", database_names::ACCOUNTS_DATABASE_NAME,
            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
            "document", matchingAccountDocument,
            "oID_Used", matchAccountOID
        );

        return false;
    } else if (num_activities > server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT) {
        const std::string error_string = "Too many activities stored.";

        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            std::optional<std::string>(), error_string,
            "database", database_names::ACCOUNTS_DATABASE_NAME,
            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
            "extracted_from_document", matchingAccountDocument,
            "oID_Used", matchAccountOID
        );

        //NOTE: no need to end here this is recoverable
    }

    return true;
}

bool removeExcessPicturesIfNecessary(
        mongocxx::client& mongo_cpp_client,
        mongocxx::collection& user_account_collection,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::document::view& user_account_doc_view,
        std::vector<std::pair<std::string, std::chrono::milliseconds>>& memberPictureTimestamps
) {

    if (memberPictureTimestamps.size() > general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) {

        std::string errorString = "Array containing pictures was greater than NUMBER_PICTURES_STORED_PER_ACCOUNT.";

        std::optional<std::string> exceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exceptionString, errorString,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                      "document", user_account_doc_view,
                                      "oID_Used", user_account_oid);

        bsoncxx::builder::basic::array newPicturesArray;

        //build array to be inserted into verified account
        for (unsigned int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {

            if (memberPictureTimestamps[i].first.empty()) { //if element does not exist
                newPicturesArray.append(bsoncxx::types::b_null{});
            } else { //if element does exist
                newPicturesArray.append(
                        document{}
                                << user_account_keys::pictures::OID_REFERENCE << bsoncxx::oid{
                                memberPictureTimestamps[i].first}
                                << user_account_keys::pictures::TIMESTAMP_STORED << bsoncxx::types::b_date{
                                memberPictureTimestamps[i].second}
                                << finalize
                );
            }
        }

        std::optional<std::string> updateVerifiedAccountExceptionString;
        bsoncxx::stdx::optional<mongocxx::v_noabi::result::update> updateVerifiedAccount;
        try {

            //change the failed array
            updateVerifiedAccount = user_account_collection.update_one(
                    document{}
                            << "_id" << user_account_oid
                            << finalize,
                    document{}
                            << "$set" << open_document
                            << user_account_keys::PICTURES << newPicturesArray.view()
                            << close_document
                            << finalize);
        }
        catch (const mongocxx::logic_error& e) {
            updateVerifiedAccountExceptionString = std::string(e.what());
        }

        if (!updateVerifiedAccount || updateVerifiedAccount->modified_count() != 1) { //update verified account failed
            std::string updateFailedErrorString = "Update verified account failed.";

            bsoncxx::builder::stream::document arrayDoc;
            arrayDoc << user_account_keys::PICTURES << newPicturesArray.view();

            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                          updateVerifiedAccountExceptionString,
                                          updateFailedErrorString,
                                          "database", database_names::ACCOUNTS_DATABASE_NAME,
                                          "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                          "document", user_account_doc_view,
                                          "oID_Used", user_account_oid,
                                          "array_attempted", arrayDoc.view());

            memberPictureTimestamps.clear();
            return false;
        } else { //update verified account succeeded
            std::vector<std::string> picturesToDelete;

            while (memberPictureTimestamps.size() > general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) {
                std::string& picture_oid = memberPictureTimestamps.back().first;
                if (!picture_oid.empty()) {
                    picturesToDelete.push_back(picture_oid);
                }
                memberPictureTimestamps.pop_back();
            }

            mongocxx::collection userPicturesCollection = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME][collection_names::USER_PICTURES_COLLECTION_NAME];

            for (const auto& pic: picturesToDelete) {

                //remove the picture from account pictures collection
                findAndDeletePictureDocument(mongo_cpp_client, userPicturesCollection, bsoncxx::oid{pic},
                                             currentTimestamp);
            }
        }
    }
    return true;
}

//extracts the picture elements stored inside pictureTimestampsArray and saves them to memberPictureTimestamps
//also saves a map of the indexes that store pictures to indexUsed map
//returns true if at least one element was found, false otherwise
//this may not be as large as NUMBER_PICTURES_STORED_PER_ACCOUNT however it will never be larger
bool extractPicturesToVectorFromChatRoom(
        mongocxx::client& mongo_cpp_client,
        mongocxx::collection& user_account_collection,
        const bsoncxx::array::view& pictureTimestampsArray,
        const bsoncxx::document::view& user_account_doc_view,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& currentTimestamp,
        std::vector<std::pair<std::string, std::chrono::milliseconds>>& memberPictureTimestamps,
        std::set<int>& indexUsed) {

    int index = 0;
    for (const auto& chatRoomPictureEle: pictureTimestampsArray) {
        if (chatRoomPictureEle.type() == bsoncxx::type::k_document) {

            bsoncxx::document::view picturesDoc = chatRoomPictureEle.get_document().value;
            std::chrono::milliseconds pictureTimestamp;
            std::string pictureOidString;

            //extract member picture oid elements
            auto pictureOidElement = picturesDoc[user_account_keys::pictures::OID_REFERENCE];
            if (pictureOidElement &&
                pictureOidElement.type() == bsoncxx::type::k_oid) { //if element exists and is type oid
                pictureOidString = pictureOidElement.get_oid().value.to_string();
            } else { //if element does not exist or is not type oid
                logElementError(__LINE__, __FILE__, pictureOidElement,
                                picturesDoc, bsoncxx::type::k_oid, user_account_keys::pictures::OID_REFERENCE,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                continue;
            }

            //extract member picture timestamps
            auto pictureTimestampsElement = picturesDoc[user_account_keys::pictures::TIMESTAMP_STORED];
            if (pictureTimestampsElement &&
                pictureTimestampsElement.type() == bsoncxx::type::k_date) { //if element exists and is type date
                pictureTimestamp = pictureTimestampsElement.get_date().value;
            } else { //if element does not exist or is not type date
                logElementError(__LINE__, __FILE__, pictureTimestampsElement,
                                picturesDoc, bsoncxx::type::k_date, user_account_keys::pictures::TIMESTAMP_STORED,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                continue;
            }

            memberPictureTimestamps.emplace_back(
                    std::move(pictureOidString),
                    pictureTimestamp
            );

            indexUsed.insert(index);
        } else {
            memberPictureTimestamps.emplace_back(
                    "",
                    -1L
            );

            if (chatRoomPictureEle.type() != bsoncxx::type::k_null) {

                //these are mandatory to fill out so there should always be at least 1
                std::string errorString = "A picture array element was found which was neither null or document types.";

                std::optional<std::string> exceptionString;
                storeMongoDBErrorAndException(__LINE__, __FILE__,
                                              exceptionString, errorString,
                                              "database", database_names::ACCOUNTS_DATABASE_NAME,
                                              "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                              "type", convertBsonTypeToString(chatRoomPictureEle.type()),
                                              "oID_Used", user_account_oid,
                                              "verified_document", user_account_doc_view);
            }
        }

        index++;
    }

    return removeExcessPicturesIfNecessary(
            mongo_cpp_client,
            user_account_collection,
            currentTimestamp,
            user_account_oid,
            user_account_doc_view,
            memberPictureTimestamps
    );
}

//this takes in a MemberSharedInfoMessage and sets the PictureMessage using matchPictureOIDArr
//also sets the thumbnail and thumbnail size to MemberSharedInfoMessage
bool savePicturesToMemberSharedInfoMessage(
        mongocxx::client& mongo_cpp_client,
        mongocxx::collection& user_pictures_collection,
        mongocxx::collection& user_account_collection,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::array::view& memberPictureOIDArr,
        const bsoncxx::document::view& member_account_doc_view,
        const bsoncxx::oid& member_account_oid,
        MemberSharedInfoMessage* responseMessage
) {

    bsoncxx::builder::basic::array user_picture_oids;
    bsoncxx::oid first_picture_oid;
    std::string first_picture_oid_str;

    bool first_picture = true;
    int sizePicturesArray = 0;
    for (const auto& pictureEle: memberPictureOIDArr) {
        sizePicturesArray++;

        if (sizePicturesArray > general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) { //if more pictures than maximum

            std::vector<std::pair<std::string, std::chrono::milliseconds>> tempVector;
            std::set<int> tempSet;

            //remove excess pictures
            extractPicturesToVectorFromChatRoom(
                    mongo_cpp_client,
                    user_account_collection,
                    memberPictureOIDArr,
                    member_account_doc_view,
                    member_account_oid,
                    currentTimestamp,
                    tempVector,
                    tempSet
            );
            break;
        }

        if (pictureEle && pictureEle.type() == bsoncxx::type::k_document) { //if is type document

            bsoncxx::document::view pictureElementDoc = pictureEle.get_document().value;

            //extract picture oid
            auto extractedPictureOIDElement = pictureElementDoc[user_account_keys::pictures::OID_REFERENCE];
            if (!extractedPictureOIDElement
                || extractedPictureOIDElement.type() != bsoncxx::type::k_oid
            ) { //if element does not exist or is not type oid
                logElementError(
                        __LINE__, __FILE__,
                        extractedPictureOIDElement, pictureElementDoc,
                        bsoncxx::type::k_oid, user_account_keys::pictures::OID_REFERENCE,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );

                return false;
            }

            bsoncxx::oid extractedPictureOID = extractedPictureOIDElement.get_oid().value;

            user_picture_oids.append(extractedPictureOID);

            if (first_picture) {
                first_picture_oid = extractedPictureOID;
                first_picture_oid_str = first_picture_oid.to_string();
                first_picture = false;
            }
        } else if (pictureEle && pictureEle.type() != bsoncxx::type::k_null) {  // type is NOT null or document

            std::string error_string = "A picture array element from user_accounts_collection was found to not be type document OR type null.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "user_account_document", member_account_doc_view,
                    "pictures_array", memberPictureOIDArr
            );

            //can continue here (will just exclude the picture)
            continue;
        }
    }

    bsoncxx::array::view user_picture_oids_view = user_picture_oids.view();

    if (user_picture_oids_view.empty()) {
        //This is possible if all of a users pictures have been deleted by an admin.
        return true;
    }

    std::optional<std::string> exception_string;
    bsoncxx::stdx::optional<mongocxx::cursor> find_user_pictures_cursor;
    try {

        mongocxx::pipeline pipeline;

        pipeline.match(
                document{}
                        << "_id" << open_document
                        << "$in" << user_picture_oids_view
                        << close_document
                        << finalize
        );

        //project thumbnail only for required document
        pipeline.project(
                document{}
                        << "_id" << 1
                        << user_pictures_keys::PICTURE_SIZE_IN_BYTES << 1
                        << user_pictures_keys::TIMESTAMP_STORED << 1
                        << user_pictures_keys::PICTURE_INDEX << 1
                        << user_pictures_keys::PICTURE_IN_BYTES << 1
                        << user_pictures_keys::THUMBNAIL_IN_BYTES << open_document
                        << "$cond" << open_document
                        << "if" << open_document
                        << "$eq" << open_array
                        << "$_id"
                        << first_picture_oid
                        << close_array
                        << close_document
                        << "then" << "$" + user_pictures_keys::THUMBNAIL_IN_BYTES
                        << "else" << "$$REMOVE"
                        << close_document
                        << close_document
                        << user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES << open_document
                        << "$cond" << open_document
                        << "if" << open_document
                        << "$eq" << open_array
                        << "$_id"
                        << first_picture_oid
                        << close_array
                        << close_document
                        << "then" << "$" + user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES
                        << "else" << "$$REMOVE"
                        << close_document
                        << close_document
                        << finalize
        );

        //find the documents containing all pictures
        find_user_pictures_cursor = user_pictures_collection.aggregate(pipeline);
    }
    catch (const mongocxx::logic_error& e) {
        exception_string = std::string(e.what());
    }

    if (!find_user_pictures_cursor) { //if an error extracting pictures occurred
        std::string errorString = "An error occurred when extracting pictures from a matching account.";

        storeMongoDBErrorAndException(__LINE__,
                                      __FILE__,
                                      exception_string, errorString,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                      "oid_used", user_picture_oids_view,
                                      "verified_acct_projection", member_account_doc_view);

        return false;
    }

    bool thumbnail_extracted = false;
    for (const bsoncxx::document::view& cursor_doc: *find_user_pictures_cursor) {

        auto extractedPictureOIDElement = cursor_doc["_id"];
        if (!extractedPictureOIDElement
            || extractedPictureOIDElement.type() != bsoncxx::type::k_oid) { //if element does not exist or is not type oid

            //building a dummy document because the cursor_doc can be huge
            bsoncxx::builder::basic::array dummy_document;

            for (const auto& ele: cursor_doc) {
                //arbitrary number
                if (ele.length() < 256) {
                    dummy_document.append(ele.get_value());
                }
            }

            logElementError(__LINE__, __FILE__, extractedPictureOIDElement,
                            dummy_document.view(), bsoncxx::type::k_oid, "_id",
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::USER_PICTURES_COLLECTION_NAME);

            return false;
        }

        std::string extractedPictureOID = extractedPictureOIDElement.get_oid().value.to_string();

        auto pictureIndexElement = cursor_doc[user_pictures_keys::PICTURE_INDEX];
        if (!pictureIndexElement
            || pictureIndexElement.type() != bsoncxx::type::k_int32) { //if element does not exist or is not type int32

            //building a dummy document because the cursor_doc can be huge
            bsoncxx::builder::basic::array dummy_document;

            for (const auto& ele: cursor_doc) {
                //arbitrary number
                if (ele.length() < 256) {
                    dummy_document.append(ele.get_value());
                }
            }

            logElementError(__LINE__, __FILE__, pictureIndexElement,
                            dummy_document.view(), bsoncxx::type::k_int32,
                            user_pictures_keys::PICTURE_INDEX,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::USER_PICTURES_COLLECTION_NAME);

            return false;
        }

        int pictureIndex = pictureIndexElement.get_int32().value;

        auto pictureTimestampElement = cursor_doc[user_pictures_keys::TIMESTAMP_STORED];
        if (!pictureTimestampElement
            ||
            pictureTimestampElement.type() != bsoncxx::type::k_date) { //if element does not exist or is not type int64

            //building a dummy document because the cursor_doc can be huge
            bsoncxx::builder::basic::array dummy_document;

            for (const auto& ele: cursor_doc) {
                //arbitrary number
                if (ele.length() < 256) {
                    dummy_document.append(ele.get_value());
                }
            }

            //NOTE: not putting the document view because they can be huge
            logElementError(__LINE__, __FILE__, pictureTimestampElement,
                            dummy_document.view(), bsoncxx::type::k_date,
                            user_pictures_keys::TIMESTAMP_STORED,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::USER_PICTURES_COLLECTION_NAME);

            return false;
        }

        std::chrono::milliseconds pictureTimestamp = pictureTimestampElement.get_date().value;

        auto pictureSizeElement = cursor_doc[user_pictures_keys::PICTURE_SIZE_IN_BYTES];
        if (!pictureSizeElement
            || pictureSizeElement.type() != bsoncxx::type::k_int32) { //if element does not exist or is not type int32

            //building a dummy document because the cursor_doc can be huge
            bsoncxx::builder::basic::array dummy_document;

            for (const auto& ele: cursor_doc) {
                //arbitrary number
                if (ele.length() < 256) {
                    dummy_document.append(ele.get_value());
                }
            }

            //NOTE: not putting the document view because they can be huge
            logElementError(__LINE__, __FILE__, pictureSizeElement,
                            dummy_document.view(), bsoncxx::type::k_int32,
                            user_pictures_keys::PICTURE_SIZE_IN_BYTES,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::USER_PICTURES_COLLECTION_NAME);

            return false;
        }

        int pictureSize = pictureSizeElement.get_int32().value;

        auto pictureByteStringElement = cursor_doc[user_pictures_keys::PICTURE_IN_BYTES];
        if (!pictureByteStringElement
            ||
            pictureByteStringElement.type() != bsoncxx::type::k_utf8) { //if element does not exist or is not type utf8
            //building a dummy document because the cursor_doc can be huge
            bsoncxx::builder::basic::array dummy_document;

            for (const auto& ele: cursor_doc) {
                //arbitrary number
                if (ele.length() < 256) {
                    dummy_document.append(ele.get_value());
                }
            }

            logElementError(__LINE__, __FILE__, pictureByteStringElement,
                            dummy_document.view(), bsoncxx::type::k_utf8,
                            user_pictures_keys::PICTURE_IN_BYTES,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::USER_PICTURES_COLLECTION_NAME);

            return false;
        }

        std::string pictureByteString = pictureByteStringElement.get_string().value.to_string();

        if (pictureSize != (int) pictureByteString.size()) { //file corrupt

            //remove the picture and the reference to it
            findAndDeletePictureDocument(mongo_cpp_client, user_pictures_collection,
                                         bsoncxx::oid{extractedPictureOID},
                                         currentTimestamp);

            removePictureArrayElement(pictureIndex,
                                      user_account_collection,
                                      member_account_doc_view,
                                      member_account_oid);

            std::string errorString = "file corrupt (size did not match stored size) '";
            errorString += database_names::ACCOUNTS_DATABASE_NAME;
            errorString += "' '";
            errorString += collection_names::USER_PICTURES_COLLECTION_NAME;
            errorString += "'";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(),errorString,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                    "ObjectID_used", extractedPictureOID,
                    "verified_acct_projection", member_account_doc_view
            );

            continue;
        }

        if (first_picture_oid_str == extractedPictureOID) { //if this is the thumbnail picture

            int thumbnailSizeInBytes;
            std::string thumbnailByteString;

            auto thumbnailSizeElement = cursor_doc[user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES];
            if (thumbnailSizeElement
                && thumbnailSizeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                thumbnailSizeInBytes = thumbnailSizeElement.get_int32().value;
            } else { //if element does not exist or is not type int32

                //building a dummy document because the cursor_doc can be huge
                bsoncxx::builder::basic::array dummy_document;

                for (const auto& ele: cursor_doc) {
                    //arbitrary number
                    if (ele.length() < 256) {
                        dummy_document.append(ele.get_value());
                    }
                }

                logElementError(__LINE__, __FILE__,
                                thumbnailSizeElement, dummy_document.view(),
                                bsoncxx::type::k_int32,
                                user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_PICTURES_COLLECTION_NAME);

                return false;
            }

            auto thumbnailElement = cursor_doc[user_pictures_keys::THUMBNAIL_IN_BYTES];
            if (thumbnailElement
                && thumbnailElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                thumbnailByteString = thumbnailElement.get_string().value.to_string();
            } else { //if element does not exist or is not type utf8
                //building a dummy document because the cursor_doc can be huge
                bsoncxx::builder::basic::array dummy_document;

                for (const auto& ele: cursor_doc) {
                    //arbitrary number
                    if (ele.length() < 256) {
                        dummy_document.append(ele.get_value());
                    }
                }

                logElementError(__LINE__, __FILE__,
                                thumbnailElement,
                                dummy_document.view(), bsoncxx::type::k_utf8,
                                user_pictures_keys::THUMBNAIL_IN_BYTES,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_PICTURES_COLLECTION_NAME);

                return false;
            }

            if (thumbnailSizeInBytes != (int) thumbnailByteString.size()) { //if thumbnail is corrupted

                //remove the picture and the reference to it
                findAndDeletePictureDocument(mongo_cpp_client, user_pictures_collection,
                                             bsoncxx::oid{extractedPictureOID},
                                             currentTimestamp);

                removePictureArrayElement(pictureIndex,
                                          user_account_collection,
                                          member_account_doc_view,
                                          member_account_oid);

                std::string errorString = "file corrupt (size did not match stored size) '";
                errorString += database_names::ACCOUNTS_DATABASE_NAME;
                errorString += "' '";
                errorString += collection_names::USER_PICTURES_COLLECTION_NAME;
                errorString += "'";

                std::optional<std::string> dummyExceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        dummyExceptionString,
                        errorString,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                        "ObjectID_used", extractedPictureOID,
                        "verified_acct_projection", member_account_doc_view);

                continue;
            }

            //set thumbnail
            responseMessage->set_account_thumbnail(thumbnailByteString);
            responseMessage->set_account_thumbnail_size(thumbnailSizeInBytes);
            responseMessage->set_account_thumbnail_index(pictureIndex);
            responseMessage->set_account_thumbnail_timestamp(pictureTimestamp.count());

            thumbnail_extracted = true;
        }

        //save picture to response
        auto picture = responseMessage->add_picture();
        picture->set_file_in_bytes(std::move(pictureByteString));
        picture->set_file_size(pictureSize);
        picture->set_index_number(pictureIndex);
        picture->set_timestamp_picture_last_updated(pictureTimestamp.count());
    }

    if (!thumbnail_extracted) {

        std::vector<bsoncxx::oid> user_picture_oids_vector;
        for (const auto& user_pic_oid_ele: user_picture_oids_view) {
            bsoncxx::oid oid = user_pic_oid_ele.get_oid().value;
            if (oid.to_string() != first_picture_oid_str) {
                user_picture_oids_vector.emplace_back(user_pic_oid_ele.get_oid().value);
            }
        }

        std::reverse(user_picture_oids_vector.begin(), user_picture_oids_vector.end());

        while (!user_picture_oids_vector.empty() && !thumbnail_extracted) {

            bsoncxx::stdx::optional<bsoncxx::document::value> find_replacement_thumbnail_document;
            try {
                mongocxx::options::find opts;

                opts.projection(
                        document{}
                                << "_id" << 1
                                << user_pictures_keys::PICTURE_INDEX << 1
                                << user_pictures_keys::TIMESTAMP_STORED << 1
                                << user_pictures_keys::THUMBNAIL_IN_BYTES << 1
                                << user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES << 1
                                << finalize
                );

                //find the documents containing all pictures
                find_replacement_thumbnail_document = user_pictures_collection.find_one(
                        document{}
                                << "_id" << user_picture_oids_vector.back()
                                << finalize,
                        opts
                );
            }
            catch (const mongocxx::logic_error& e) {
                exception_string = std::string(e.what());
            }

            if (!find_replacement_thumbnail_document) { //if an error extracting pictures occurred
                std::string errorString = "A picture was found in the user account document. However not found in the user pictures collection.";

                storeMongoDBErrorAndException(__LINE__,
                                              __FILE__,
                                              exception_string, errorString,
                                              "database", database_names::ACCOUNTS_DATABASE_NAME,
                                              "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                              "oid_used", user_picture_oids_view,
                                              "verified_acct_projection", member_account_doc_view);

                continue;
            }

            bsoncxx::document::view cursor_doc = *find_replacement_thumbnail_document;

            auto pictureIndexElement = cursor_doc[user_pictures_keys::PICTURE_INDEX];
            if (!pictureIndexElement
                ||
                pictureIndexElement.type() != bsoncxx::type::k_int32) { //if element does not exist or is not type int32

                //building a dummy document because the cursor_doc can be huge
                bsoncxx::builder::basic::array dummy_document;

                for (const auto& ele: cursor_doc) {
                    //arbitrary number
                    if (ele.length() < 256) {
                        dummy_document.append(ele.get_value());
                    }
                }

                logElementError(__LINE__, __FILE__, pictureIndexElement,
                                dummy_document.view(), bsoncxx::type::k_int32,
                                user_pictures_keys::PICTURE_INDEX,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_PICTURES_COLLECTION_NAME);

                return false;
            }

            int pictureIndex = pictureIndexElement.get_int32().value;

            auto pictureTimestampElement = cursor_doc[user_pictures_keys::TIMESTAMP_STORED];
            if (!pictureTimestampElement
                || pictureTimestampElement.type() !=
                   bsoncxx::type::k_date) { //if element does not exist or is not type int64

                //building a dummy document because the cursor_doc can be huge
                bsoncxx::builder::basic::array dummy_document;

                for (const auto& ele: cursor_doc) {
                    //arbitrary number
                    if (ele.length() < 256) {
                        dummy_document.append(ele.get_value());
                    }
                }

                //NOTE: not putting the document view because they can be huge
                logElementError(__LINE__, __FILE__, pictureTimestampElement,
                                dummy_document.view(), bsoncxx::type::k_date,
                                user_pictures_keys::TIMESTAMP_STORED,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_PICTURES_COLLECTION_NAME);

                return false;
            }

            std::chrono::milliseconds pictureTimestamp = pictureTimestampElement.get_date().value;

            int thumbnailSizeInBytes;
            std::string thumbnailByteString;

            auto thumbnailSizeElement = cursor_doc[user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES];
            if (thumbnailSizeElement
                && thumbnailSizeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                thumbnailSizeInBytes = thumbnailSizeElement.get_int32().value;
            } else { //if element does not exist or is not type int32

                //building a dummy document because the cursor_doc can be huge
                bsoncxx::builder::basic::array dummy_document;

                for (const auto& ele: cursor_doc) {
                    //arbitrary number
                    if (ele.length() < 256) {
                        dummy_document.append(ele.get_value());
                    }
                }

                logElementError(__LINE__, __FILE__,
                                thumbnailSizeElement, dummy_document.view(),
                                bsoncxx::type::k_int32,
                                user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_PICTURES_COLLECTION_NAME);

                return false;
            }

            auto thumbnailElement = cursor_doc[user_pictures_keys::THUMBNAIL_IN_BYTES];
            if (thumbnailElement
                && thumbnailElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                thumbnailByteString = thumbnailElement.get_string().value.to_string();
            } else { //if element does not exist or is not type utf8
                //building a dummy document because the cursor_doc can be huge
                bsoncxx::builder::basic::array dummy_document;

                for (const auto& ele: cursor_doc) {
                    //arbitrary number
                    if (ele.length() < 256) {
                        dummy_document.append(ele.get_value());
                    }
                }

                logElementError(__LINE__, __FILE__,
                                thumbnailElement,
                                dummy_document.view(), bsoncxx::type::k_utf8,
                                user_pictures_keys::THUMBNAIL_IN_BYTES,
                                database_names::ACCOUNTS_DATABASE_NAME,
                                collection_names::USER_PICTURES_COLLECTION_NAME);

                return false;
            }

            if (thumbnailSizeInBytes != (int) thumbnailByteString.size()) { //if thumbnail is corrupted

                user_picture_oids_vector.pop_back();

                auto extractedPictureOIDElement = cursor_doc["_id"];
                if (!extractedPictureOIDElement
                    || extractedPictureOIDElement.type() !=
                       bsoncxx::type::k_oid) { //if element does not exist or is not type oid

                    //building a dummy document because the cursor_doc can be huge
                    bsoncxx::builder::basic::array dummy_document;

                    for (const auto& ele: cursor_doc) {
                        //arbitrary number
                        if (ele.length() < 256) {
                            dummy_document.append(ele.get_value());
                        }
                    }

                    logElementError(__LINE__, __FILE__, extractedPictureOIDElement,
                                    dummy_document.view(), bsoncxx::type::k_oid, "_id",
                                    database_names::ACCOUNTS_DATABASE_NAME,
                                    collection_names::USER_PICTURES_COLLECTION_NAME);

                    return false;
                }

                //remove the picture and the reference to it
                findAndDeletePictureDocument(mongo_cpp_client, user_pictures_collection,
                                             extractedPictureOIDElement.get_oid().value,
                                             currentTimestamp);


                removePictureArrayElement(pictureIndex,
                                          user_account_collection,
                                          member_account_doc_view,
                                          member_account_oid);

                std::string errorString = "file corrupt (size did not match stored size) '";
                errorString += database_names::ACCOUNTS_DATABASE_NAME;
                errorString += "' '";
                errorString += collection_names::USER_PICTURES_COLLECTION_NAME;
                errorString += "'";

                std::optional<std::string> dummyExceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        dummyExceptionString,
                        errorString,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                        "ObjectID_used", extractedPictureOIDElement.get_oid().value,
                        "verified_acct_projection", member_account_doc_view);

                continue;
            } else {

                //set thumbnail
                responseMessage->set_account_thumbnail(thumbnailByteString);
                responseMessage->set_account_thumbnail_size(thumbnailSizeInBytes);
                responseMessage->set_account_thumbnail_index(pictureIndex);
                responseMessage->set_account_thumbnail_timestamp(pictureTimestamp.count());

                thumbnail_extracted = true;
            }
        }
    }

    return true;

}

//extracts thumbnail from a document view from user pictures collection
bool extractThumbnailFromUserPictureDocument(
        const bsoncxx::document::view& pictureDocumentView,
        std::string& thumbnail,
        const std::function<void(const std::string& errorString)>& errorLambda
) {

    int thumbnailSize;

    auto thumbnailSizeElement = pictureDocumentView[user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES];
    if (thumbnailSizeElement &&
        thumbnailSizeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        thumbnailSize = thumbnailSizeElement.get_int32().value;
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, thumbnailSizeElement,
                        pictureDocumentView, bsoncxx::type::k_int32, user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

        return false;
    }

    auto thumbnailElement = pictureDocumentView[user_pictures_keys::THUMBNAIL_IN_BYTES];
    if (thumbnailElement && thumbnailElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8

        thumbnail = thumbnailElement.get_string().value.to_string();

        if (thumbnailSize != (int) thumbnail.size()) {

            std::string errorString =
                    "Thumbnail was corrupted in '" + database_names::ACCOUNTS_DATABASE_NAME + ' ' +
                    collection_names::USER_PICTURES_COLLECTION_NAME +
                    ".\n";

            errorLambda(errorString);

            return false;
        }

    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, thumbnailElement,
                        pictureDocumentView, bsoncxx::type::k_utf8, user_pictures_keys::THUMBNAIL_IN_BYTES,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

        return false;
    }

    return true;
}

//this function stores all requested pictures to a response using setPictureToResponse() lambda
//Pass in deleted_DB as nullptr if searching the deleted database is NOT required.
//If deleted_DB is set AND no pictures are found inside the user_pictures collection, then deleted_DB
// will be searched for pictures INSTEAD. Note that it will not be searched ALSO but INSTEAD. This means
// that pictures will be returned from user_pictures collection OR deleted_pictures collection not both.
//If function is successful, this will always call either setPictureToResponse() or setPictureEmptyResponse()
// for each index stored inside nonEmptyRequestedIndexes.
bool extractAllPicturesToResponse(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& userAccountsCollection,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::document::view& userAccountDocView,
        const bsoncxx::array::view& requestedPictureOid,
        std::set<int> nonEmptyRequestedIndexes, const bsoncxx::oid& userAccountOID,
        mongocxx::client_session* session,
        const std::function<void(const bsoncxx::oid& picture_oid,
                                 std::string&& pictureByteString, int pictureSize,
                                 int indexNumber,
                                 const std::chrono::milliseconds& picture_timestamp,
                                 bool extracted_from_deleted_pictures,
                                 bool references_removed_after_delete
        )>& setPictureToResponse,
        const std::function<void(int)>& setPictureEmptyResponse,
        mongocxx::database* deleted_DB //If searching deleted pictures is not wanted; set to null.
) {

    mongocxx::collection userPictures = accountsDB[collection_names::USER_PICTURES_COLLECTION_NAME];

    bool extracted_from_deleted_pictures = false;

    bsoncxx::builder::stream::document projectionDoc;

    projectionDoc
            << "_id" << 1
            << user_pictures_keys::PICTURE_INDEX << 1
            << user_pictures_keys::PICTURE_SIZE_IN_BYTES << 1
            << user_pictures_keys::TIMESTAMP_STORED << 1
            << user_pictures_keys::PICTURE_IN_BYTES << 1;

    mongocxx::stdx::optional<mongocxx::cursor> findUserPicturesCursor;
    std::optional<std::string> find_user_pictures_cursor;
    try {

        mongocxx::options::find findOptions;
        findOptions.projection(projectionDoc.view());

        //find user picture document
        findUserPicturesCursor = find_optional_session(
                session,
                userPictures,
                document{}
                        << "_id" << open_document
                        << "$in" << requestedPictureOid
                        << close_document
                        << finalize,
                findOptions
        );
    }
    catch (mongocxx::logic_error& e) {
        find_user_pictures_cursor = std::string(e.what());
    }

    if (!findUserPicturesCursor) { //cursor failed

        std::string errorString = "No results found when attempting to extract pictures.";

        storeMongoDBErrorAndException(__LINE__,
                                      __FILE__, find_user_pictures_cursor,
                                      errorString,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                                      "userAccountOID", userAccountOID,
                                      "cursor_empty",
                                      (findUserPicturesCursor->begin() == findUserPicturesCursor->end()) ? "true"
                                                                                                         : "false",
                                      "array_used", makePrettyJson(requestedPictureOid));

        return false;
    } else if (findUserPicturesCursor->begin() == findUserPicturesCursor->end()) { //no pictures found
        if (deleted_DB == nullptr) { //deleted database was left as null, no need to search it
            //NOTE: This is possible if all requested pictures do not exist anymore
            for (int requestedIndex: nonEmptyRequestedIndexes) {
                setPictureEmptyResponse(requestedIndex);
            }
            return true;
        }

        mongocxx::collection deletedPictures = (*deleted_DB)[collection_names::DELETED_USER_PICTURES_COLLECTION_NAME];

        projectionDoc
                << deleted_user_pictures_keys::REFERENCES_REMOVED_AFTER_DELETE << 1;

        try {
            mongocxx::options::find findOptions;
            findOptions.projection(projectionDoc.view());

            //find user picture document
            findUserPicturesCursor = find_optional_session(
                    session,
                    deletedPictures,
                    document{}
                            << "_id" << open_document
                            << "$in" << requestedPictureOid
                            << close_document
                            << finalize,
                    findOptions
            );
        }
        catch (mongocxx::logic_error& e) {
            find_user_pictures_cursor = std::string(e.what());
        }

        if (!findUserPicturesCursor) { //cursor failed

            std::string errorString = "No results found when attempting to extract pictures (from deleted collection.";

            storeMongoDBErrorAndException(__LINE__,
                                          __FILE__, find_user_pictures_cursor,
                                          errorString,
                                          "database", database_names::ACCOUNTS_DATABASE_NAME,
                                          "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                                          "userAccountOID", userAccountOID,
                                          "cursor_empty",
                                          (findUserPicturesCursor->begin() == findUserPicturesCursor->end()) ? "true"
                                                                                                             : "false",
                                          "array_used", makePrettyJson(requestedPictureOid));

            return false;
        }

        if (findUserPicturesCursor->begin() == findUserPicturesCursor->end()) { //no pictures found
            for (int requestedIndex: nonEmptyRequestedIndexes) {
                setPictureEmptyResponse(requestedIndex);
            }
            return true;
        }

        extracted_from_deleted_pictures = true;
    }

    auto removePictureAndSendEmptyReply = [&](const bsoncxx::oid& pictureOid, int indexOfArray) {

        if (!extracted_from_deleted_pictures) {
            //remove the picture and the reference to it
            findAndDeletePictureDocument(mongoCppClient, userPictures, pictureOid, currentTimestamp, session);

            removePictureArrayElement(indexOfArray, userAccountsCollection, userAccountDocView,
                                      userAccountOID, session);
        }

        setPictureEmptyResponse(indexOfArray);
    };

    for (const auto& findUserPictureAccountView: *findUserPicturesCursor) {

        bsoncxx::oid pictureOid;
        std::chrono::milliseconds pictureTimestamp;
        int pictureIndex;
        int pictureSize;
        std::string pictureByteString;
        bool references_removed_after_delete = false;

        auto pictureOidElement = findUserPictureAccountView["_id"];
        if (pictureOidElement &&
            pictureOidElement.type() == bsoncxx::type::k_oid) { //if element exists and is type oid
            pictureOid = pictureOidElement.get_oid().value;
        } else { //if element does not exist or is not type oid

            bsoncxx::document::value dummyDoc = document{}
                    << finalize;

            //NOTE: not putting the document view because they can be huge
            logElementError(__LINE__, __FILE__, pictureOidElement,
                            dummyDoc.view(), bsoncxx::type::k_oid, "_id",
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

            continue;
        }

        auto pictureIndexElement = findUserPictureAccountView[user_pictures_keys::PICTURE_INDEX];
        if (pictureIndexElement && pictureIndexElement.type() == bsoncxx::type::k_int32) {
            pictureIndex = pictureIndexElement.get_int32().value;
        } else {

            bsoncxx::document::value dummyDoc = document{}
                    << "_id" << pictureOid
                    << finalize;

            //NOTE: not putting the document view because they can be huge
            logElementError(__LINE__, __FILE__, pictureIndexElement,
                            dummyDoc.view(), bsoncxx::type::k_int32, user_pictures_keys::PICTURE_INDEX,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

            continue;
        }

        nonEmptyRequestedIndexes.erase(pictureIndex);

        auto pictureTimestampElement = findUserPictureAccountView[user_pictures_keys::TIMESTAMP_STORED];
        if (pictureTimestampElement &&
            pictureTimestampElement.type() == bsoncxx::type::k_date) { //if element exists and is type int64
            pictureTimestamp = pictureTimestampElement.get_date().value;
        } else { //if element does not exist or is not type int64

            bsoncxx::document::value dummyDoc = document{}
                    << "_id" << pictureOid
                    << finalize;

            //NOTE: not putting the document view because they can be huge
            logElementError(__LINE__, __FILE__, pictureTimestampElement,
                            dummyDoc.view(), bsoncxx::type::k_date, user_pictures_keys::TIMESTAMP_STORED,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

            removePictureAndSendEmptyReply(pictureOid, pictureIndex);

            continue;
        }

        auto pictureSizeElement = findUserPictureAccountView[user_pictures_keys::PICTURE_SIZE_IN_BYTES];
        if (pictureSizeElement && pictureSizeElement.type() == bsoncxx::type::k_int32) {
            pictureSize = pictureSizeElement.get_int32().value;
        } else {
            bsoncxx::document::value dummyDoc = document{}
                    << "_id" << pictureOid
                    << finalize;

            logElementError(__LINE__, __FILE__, pictureSizeElement,
                            dummyDoc.view(), bsoncxx::type::k_int32, user_pictures_keys::PICTURE_SIZE_IN_BYTES,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

            removePictureAndSendEmptyReply(pictureOid, pictureIndex);

            continue;
        }

        auto pictureByteStringElement = findUserPictureAccountView[user_pictures_keys::PICTURE_IN_BYTES];
        if (pictureByteStringElement && pictureByteStringElement.type() == bsoncxx::type::k_utf8) {
            pictureByteString = pictureByteStringElement.get_string().value.to_string();
        } else { //if element does not exist or is not type array

            bsoncxx::document::value dummyDoc = document{}
                    << "_id" << pictureOid
                    << finalize;

            //NOTE: not putting the element or document in here for because it can be huge
            /** DO NOT PLACE ELEMENT OR DOCUMENT IN THIS logElementError **/
            logElementError(__LINE__, __FILE__, pictureOidElement,
                            dummyDoc.view(), bsoncxx::type::k_utf8, user_pictures_keys::PICTURE_IN_BYTES,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

            removePictureAndSendEmptyReply(pictureOid, pictureIndex);

            continue;
        }

        if (extracted_from_deleted_pictures) {
            auto referencesRemovedAfterDeleteElement = findUserPictureAccountView[deleted_user_pictures_keys::REFERENCES_REMOVED_AFTER_DELETE];
            if (referencesRemovedAfterDeleteElement) {
                if (referencesRemovedAfterDeleteElement.type() == bsoncxx::type::k_bool) {
                    references_removed_after_delete = referencesRemovedAfterDeleteElement.get_bool();
                } else {
                    bsoncxx::document::value dummyDoc = document{}
                            << "_id" << pictureOid
                            << finalize;

                    //NOTE: not putting the element or document in here for because it can be huge
                    /** DO NOT PLACE ELEMENT OR DOCUMENT IN THIS logElementError (large) **/
                    logElementError(__LINE__, __FILE__, pictureOidElement,
                                    dummyDoc.view(), bsoncxx::type::k_bool, user_pictures_keys::PICTURE_IN_BYTES,
                                    database_names::ACCOUNTS_DATABASE_NAME,
                                    collection_names::USER_PICTURES_COLLECTION_NAME);

                    continue;
                }
            }
            //else {} //OK if this element does not exist
        }

        if (pictureSize != (int) pictureByteString.size()) { //if file is corrupt

            removePictureAndSendEmptyReply(pictureOid, pictureIndex);

#ifndef LG_TESTING
            std::string errorString = "File corrupt (size did not match stored size).";

            std::optional<std::string> exceptionString;
            storeMongoDBErrorAndException(__LINE__,
                                          __FILE__, exceptionString,
                                          errorString,
                                          "database", database_names::ACCOUNTS_DATABASE_NAME,
                                          "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                                          "ObjectID_used", pictureOid,
                                          "pic_byte_array_size",
                                          std::to_string(pictureByteString.size()),
                                          "pic_stored_size", std::to_string(pictureSize));
#endif // !TESTING
            continue;
        }

        //NOTE: It is very important that the picture timestamp is sent back in the lambda, the android
        // client relies on the timestamp being the exact same (not just greater).
        //It is also important that it is from the array inside the user account document because the default
        // picture can have a different timestamp.
        setPictureToResponse(
                pictureOid,
                std::move(pictureByteString),
                pictureSize,
                pictureIndex,
                pictureTimestamp,
                extracted_from_deleted_pictures,
                references_removed_after_delete
        );
    }

    for (int leftOverIndex: nonEmptyRequestedIndexes) {
        removePictureArrayElement(leftOverIndex, userAccountsCollection, userAccountDocView,
                                  userAccountOID, session);

        std::string errorString = "A picture element was found inside the user account index however not inside the userPictures collection.";

        std::optional<std::string> exceptionString;
        storeMongoDBErrorAndException(__LINE__,
                                      __FILE__, exceptionString,
                                      errorString,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                                      "userAccountOID", userAccountOID,
                                      "leftOverIndex", leftOverIndex,
                                      "array_used", makePrettyJson(requestedPictureOid));

        setPictureEmptyResponse(leftOverIndex);
    }

    return true;
}

//this function stores a picture to a response using the setPictureToResponse() and setThumbnailToResponse() lambdas
//function expects verified account values PICTURE_SIZE_IN_BYTES PICTURE_IN_BYTES to be projected
bool extractPictureToResponse(mongocxx::client& mongoCppClient, mongocxx::database& accountsDB,
                              mongocxx::collection& userAccountCollection,
                              const std::chrono::milliseconds& currentTimestamp,
                              const bsoncxx::document::view& userAccountDocView, const bsoncxx::oid& pictureOid,
                              const bsoncxx::oid& userAccountOID, int indexOfArray,
                              const std::function<void(
                                      std::string&& /*picture*/,
                                      int /*picture size*/,
                                      const std::chrono::milliseconds& /*picture timestamp*/
                              )>& setPictureToResponse, bool extractThumbnail,
                              const std::function<void(
                                      std::string&& /*thumbnail*/,
                                      int /*thumbnail_size*/,
                                      const std::chrono::milliseconds& /*thumbnail_timestamp*/
                              )>& setThumbnailToResponse) {

    mongocxx::collection userPictures = accountsDB[collection_names::USER_PICTURES_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findUserPictureAccount;
    try {

        bsoncxx::builder::stream::document projectionDoc;

        projectionDoc
                << "_id" << 0
                << user_pictures_keys::PICTURE_SIZE_IN_BYTES << 1
                << user_pictures_keys::TIMESTAMP_STORED << 1
                << user_pictures_keys::PICTURE_IN_BYTES << 1;

        if (extractThumbnail) {
            projectionDoc
                    << user_pictures_keys::THUMBNAIL_IN_BYTES << 1
                    << user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES << 1;
        }

        mongocxx::options::find findOptions;
        findOptions.projection(projectionDoc.view());

        //find user picture document
        findUserPictureAccount = userPictures.find_one(document{}
                                                               << "_id" << pictureOid
                                                               << finalize,
                                                       findOptions);

    }
    catch (mongocxx::logic_error& e) {
        std::optional<std::string> exceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__, exceptionString,
                                      std::string(e.what()),
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_PICTURES_COLLECTION_NAME);

        return false;
    }

    if (findUserPictureAccount) { //if successfully found the picture location
        bsoncxx::document::view findUserPictureAccountView = *findUserPictureAccount;

        auto pictureTimestampElement = findUserPictureAccountView[user_pictures_keys::TIMESTAMP_STORED];
        if (!pictureTimestampElement
            ||
            pictureTimestampElement.type() != bsoncxx::type::k_date) { //if element does not exist or is not type int64

            //NOTE: not putting the document view because they can be huge
            logElementError(__LINE__, __FILE__, pictureTimestampElement,
                            findUserPictureAccountView, bsoncxx::type::k_date, user_pictures_keys::TIMESTAMP_STORED,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

            return false;
        }

        auto pictureSizeElement = findUserPictureAccountView[user_pictures_keys::PICTURE_SIZE_IN_BYTES];
        if (!pictureSizeElement || pictureSizeElement.type() != bsoncxx::type::k_int32) {
            logElementError(__LINE__, __FILE__, pictureSizeElement,
                            findUserPictureAccountView, bsoncxx::type::k_int32,
                            user_pictures_keys::PICTURE_SIZE_IN_BYTES,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::USER_PICTURES_COLLECTION_NAME);

            return false;
        }

        auto pictureByteStringElement = findUserPictureAccountView[user_pictures_keys::PICTURE_IN_BYTES];
        if (!pictureByteStringElement ||
            pictureByteStringElement.type() != bsoncxx::type::k_utf8) { //if element does not exist or is not type array
            //NOTE: not putting the element in here for because it can be huge
            logElementError(__LINE__, __FILE__, pictureByteStringElement,
                            findUserPictureAccountView, bsoncxx::type::k_utf8,
                            user_pictures_keys::PICTURE_IN_BYTES,
                            database_names::ACCOUNTS_DATABASE_NAME,
                            collection_names::USER_PICTURES_COLLECTION_NAME);

            return false;
        }

        std::chrono::milliseconds pictureTimestamp = pictureTimestampElement.get_date().value;
        int pictureSize = pictureSizeElement.get_int32().value;
        std::string pictureByteString = pictureByteStringElement.get_string().value.to_string();

        if (pictureSize == (int) pictureByteString.size()) { //file size matches bytes size

            if (extractThumbnail) {

                std::string thumbnail;

                auto errorLambda = [&](const std::string& errorString) {
                    std::optional<std::string> exceptionString;
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__, exceptionString,
                            errorString,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection",
                            collection_names::USER_PICTURES_COLLECTION_NAME,
                            "ObjectID_used", pictureOid);
                };

                //extracts thumbnail from a document view from user pictures collection
                if (!extractThumbnailFromUserPictureDocument(findUserPictureAccountView,
                                                             thumbnail,
                                                             errorLambda)) { //if thumbnail was corrupt

                    //remove the picture and the reference to it
                    findAndDeletePictureDocument(mongoCppClient, userPictures, pictureOid, currentTimestamp);

                    removePictureArrayElement(indexOfArray, userAccountCollection, userAccountDocView,
                                              userAccountOID);

                    //Error stored in above function
                    return false;
                }

                setThumbnailToResponse(std::move(thumbnail), (int) thumbnail.size(), pictureTimestamp);
            }

            setPictureToResponse(std::move(pictureByteString), pictureSize, pictureTimestamp);
        } else { //file corrupt

            //remove the picture and the reference to it
            findAndDeletePictureDocument(mongoCppClient, userPictures, pictureOid, currentTimestamp);

            removePictureArrayElement(indexOfArray, userAccountCollection, userAccountDocView,
                                      userAccountOID);

            std::string errorString = "File corrupt (size did not match stored size).";

            std::optional<std::string> exceptionString;
            storeMongoDBErrorAndException(__LINE__,
                                          __FILE__, exceptionString,
                                          errorString,
                                          "database", database_names::ACCOUNTS_DATABASE_NAME,
                                          "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                                          "ObjectID_used", pictureOid,
                                          "pic_byte_array_size",
                                          std::to_string(pictureByteString.size()),
                                          "pic_stored_size", std::to_string(pictureSize));
            return false;
        }

    } else { //if failed to find the picture location even though there was an Oid referencing it

        removePictureArrayElement(indexOfArray, userAccountCollection, userAccountDocView,
                                  userAccountOID);

        return false;
    }

    return true;
}

ReturnStatus checkForValidLoginToken(
        const bsoncxx::stdx::optional<bsoncxx::document::value>& findAndUpdateUserAccount,
        const std::string& userAccountOIDStr
) {

    //NOTE: Trying to handle NO_VERIFIED_ACCOUNT here for consistency reasons
    //if document found and no exception occurred
    if (findAndUpdateUserAccount) {
        const bsoncxx::document::view userAccountDocView = *findAndUpdateUserAccount;
        ReturnStatus returnStatus;

        auto returnMessageElement = userAccountDocView[user_account_keys::LOGGED_IN_RETURN_MESSAGE];
        if (returnMessageElement &&
            returnMessageElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            returnStatus = ReturnStatus(returnMessageElement.get_int32().value);
        } else { //if element does not exist or is not type int32
            logElementError(__LINE__, __FILE__, returnMessageElement,
                            userAccountDocView, bsoncxx::type::k_int32, user_account_keys::LOGGED_IN_RETURN_MESSAGE,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

            return ReturnStatus::LG_ERROR;
        }

        if (returnStatus == LG_ERROR) {
            const std::string error_string = "A combination was returned that should not be possible.\n";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", userAccountOIDStr
            );
        }

        return returnStatus;

    } else {
        return ReturnStatus::NO_VERIFIED_ACCOUNT;
    }

}

//If the thumbnail has been updated, will remove the chat room id from the user picture inside user_pictures_collection.
//chat_room_header_doc_view should contain current user with the oid and the thumbnail reference projected
bool extractUserAndRemoveChatRoomIdFromUserPicturesReference(
        mongocxx::database& accounts_db,
        const bsoncxx::document::view& chat_room_header_doc_view,
        const bsoncxx::oid& current_user_oid,
        const std::string& updated_thumbnail_reference_oid,
        const std::string& chat_room_id,
        mongocxx::client_session* session
) {

    auto accountsListElement = chat_room_header_doc_view[chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM];
    if (!accountsListElement
        || accountsListElement.type() != bsoncxx::type::k_array) {  //if element does not exist or is not type array
        logElementError(__LINE__, __FILE__, accountsListElement,
                        chat_room_header_doc_view, bsoncxx::type::k_array, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id);

        return false;
    }

    bsoncxx::array::view accounts_list = accountsListElement.get_array().value;
    for (auto& ele: accounts_list) {
        if (ele.type() != bsoncxx::type::k_document) {
            logElementError(__LINE__, __FILE__, accountsListElement,
                            chat_room_header_doc_view, bsoncxx::type::k_array,
                            chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                            database_names::CHAT_ROOMS_DATABASE_NAME,
                            collection_names::CHAT_ROOM_ID_ + chat_room_id);

            return false;
        }

        bsoncxx::document::view member_doc = ele.get_document().value;

        auto member_account_oid_element = member_doc[chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID];
        if (!member_account_oid_element
            ||
            member_account_oid_element.type() != bsoncxx::type::k_oid) { //if element does not exist or is not type oid
            logElementError(__LINE__, __FILE__,
                            member_account_oid_element,
                            member_doc, bsoncxx::type::k_oid,
                            chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID,
                            database_names::CHAT_ROOMS_DATABASE_NAME,
                            collection_names::CHAT_ROOM_ID_ + chat_room_id);

            return false;
        }

        bsoncxx::oid member_account_oid = member_account_oid_element.get_oid().value;

        if (member_account_oid == current_user_oid) {

            auto thumbnailReferenceElement = member_doc[chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE];
            if (thumbnailReferenceElement &&
                thumbnailReferenceElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                std::string thumbnail_reference = thumbnailReferenceElement.get_string().value.to_string();

                if (!thumbnail_reference.empty()
                    && updated_thumbnail_reference_oid != thumbnail_reference) { //if thumbnail was updated

                    removeChatRoomIdFromUserPicturesReference(
                            accounts_db,
                            bsoncxx::oid(thumbnail_reference),
                            chat_room_id,
                            session
                    );
                }

            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__,
                                thumbnailReferenceElement,
                                member_doc, bsoncxx::type::k_utf8,
                                chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE,
                                database_names::CHAT_ROOMS_DATABASE_NAME,
                                collection_names::CHAT_ROOM_ID_ + chat_room_id);
            }

            break;
        }
    }

    return true;
}

//removes a chat room from THUMBNAIL_REFERENCES
bool removeChatRoomIdFromUserPicturesReference(
        mongocxx::database& accounts_db,
        const bsoncxx::oid& picture_id,
        const std::string& chat_room_id,
        mongocxx::client_session* session
) {

    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    bsoncxx::stdx::optional<mongocxx::result::update> update_user_pictures_result;
    std::optional<std::string> update_picture_exception_string;
    try {

        update_user_pictures_result = update_one_optional_session(
                session,
                user_pictures_collection,
                document{}
                        << "_id" << picture_id
                        << finalize,
                document{}
                        << "$pull" << open_document
                        << user_pictures_keys::THUMBNAIL_REFERENCES << chat_room_id
                        << close_document
                        << finalize
        );

    } catch (const mongocxx::logic_error& e) {
        update_picture_exception_string = e.what();
    }

    //NOTE: it is possible for this to NOT be found in the case that the picture was
    // deleted and the server is in the process of removing references to it, so
    // update_user_pictures_result->matched_count() can be equal to 0
    if (!update_user_pictures_result) {

        std::string errorString = "Failed to update picture document.\n";

        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      update_picture_exception_string,
                                      errorString,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                                      "ObjectID_used", picture_id,
                                      "chat_room_id", chat_room_id);

        return false;
    }

    return true;
}

std::string gen_random_alpha_numeric_string(int len) {

    std::string tmp_s;
    static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

    std::string random_seed_str = bsoncxx::oid{}.to_string();
    std::seed_seq seed(random_seed_str.begin(), random_seed_str.end());
    std::mt19937 rng(seed);

    //this must be size-2 it is a C style string and so the final char will be null terminator
    std::uniform_int_distribution<std::mt19937::result_type> distribution(0, sizeof(alphanum) - 2);

    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i)
        tmp_s += alphanum[distribution(rng)];

    return tmp_s;
}

//Will generate account ids with their respective prefix. Will return "~" if an error
// with passed account ID occurs.
std::string errorCheckAccountIDAndGenerateUniqueAccountID(
        const AccountLoginType& accountType,
        const std::string& accountID
) {

    if (accountID.length() < 4
        || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES < accountID.length()
            ) {
        return "~";
    }

    if (accountType == AccountLoginType::FACEBOOK_ACCOUNT) {
        return general_values::FACEBOOK_ACCOUNT_ID_PREFIX + accountID;
    } else if (accountType == AccountLoginType::GOOGLE_ACCOUNT) {
        return general_values::GOOGLE_ACCOUNT_ID_PREFIX + accountID;
    }

    return accountID;
}

void trimLeadingWhitespace(std::string& str) {
    str.erase(std::begin(str), std::find_if(std::begin(str), std::end(str), [](char c) -> bool {
        return !isspace(c);
    }));
}

void trimTrailingWhitespace(std::string& str) {
    str.erase(std::find_if(str.rbegin(), str.rend(), [](char c) -> bool {
        return !isspace(c);
    }).base(), str.end());
}

void trimWhitespace(std::string& str) {
    trimLeadingWhitespace(str);
    trimTrailingWhitespace(str);
}

std::string getDateTimeStringFromTimestamp(const std::chrono::milliseconds& timestamp) {

    time_t timeObject = timestamp.count() / 1000;
    tm* dateTimeStruct = std::localtime(&timeObject);

    std::ostringstream oss;
    oss << std::put_time(dateTimeStruct, "%m/%d/%Y %H:%M:%S");
    return oss.str();

}

int generateRandomInt() {
    std::string random_seed_str = bsoncxx::oid{}.to_string();
    std::seed_seq seed(random_seed_str.begin(), random_seed_str.end());
    std::mt19937 rng(seed);

    //this must be size-2 it is a C style string and so the final char will be null terminator
    std::uniform_int_distribution<std::mt19937::result_type> distribution(0, INT_MAX);

    return (int) distribution(rng);
}

bool isActiveMessageType(MessageSpecifics::MessageBodyCase message_body_case) {
    switch (message_body_case) {
        case MessageSpecifics::kTextMessage:
        case MessageSpecifics::kPictureMessage:
        case MessageSpecifics::kLocationMessage:
        case MessageSpecifics::kMimeTypeMessage:
        case MessageSpecifics::kInviteMessage:
            return true;

        case MessageSpecifics::kEditedMessage:
        case MessageSpecifics::kDeletedMessage:
        case MessageSpecifics::kUserKickedMessage:
        case MessageSpecifics::kUserBannedMessage:
        case MessageSpecifics::kDifferentUserJoinedMessage:
        case MessageSpecifics::kDifferentUserLeftMessage:
        case MessageSpecifics::kUpdateObservedTimeMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomStartMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage:
        case MessageSpecifics::kThisUserLeftChatRoomMessage:
        case MessageSpecifics::kUserActivityDetectedMessage:
        case MessageSpecifics::kChatRoomNameUpdatedMessage:
        case MessageSpecifics::kChatRoomPasswordUpdatedMessage:
        case MessageSpecifics::kNewAdminPromotedMessage:
        case MessageSpecifics::kNewPinnedLocationMessage:
        case MessageSpecifics::kChatRoomCapMessage:
        case MessageSpecifics::kMatchCanceledMessage:
        case MessageSpecifics::kNewUpdateTimeMessage:
        case MessageSpecifics::kHistoryClearedMessage:
        case MessageSpecifics::kLoadingMessage:
        case MessageSpecifics::MESSAGE_BODY_NOT_SET:
            break;
    }

    return false;
}

bsoncxx::array::value buildActiveMessageTypesDoc() {

    return bsoncxx::builder::stream::array{}
            << MessageSpecifics::kTextMessage
            << MessageSpecifics::kPictureMessage
            << MessageSpecifics::kLocationMessage
            << MessageSpecifics::kMimeTypeMessage
            << MessageSpecifics::kInviteMessage
            << bsoncxx::builder::stream::finalize;
}

LetsGoEventStatus convertExpirationTimeToEventStatus(
        const std::chrono::milliseconds & event_expiration_time,
        const std::chrono::milliseconds& current_timestamp
) {
    LetsGoEventStatus event_status;

    if(event_expiration_time == general_values::event_expiration_time_values::USER_ACCOUNT.value) {
        event_status = LetsGoEventStatus::NOT_AN_EVENT;
    } else if(event_expiration_time == general_values::event_expiration_time_values::EVENT_CANCELED.value) {
        event_status = LetsGoEventStatus::CANCELED;
    } else if(current_timestamp < event_expiration_time) {
        event_status = LetsGoEventStatus::ONGOING;
    } else { //Not canceled, but event is passed the expiration time has passed
        event_status = LetsGoEventStatus::COMPLETED;
    }

    return event_status;
}

//This will modify gender_range_vector and user_matches_with_everyone.
//If gender_range_vector is false, no valid elements were found.
void extractRepeatedGenderProtoToVector(
        const google::protobuf::RepeatedPtrField<std::basic_string<char>>& gender_range_list,
        std::vector<std::string>& gender_range_vector,
        bool& user_matches_with_everyone
) {

    const int gender_range_num_elements = std::min(gender_range_list.size(), server_parameter_restrictions::NUMBER_GENDER_USER_CAN_MATCH_WITH);

    //NOTE: Allowing chars other than the alphabet in gender names.
    std::set<std::string> gender_range_set;
    for (const auto& gender : gender_range_list) {
        if ((int)gender_range_set.size() == gender_range_num_elements) {
            break;
        }
        if (!isInvalidGender(gender)) { //if gender is valid
            //store valid gender
            gender_range_set.insert(gender);
        }
    }

    if (gender_range_set.empty()) { //if no genders in the list are valid
        return;
    }

    bsoncxx::builder::basic::array mongo_arr_gender_range_aggregation;
    bsoncxx::builder::basic::array mongo_arr_gender_range;
    user_matches_with_everyone = false;

    if (gender_range_set.find(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE) != gender_range_set.end()) {
        gender_range_vector.emplace_back(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
        user_matches_with_everyone = true;
    } else {

        for (const auto& gender: gender_range_set) {
            gender_range_vector.emplace_back(gender);
        }

        std::sort(gender_range_vector.begin(), gender_range_vector.end(), [](
                const std::string& lhs, const std::string& rhs
        ) -> bool {
            //NOTE: There may be a warning here however order is important here so don't fix it.
            if (lhs == general_values::MALE_GENDER_VALUE) {
                return true;
            } else if (rhs == general_values::MALE_GENDER_VALUE) {
                return false;
            } else if (lhs == general_values::FEMALE_GENDER_VALUE) {
                return true;
            } else if (rhs == general_values::FEMALE_GENDER_VALUE) {
                return false;
            }

            return lhs < rhs;
        });
    }
}