//
// Created by jeremiah on 3/18/21.
//
#pragma once

#include <mongocxx/collection.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>

#include <tbb/concurrent_vector.h>

#include <RequestFields.grpc.pb.h>
#include <MemberSharedInfoMessage.grpc.pb.h>
#include <AccountState.grpc.pb.h>
#include <SetFields.grpc.pb.h>
#include <AccountLoginTypeEnum.grpc.pb.h>
#include <TypeOfChatMessage.grpc.pb.h>
#include <reason_picture_deleted.h>


#include "chat_room_shared_keys.h"
#include "chat_room_header_keys.h"
#include "server_initialization_functions.h"
#include "chat_change_stream.h"
#include "chat_change_stream_values.h"
#include "user_pictures_keys.h"

struct AgeRangeDataObject {
    bool operator==(const AgeRangeDataObject& rhs) const {
        return min_age == rhs.min_age &&
               max_age == rhs.max_age;
    }

    bool operator!=(const AgeRangeDataObject& rhs) const {
        return !(rhs == *this);
    }

    int min_age = -1;
    int max_age = -1;
};

enum LoginTypesAccepted {
    LOGIN_TYPE_ONLY_CLIENT,
    LOGIN_TYPE_ONLY_ADMIN,
    LOGIN_TYPE_ADMIN_AND_CLIENT
};

enum MatchesWithEveryoneReturnValue {
    USER_MATCHES_WITH_EVERYONE,
    USER_DOES_NOT_MATCH_WITH_EVERYONE,
    MATCHES_WITH_EVERYONE_ERROR_OCCURRED
};

//returns true if a gender is INVALID and false if gender is VALID
bool isInvalidGender(const std::string& gender);

//checks if a location is invalid
bool isInvalidLocation(const double& longitude, const double& latitude);

//returns true if the phoneNumber is valid false if it is not
//NOTE: only taking north america phone numbers atm
bool isValidPhoneNumber(const std::string& phoneNumber);

//returns FALSE if all values are valid birthday values
bool isInvalidBirthday(
        const std::chrono::milliseconds& current_timestamp,
        int birth_year,
        int birth_month,
        int birth_day_of_month
);

//essentially mimics bsoncxx::to_json(), however shouldn't randomly crash or crash
// from having too long of documents sent to it OR crash from null/empty documents
std::string documentViewToJson(const bsoncxx::document::view& view);

//calculates age range (expects a valid age >= 13 && <= 120 )
AgeRangeDataObject calculateAgeRangeFromUserAge(int user_age);

//makes the string from bsoncxx::to_json(_, bsoncxx::v_noabi::ExtendedJsonMode::k_relaxed) pretty
// NOTE: this will not take into account for example strings that contain characters versus
std::string makePrettyJson(const bsoncxx::document::view& document_view);

//Deletes the account of the passed phoneNumber this is not called by users, only by server functions.
//NOTE: This will update currentTimestamp to the timestamp_stored of the message.
bool serverInternalDeleteAccount(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& userAccountsCollection,
        mongocxx::client_session* session,
        const bsoncxx::oid& userAccountOID,
        const std::chrono::milliseconds& currentTimestamp
);

//deletes all pictures inside the verified pictures array by calling findAndDeletePictureDocument
bool deleteAccountPictures(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::client_session* session,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::document::view& user_account_doc_view
);

/*
//serverInternalDeleteAccount helper function
//deletes a reference account to the passed verified account
bool deleteReferenceAccountById(mongocxx::database* accountsDB, mongocxx::collection* deleteAccountsErrorCollection,
                                const bsoncxx::document::view* verifiedAccountDocumentView,
                                const std::string& verifiedAccountIDKey, const std::string& accountCollectionName,
                                const std::string& referenceAccountIDKEY);
*/

/**
 * Finds a picture and deletes it. NOTE: This is a 'proper' delete. This means that
 * it will not remove the thumbnails from the chat rooms, it will just move it to
 * the deleted collection.
 **/
//extracts the picture document by oid then runs deletePictureDocument()
bool findAndDeletePictureDocument(
        mongocxx::client& mongoCppClient,
        mongocxx::collection& picturesCollection,
        const bsoncxx::oid& picturesDocumentOID,
        const std::chrono::milliseconds& currentTimestamp,
        mongocxx::client_session* session = nullptr
);

//saves the picture document to a separate file then deletes it at the passed ObjectID
//(does not change the verified account array, see removePictureArrayElement)
//will save the thumbnail of the passed document to thumbnail if not nullptr
// pictureDocumentView is expected to contain the entire document (noting projected out)
bool deletePictureDocument(const bsoncxx::document::view& pictureDocumentView,
                           mongocxx::collection& picturesCollection,
                           mongocxx::collection& deletedPicturesCollection,
                           const bsoncxx::oid& picturesDocumentOID,
                           const std::chrono::milliseconds& currentTimestamp,
                           ReasonPictureDeleted reason_picture_deleted,
                           const std::string& admin_name, //only used if ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE
                           mongocxx::client_session* session
);

//extracts min_age_range and max_age_range from the passed document and saves them to their respective values
bool extractMinMaxAgeRange(
        const bsoncxx::document::view& matching_account_view,
        int& min_age_range,
        int& max_age_range
);

//this function will check if the user has "everyone" selected as their genders to match with
//returning false here means an error occurred, the bool passed is the parameter
MatchesWithEveryoneReturnValue checkIfUserMatchesWithEveryone(
        const bsoncxx::array::view& genders_to_match_mongo_db_array,
        const bsoncxx::document::view& user_account_doc
);

//returns a tm for the passed month day and year (in GMT time)
tm initializeTmByDate(int birthYear, int birthMonth, int birthDayOfMonth);

//calculate age of person at passed time
//year needs to be a (4 digit most likely) value ex: 20xx
//month needs to be a value 1-12
//day needs to be a value 1-31
//NOTE: returns -1 if function fails
int calculateAge(const std::chrono::milliseconds& currentTime, int birthYear, int birthMonth, int birthDayOfMonth);

//year needs to be a (4 digit most likely) value ex: 20xx
//day of year needs to be a value [0-365] (same as tm_yday, ideally extracted from tm_yday)
//NOTE: returns -1 if function fails
int calculateAge(const std::chrono::milliseconds& currentTime, int birthYear, int birthDayOfYear);

//returns false if version is valid, true if not valid
bool isInvalidLetsGoDesktopInterfaceVersion(unsigned int letsGoVersion);

//Returns true if admin info is valid and false if an error occurs.
//If admin info matches will save the admin_account (from ADMIN_ACCOUNTS_COLLECTION_NAME) to
// a document, projecting out the name and password. This will be sent to admin_info_doc().
//If admin info does NOT match an error message will be sent to admin_info_doc().
bool validAdminInfo(
        const std::string& name,
        const std::string& password,
        const std::function<void(bsoncxx::stdx::optional<bsoncxx::document::value>& admin_info_doc,
                                 const std::string& error_message)>& store_admin_info
);

//validates LoginToServerBasicInfo values and saves them to the respective variables
// callerVersion = lets_go_version
// logged_in_token_str = logged_in_token
// installation_id = installation_id
// store_admin_info is only needed to be passed if the grpc function will be called from the desktop interface (don't pass it otherwise
//  that way std::exception will be thrown from bad function call and the server won't even try to run the function)
// returns ReturnStatus::SUCCESS if all values were valid
ReturnStatus isLoginToServerBasicInfoValid(
        LoginTypesAccepted login_types_accepted,
        const LoginToServerBasicInfo& login_info,
        std::string& user_account_oid,
        std::string& logged_in_token_str,
        std::string& installation_id,
        const std::function<void(bsoncxx::stdx::optional<bsoncxx::document::value>& admin_info_doc,
                                 const std::string& error_message)>& store_admin_info = [](
                bsoncxx::stdx::optional<bsoncxx::document::value>&, const std::string&) {}
);

//returns false if version is valid, true if not valid
bool isInvalidLetsGoAndroidVersion(unsigned int letsGoVersion);

//returns true if OID is valid, false if not valid
bool isInvalidOIDString(const std::string& stringOID);

//returns a UUID in string form
std::string generateUUID();

//returns false if installationID is valid, true if not valid
bool isInvalidUUID(const std::string& uuid_as_string);

//returns false if email is valid, true if not valid
bool isInvalidEmailAddress(const std::string& email);

//this takes in a MemberSharedInfoMessage and sets the CategoryActivityMessage to the values inside of matchCategories
//it assumes that except for the first one each stop time has a start time immediately before it
//NOTE: this does no cleaning of the match account
bool saveActivitiesToMemberSharedInfoMessage(
        const bsoncxx::oid& matchAccountOID,
        const bsoncxx::array::view& matchCategories,
        MemberSharedInfoMessage* responseMessage,
        const bsoncxx::document::view& matchingAccountDocument
);

//will remove excess pictures (if memberPictureTimestamps is greater than NUMBER_PICTURES_STORED_PER_ACCOUNT)
// from both the user account array and the user pictures collection
bool removeExcessPicturesIfNecessary(
        mongocxx::client& mongo_cpp_client,
        mongocxx::collection& user_account_collection,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::document::view& user_account_doc_view,
        std::vector<std::pair<std::string, std::chrono::milliseconds>>& memberPictureTimestamps
);

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
        std::set<int>& indexUsed);

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
        MemberSharedInfoMessage* responseMessage);

//extracts thumbnail from a document view from user pictures collection
bool extractThumbnailFromUserPictureDocument(
        const bsoncxx::document::view& pictureDocumentView,
        std::string& thumbnail,
        const std::function<void(const std::string& errorString)>& errorLambda
);

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
        const std::function<void(
                const bsoncxx::oid& picture_oid,
                std::string&& pictureByteString,
                int pictureSize,
                int indexNumber,
                const std::chrono::milliseconds& picture_timestamp,
                bool extracted_from_deleted_pictures,
                bool references_removed_after_delete
        )>& setPictureToResponse,
        const std::function<void(int)>& setPictureEmptyResponse,
        mongocxx::database* deleted_DB //If searching deleted pictures is not wanted; set to null. This will not search for each individual picture, but ALL pictures if NONE are found in accounts.
);

//this function stores a picture to a response
//the response is set in setPictureToResponse, param1 = picture in bytes, param2 = picture size, param3 = picture timestamp
//the response is set in setThumbnailToResponse, param1 = thumbnail in bytes, param2 = thumbnail size
bool extractPictureToResponse(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& userAccountCollection,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::document::view& userAccountDocView,
        const bsoncxx::oid& pictureOid,
        const bsoncxx::oid& userAccountOID,
        int indexOfArray,
        const std::function<void(
                std::string&& /*picture*/,
                int /*picture size*/,
                const std::chrono::milliseconds& /*picture timestamp*/
        )>& setPictureToResponse,
        bool extractThumbnail,
        const std::function<void(
                std::string&& /*thumbnail*/,
                int /*thumbnail_size*/,
                const std::chrono::milliseconds& /*thumbnail_timestamp*/
        )>& setThumbnailToResponse
);

//create a new login account at the phoneLoggedInCollection returns true for success or false for failure
//returnLoginIdString is a return value it is returned to android for the login ID
void logElementError(
        const int& lineNumber,
        const std::string& fileName,
        const bsoncxx::document::element& errorElement,
        const bsoncxx::document::view& documentView,
        const bsoncxx::type& type,
        const std::string& key,
        const std::string& databaseName,
        const std::string& collectionName
);

//will return SUCCESS if successfully logged on or the relevant error if login failed
// expects VERIFIED_LOGGED_IN_INFO and STATUS to be projected to the document
ReturnStatus checkForValidLoginToken(
        const bsoncxx::stdx::optional<bsoncxx::document::value>& findAndUpdateUserAccount,
        const std::string& userAccountOIDStr
);

//Overview; this function is used to remove obsolete references to chat rooms from user pictures.
//Specifics; extracts the user from the projected header document (chat_room_header_doc_view) and checks if the thumbnail reference
// is the same as the passed thumbnail reference, if it is NOT the same it will remove the chat room id from
// the user picture that was previously stored.
//NOTE: chat_room_header_doc_view should contain current user with the oid and the thumbnail reference projected
bool extractUserAndRemoveChatRoomIdFromUserPicturesReference(
        mongocxx::database& accounts_db,
        const bsoncxx::document::view& chat_room_header_doc_view,
        const bsoncxx::oid& current_user_oid,
        const std::string& updated_thumbnail_reference_oid,
        const std::string& chat_room_id,
        mongocxx::client_session* session = nullptr
);

bool removeChatRoomIdFromUserPicturesReference(
        mongocxx::database& accounts_db,
        const bsoncxx::oid& picture_id,
        const std::string& chat_room_id,
        mongocxx::client_session* session = nullptr
);

//generates a random alphanumeric string of the passed length
std::string gen_random_alpha_numeric_string(int len);

//if invalid accountId passed this will return "~"
//appends a string to the start of the account Ids passed based on account type
std::string errorCheckAccountIDAndGenerateUniqueAccountID(
        const AccountLoginType& accountType,
        const std::string& accountID
);

//remove leading whitespace
void trimLeadingWhitespace(std::string& str);

//remove trailing whitespace
void trimTrailingWhitespace(std::string& str);

//remove leading and trailing whitespace
void trimWhitespace(std::string& str);

//returns a string representing a date and time
std::string getDateTimeStringFromTimestamp(const std::chrono::milliseconds& timestamp);

//generates a random int between [0, INT_MAX]
int generateRandomInt();

//returns Unix timestamps since January 1,1970, UTC in Greenwich time
inline std::chrono::milliseconds getCurrentTimestamp() {
    //NOTE: C++ 20 gives a guarantee that time_since_epoch is relative to the UNIX epoch.
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch());
}

bool isActiveMessageType(MessageSpecifics::MessageBodyCase message_body_case);

bsoncxx::array::value buildActiveMessageTypesDoc();

//inside the user account document this will change an array index value to the default value of null
bool removePictureArrayElement(
        int index_of_array,
        mongocxx::collection& user_accounts_collection,
        const bsoncxx::document::view& user_account_doc_view,
        const bsoncxx::oid& user_account_oid,
        mongocxx::client_session* session = nullptr
);

LetsGoEventStatus convertExpirationTimeToEventStatus(
        const std::chrono::milliseconds& event_expiration_time,
        const std::chrono::milliseconds& current_timestamp
);

void extractRepeatedGenderProtoToVector(
        const google::protobuf::RepeatedPtrField<std::basic_string<char>>& gender_range_list,
        std::vector<std::string>& gender_range_vector,
        bool& user_matches_with_everyone
);

//created the user account document to upsert into the USER_PICTURES_COLLECTION
//NOTE: This will move file_in_bytes and thumbnail_in_bytes out of their pointers and NOT delete them.
inline bsoncxx::document::value createUserPictureDoc(
        const bsoncxx::oid& document_id,
        const bsoncxx::oid& user_account_reference,
        const std::chrono::milliseconds& timestamp_stored,
        const int picture_index,
        std::basic_string<char> *thumbnail_in_bytes,
        const int thumbnail_size_in_bytes,
        std::basic_string<char> *file_in_bytes,
        const int picture_size_in_bytes
) {
    return bsoncxx::builder::stream::document{}
            << "_id" << document_id
            << user_pictures_keys::USER_ACCOUNT_REFERENCE << user_account_reference
            << user_pictures_keys::THUMBNAIL_REFERENCES << bsoncxx::builder::stream::open_array << bsoncxx::builder::stream::close_array
            << user_pictures_keys::TIMESTAMP_STORED << bsoncxx::types::b_date{timestamp_stored}
            << user_pictures_keys::PICTURE_INDEX << bsoncxx::types::b_int32{picture_index}
            << user_pictures_keys::THUMBNAIL_IN_BYTES << std::move(*thumbnail_in_bytes)
            << user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES << bsoncxx::types::b_int32{thumbnail_size_in_bytes}
            << user_pictures_keys::PICTURE_IN_BYTES << std::move(*file_in_bytes)
            << user_pictures_keys::PICTURE_SIZE_IN_BYTES << bsoncxx::types::b_int32{picture_size_in_bytes}
        << bsoncxx::builder::stream::finalize;
}

//creates the user account document to upsert into the chat room array ACCOUNTS_IN_CHAT_ROOM
inline bsoncxx::document::value createChatRoomHeaderUserDoc(
        const bsoncxx::oid& account_oid,
        const AccountStateInChatRoom account_state,
        const std::string& user_name,
        const int& thumbnail_size,
        const std::string& thumbnail_reference_oid,
        const bsoncxx::types::b_date& last_activity_time,
        const std::chrono::milliseconds& current_timestamp
) {

    const auto current_timestamp_date_object = bsoncxx::types::b_date{current_timestamp};

    return bsoncxx::builder::stream::document{}
            << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << bsoncxx::types::b_oid{account_oid}
            << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{(int) account_state}
            << chat_room_header_keys::accounts_in_chat_room::FIRST_NAME << bsoncxx::types::b_string{user_name}
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << bsoncxx::types::b_string{thumbnail_reference_oid}
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << current_timestamp_date_object
            << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << thumbnail_size
            << chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << bsoncxx::types::b_date{last_activity_time}
            << chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT << bsoncxx::builder::stream::open_array
                << current_timestamp_date_object
            << bsoncxx::builder::stream::close_array
            << bsoncxx::builder::stream::finalize;
}

inline bool generateBirthYearForPassedAge(
        const int expected_age,
        int& birth_year,
        const int birth_month,
        const int birth_day_of_month,
        int& birth_day_of_year
) {

    const time_t time_object = getCurrentTimestamp().count() / 1000;

    tm current_date_time{};
    gmtime_r(&time_object, &current_date_time);

    if (current_date_time.tm_year == 0) { //failed to calculate proper date time
        //handle the error in calling function
        return false;
    }

    birth_year = (1900 + current_date_time.tm_year) - expected_age;

    const int current_month = current_date_time.tm_mon + 1;
    const int current_day_of_month = current_date_time.tm_mday;

    //if birthday has not yet happened this year
    if(birth_month > current_month
       || (birth_month == current_month
           && birth_day_of_month > current_day_of_month)
            ) {
        birth_year--;
    }

    //It would be possible for the birth_day_of_year to be off during a leap year.
    birth_day_of_year = initializeTmByDate(
            birth_year,
            birth_month,
            birth_day_of_month
    ).tm_yday;

    return true;
}

