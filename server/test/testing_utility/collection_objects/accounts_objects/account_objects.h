#pragma once

#include <optional>
#include <bsoncxx/oid.hpp>
#include <iostream>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <utility>
#include <UserAccountStatusEnum.grpc.pb.h>
#include <AlgorithmSearchOptions.grpc.pb.h>
#include <AccountCategoryEnum.grpc.pb.h>
#include <server_parameter_restrictions.h>
#include <matching_algorithm.h>
#include <database_names.h>
#include <collection_names.h>
#include <DisciplinaryActionType.grpc.pb.h>
#include <base_collection_object.h>
#include <AdminLevelEnum.grpc.pb.h>
#include <AccountLoginTypeEnum.grpc.pb.h>

#include "utility_general_functions.h"
#include "RequestUserAccountInfo.pb.h"
#include "MatchTypeEnum.grpc.pb.h"
#include "EventRequestMessage.grpc.pb.h"

struct TestTimeframe {
    long long time;
    int startStopValue; //1 for start time, -1 for stop time

    TestTimeframe() = delete;

    TestTimeframe(const long long& _time, int _startStopValue) :
            time(_time), startStopValue(_startStopValue) {}

    bool operator==(const TestTimeframe& rhs) const {
        return time == rhs.time &&
               startStopValue == rhs.startStopValue;
    }

    bool operator!=(const TestTimeframe& rhs) const {
        return !(rhs == *this);
    }
};

struct TestCategory {
    AccountCategoryType type; //"tYp"; //int32; enum representing what type of document this is, follows AccountCategoryType
    int index_value; //"iAc"; //int32; integer value of activity or category
    std::vector<TestTimeframe> time_frames; //"aTf"; //array of documents containing: time (mongoDBDate), startStopValue; (1 is start time -1 if stop time)

    TestCategory() = delete;

    TestCategory(AccountCategoryType _type,
                 int _index_value) :
            type(_type),
            index_value(_index_value) {}

    TestCategory(AccountCategoryType _type, int _index_value, std::vector<TestTimeframe> _timeFrames) :
            type(_type), index_value(_index_value), time_frames(std::move(_timeFrames)) {}

    void convertToCategoryActivityMessage(CategoryActivityMessage* category_ele) const {
        category_ele->set_activity_index(index_value);

        if (!time_frames.empty()) {
            CategoryTimeFrameMessage* time_frame_message = nullptr;
            bool first_ele = true;
            for (const auto& frame: time_frames) {
                if (first_ele) { //if this is the first element
                    first_ele = false;
                    if (frame.startStopValue == -1) { //if first element is a stop time
                        //it is valid for the first element to be a stop time, so create a pair to be returned
                        time_frame_message = category_ele->add_time_frame_array();
                        time_frame_message->set_start_time_frame(-1L);
                        time_frame_message->set_stop_time_frame(frame.time);

                        time_frame_message = nullptr;
                    } else if (frame.startStopValue == 1) { //if first element is a start time
                        //add a new gRPC array element and set the start time
                        time_frame_message = category_ele->add_time_frame_array();
                        time_frame_message->set_start_time_frame(frame.time);
                        time_frame_message->set_stop_time_frame(-1L); //set a default value
                    } else { //startStopTime was not 1 or -1
                        //an error with formatting occurred
                        assert(false);
                    }
                } else if (frame.startStopValue == -1 &&
                           time_frame_message != nullptr) { //if time is a stop time

                    //set the time then put the pointer to the gRPC element to null
                    time_frame_message->set_stop_time_frame(frame.time);
                    time_frame_message = nullptr;
                } else if (frame.startStopValue == 1) { //if time is a start time

                    //add a new gRPC array element and set the start time
                    time_frame_message = category_ele->add_time_frame_array();
                    time_frame_message->set_start_time_frame(frame.time);
                    time_frame_message->set_stop_time_frame(-1L); //set a default value
                }
            }
        }
    }

};

struct MatchingElement {

    bsoncxx::oid oid{}; //"oIe"; //oID; account OID of the matched account (this is for both the matching account document and the verified account document)
    double point_value = 0; //"dPv"; //double; point value of the matched account
    double distance = 0; //"dDs"; //double; distance in miles of the matched account
    bsoncxx::types::b_date expiration_time = bsoncxx::types::b_date{std::chrono::milliseconds{
            -1}}; //"dEx"; //mongoDB Date; expiration time of the matched account (this is the raw expiration time, it does not have TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED factored in)
    bsoncxx::types::b_date match_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{
            -1}}; //"dTs"; //mongoDB Date; timestamp this match was made, used to compare against account timestamps to see if match still valid
    //inline const std::string activity_statistics = "aCs"; //array of documents; final match result containing some potentially useful values for this match (not actually stored in above arrays, just used for statistics before those are initialized)
    bool from_match_algorithm_list = false; //"bFm"; //bool; true if from ALGORITHM_MATCHED_ACCOUNTS_LIST false if from OTHER_USERS_MATCHED_ACCOUNTS_LIST
    //NOTE: this is optional and will be set to bsoncxx::oid("000000000000000000000000") if it doesn't exist
    bsoncxx::oid saved_statistics_oid{}; //"oSs"; //oID; this is the oid to the location where the saved statistics are stored; NOTE: if an error occurs this field will not exist

    std::shared_ptr<bsoncxx::builder::basic::array> activity_statistics = nullptr;

    void initializeActivityStatistics() {
        activity_statistics = std::make_shared<bsoncxx::builder::basic::array>();
    }

    MatchingElement() = default;

    MatchingElement(
            const bsoncxx::oid& _oid,
            const double& _point_value,
            const double& _distance,
            const bsoncxx::types::b_date& _expiration_time,
            const bsoncxx::types::b_date& _match_timestamp,
            bool _from_match_algorithm_list,
            bsoncxx::oid _saved_statistics_oid
    ) :
            oid(_oid),
            point_value(_point_value),
            distance(_distance),
            expiration_time(_expiration_time),
            match_timestamp(_match_timestamp),
            from_match_algorithm_list(_from_match_algorithm_list),
            saved_statistics_oid(_saved_statistics_oid),
            activity_statistics(nullptr) {}

    void generateRandomValues();

    //NOTE: activity_statistics can be sent to this OR inserted above. The convertToDocument() methods are const
    // and so sometimes they cannot update the value.
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

};

void extractArrayOfMatchingElementsToDocument(
        const std::string& key,
        const std::vector<MatchingElement>& matching_array,
        bsoncxx::builder::stream::document& document_result
);

void checkMatchingElementEquality(
        const MatchingElement& first_matching_element,
        const MatchingElement& other_matching_element,
        const std::string& key,
        const std::string& object_class_name,
        bool& return_value
);

void checkMatchingElementVectorEquality(
        const std::vector<MatchingElement>& current_user_matched_with,
        const std::vector<MatchingElement>& other_user_matched_with,
        const std::string& key,
        const std::string& object_class_name,
        bool& return_value
);

MatchingElement extractMatchingElementToClass(
        const bsoncxx::document::view& match_element_doc
);

void extractMatchingElementToVector(
        std::vector<MatchingElement>& matching_array,
        const bsoncxx::array::view& matching_array_doc
);

class AccountRecoveryDoc : public BaseCollectionObject {
public:
    std::string verification_code; //"vC"; //string NOTE: accessed on django web server; make sure to change there if string is changed
    std::string phone_number; //"pN"; //string NOTE: accessed on django web server; make sure to change there if string is changed
    bsoncxx::types::b_date time_verification_code_generated = DEFAULT_DATE; //"tC"; //mongoDB Date Type; NOTE: accessed on django web server; make sure to change there if string is changed
    int number_attempts = 0; //"nA"; //int32; number of times user has attempted to 'guess' (or just got it wrong) correct current phone number NOTE: accessed on django web server; make sure to change there if string is changed
    bsoncxx::oid user_account_oid; //"iD"; //oid; the current account oid of the account

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    bool getFromCollection(const std::string& _phone_number);

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    AccountRecoveryDoc() = default;

    explicit AccountRecoveryDoc(const std::string& _phone_number) {
        getFromCollection(_phone_number);
    }

    explicit AccountRecoveryDoc(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const AccountRecoveryDoc& v);

    bool operator==(const AccountRecoveryDoc& other) const;

    bool operator!=(const AccountRecoveryDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Account Recovery Doc";
};

class ActivitiesInfoDoc : public BaseCollectionObject {
public:
    struct CategoryDoc {
        std::string display_name; //SHARED_DISPLAY_NAME_KEY; //string; name of the category
        std::string icon_display_name; //SHARED_ICON_DISPLAY_NAME_KEY; //string; name of the category
        double order_number; //"order_num"; //double; number that will be used for sorting categories on the client (sorted by ascending)
        int min_age; //SHARED_MIN_AGE_KEY; //int32; minimum age allowed to access this category inclusive (ex: 18 would mean 18+ can see it); this is also used to 'delete' an activity by setting it over HIGHEST_ALLOWED_AGE
        std::string stored_name; //"stored_name"; //string; stored name for the category
        std::string color; //"color"; //string; hex color code for this category

        CategoryDoc(
                std::string _display_name,
                std::string _icon_display_name,
                double _order_number,
                int _min_age,
                std::string _stored_name,
                std::string _color
        ) :
                display_name(std::move(_display_name)),
                icon_display_name(std::move(_icon_display_name)),
                order_number(_order_number),
                min_age(_min_age),
                stored_name(std::move(_stored_name)),
                color(std::move(_color)) {}
    };

    struct ActivityDoc {
        std::string display_name; //SHARED_DISPLAY_NAME_KEY; //string; name of the activity
        std::string icon_display_name; //SHARED_ICON_DISPLAY_NAME_KEY; //string; name of the activity
        int min_age; //SHARED_MIN_AGE_KEY; //int32; minimum age allowed to access this category inclusive (ex: 18 would mean 18+ can see it); this is also used to 'delete' an activity by setting it over HIGHEST_ALLOWED_AGE
        std::string stored_name; //"stored_name"; //string; stored name for the category
        int category_index; //"category_index"; //int32; index of the category this activity 'belongs' to
        int icon_index; //"icon_index"; //int32; index of the icon this uses

        ActivityDoc(
                std::string _display_name,
                std::string _icon_display_name,
                int _min_age,
                std::string _stored_name,
                int _category_index,
                int _icon_index
        ) :
                display_name(std::move(_display_name)),
                icon_display_name(std::move(_icon_display_name)),
                min_age(_min_age),
                stored_name(std::move(_stored_name)),
                category_index(_category_index),
                icon_index(_icon_index) {}
    };

    //Will be set to activities_info_keys::ID if this document has been populated from a collection. Will be empty
    // if not.
    std::string id;

    //each document in this collection will represent either an activity or a category, the _id is overridden to represent the index
    std::vector<CategoryDoc> categories; //"all_categories"; //array of documents; each element represents a single category, uses SHARED_MIN_AGE_KEY & COLOR
    std::vector<ActivityDoc> activities; //"all_activities"; //array of documents; each element represents a single activity, uses SHARED_MIN_AGE_KEY, CATEGORY_INDEX & ICON_INDEX

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    ActivitiesInfoDoc() = default;

    explicit ActivitiesInfoDoc(bool run [[maybe_unused]]) {
        getFromCollection();
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const ActivitiesInfoDoc& v);

    bool operator==(const ActivitiesInfoDoc& other) const;

    bool operator!=(const ActivitiesInfoDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Activities Info Doc";
};

class AdminAccountDoc : public BaseCollectionObject {
public:
    std::string name; //"name"; //string; name for this admin
    std::string password; //"pass"; //string; password for this admin
    AdminLevelEnum privilege_level = AdminLevelEnum::NO_ADMIN_ACCESS; //"privilege_level"; //int32; follows AdminLevelEnum inside AdminLevelEnum.proto

    bsoncxx::types::b_date last_time_extracted_age_gender_statistics = DEFAULT_DATE; //"lst_age_gender"; //MongoDB Date; last time this user extracted age gender statistics

    bsoncxx::types::b_date last_time_extracted_reports = DEFAULT_DATE; //"lst_reports"; //MongoDB Date; last time this user extracted reports
    bsoncxx::types::b_date last_time_extracted_blocks = DEFAULT_DATE; //"lst_blocks"; //MongoDB Date; last time this user extracted blocks

    bsoncxx::types::b_date last_time_extracted_feedback_activity = DEFAULT_DATE; //"lst_feedback_activity"; //MongoDB Date; last time this user extracted activity type feedback
    bsoncxx::types::b_date last_time_extracted_feedback_other = DEFAULT_DATE; //"lst_feedback_other"; //MongoDB Date; last time this user extracted other type feedback
    bsoncxx::types::b_date last_time_extracted_feedback_bug = DEFAULT_DATE; //"lst_feedback_bug"; //MongoDB Date; last time this user extracted bug report type feedback

    long number_feedback_marked_as_spam = 0; //"num_feedback_marked_spam"; //int64; number of times the admin pushed 'mark as spam' button for feedback
    long number_reports_marked_as_spam = 0; //"num_reports_marked_spam"; //int64; number of times the admin pushed 'mark as spam' button for reports

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    bool getFromCollection(const std::string& admin_name, const std::string& admin_password);

    AdminAccountDoc() = default;

    explicit AdminAccountDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    explicit AdminAccountDoc(const std::string& admin_name, const std::string& admin_password) {
        getFromCollection(admin_name, admin_password);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const AdminAccountDoc& v);

    bool operator==(const AdminAccountDoc& other) const;

    bool operator!=(const AdminAccountDoc& other) const {
        return !(*this == other);
    }

private:

    bool extractDocument(bsoncxx::document::value&& find_doc);

    const std::string OBJECT_CLASS_NAME = "Admin Account Doc";
};

class EmailVerificationDoc : public BaseCollectionObject {
public:
    bsoncxx::oid user_account_reference; //"_id"; //Object Id; NOTE: accessed on django web server; make sure to change there if string is changed
    std::string verification_code; //"vC"; //string; NOTE: accessed on django web server; make sure to change there if string is changed
    bsoncxx::types::b_date time_verification_code_generated = DEFAULT_DATE; //"tC"; //mongoDB Date Type; NOTE: accessed on django web server; make sure to change there if string is changed
    std::string address_being_verified; //"eA"; //string; NOTE: accessed on django web server; make sure to change there if string is changed

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    EmailVerificationDoc() = default;

    explicit EmailVerificationDoc(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const EmailVerificationDoc& v);

    bool operator==(const EmailVerificationDoc& other) const;

    bool operator!=(const EmailVerificationDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Email Verification Doc";
};

class IconsInfoDoc : public BaseCollectionObject {
public:
    //inline const std::string INDEX = "_id"; //int64, index of the icon
    int index = 0;
    bsoncxx::types::b_date timestamp_last_updated = DEFAULT_DATE; //"ts"; //mongoDB Date, time this icon last received an update, will be set to -2 if file corrupt or inactive
    std::string icon_in_bytes; //string; byte array for the basic image
    long icon_size_in_bytes = 0; //int64; basic image size in bytes
    bool icon_active = false; //bool; true if icon is active, false if icon is disabled (note this is opposite of how android handles it where it uses isDeleted as the bool)

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    void convertToDocument(
            bsoncxx::builder::stream::document& document_result,
            bool skip_long_strings
    ) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection(long passed_index);

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    IconsInfoDoc() = default;

    explicit IconsInfoDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    explicit IconsInfoDoc(long passed_index) {
        getFromCollection(passed_index);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const IconsInfoDoc& v);

    bool operator==(const IconsInfoDoc& other) const;

    bool operator!=(const IconsInfoDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Icons Info Doc";
};

class InfoStoredAfterDeletionDoc : public BaseCollectionObject {
public:
    std::string phone_number; //"pN"; //string
    bsoncxx::types::b_date time_sms_can_be_sent_again = DEFAULT_DATE; //"tC"; //mongoDB Date Type; time when another SMS message is allowed to be sent for account authorization, this will ONLY be used if the verified account does not exist, otherwise TIME_SMS_CAN_BE_SENT_AGAIN is used
    bsoncxx::types::b_date time_email_can_be_sent_again = DEFAULT_DATE; //"tE"; //mongoDB Date Type; time when another SMS message is allowed to be sent for account authorization, this will ONLY be used if the verified account does not exist, otherwise TIME_EMAIL_CAN_BE_SENT_AGAIN is used
    int cool_down_on_sms = 0; //"sC"; //int32; time until sms is off cool down NOTE: IN SECONDS; NOTE: This is meant to be generated and immediately extracted, it should never be relied on to be correct.
    int number_swipes_remaining = 0; //"nSr"; //int32; the total number of swipes remaining for this phone number NOTE: mirrors NUMBER_SWIPES_REMAINING
    long swipes_last_updated_time = 0; //"lU"; //int64 (NOT MONGODB DATE TYPE); this is unixTimestamp/TIME_BETWEEN_SWIPES_UPDATED so it is not the full timestamp NOTE: mirrors SWIPES_LAST_UPDATED_TIME

    int number_failed_sms_verification_attempts = 0; //"nFa"; //int32; the total number of times sms verification guesses have failed NOTE: works with FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME; Set as an int64 in order to make sure a user cannot input so many attempts, it loops back to zero.
    long failed_sms_verification_last_update_time = 0; //"vLu"; //int64; (NOT MONGODB DATE TYPE); this is unixTimestamp/TIME_BETWEEN_VERIFICATION_ATTEMPTS, so it is not the full timestamp NOTE: works with NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection(const std::string& phone_number);

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    InfoStoredAfterDeletionDoc() = default;

    explicit InfoStoredAfterDeletionDoc(const std::string& phone_number) {
        getFromCollection(phone_number);
    }

    explicit InfoStoredAfterDeletionDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const InfoStoredAfterDeletionDoc& v);

    bool operator==(const InfoStoredAfterDeletionDoc& other) const;

    bool operator!=(const InfoStoredAfterDeletionDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Info Stored After Deletion Doc";
};

class PendingAccountDoc : public BaseCollectionObject {
public:
    AccountLoginType type = AccountLoginType::LOGIN_TYPE_VALUE_NOT_SET; //"aT"; //int follows AccountLoginType;
    std::string phone_number; //"pN"; //string; used with all account types to store the phone number, allows for only 1 pending account per phone number (even if it is redundant when INDEXING is also a phone number)
    std::string indexing; //"aI"; //string; will be phone number if accountType is phone or accountID if accountType is facebook or google
    std::string id; //"dI"; //string
    std::string verification_code; //"vC"; //string
    bsoncxx::types::b_date time_verification_code_was_sent = DEFAULT_DATE; //"tC"; //mongoDB Date Type; note this is used in TTL indexing (removing the account) so it stores the time the code was sent, NOT the time it expires

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const std::string& _phone_number);

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    PendingAccountDoc() = default;

    explicit PendingAccountDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    explicit PendingAccountDoc(const std::string& _phone_number) {
        getFromCollection(_phone_number);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const PendingAccountDoc& v);

    bool operator==(const PendingAccountDoc& other) const;

    bool operator!=(const PendingAccountDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Pending Account Doc";
};

class UserAccountDoc : public BaseCollectionObject {

public:

    inline static const bsoncxx::types::b_date DEFAULT_DATE{std::chrono::milliseconds{-1}};
    inline static const std::string DEFAULT_STRING = "~";
    inline static const int DEFAULT_INT = -1;
    inline static const int INT_ZERO = 0;

    UserAccountStatus status = UserAccountStatus::STATUS_REQUIRES_MORE_INFO; //"iAs"; //int32; based on enum UserAccountStatus
    std::string inactive_message = DEFAULT_STRING; //"sIm"; //string; message to be sent back for some inactive status ex: banned, suspended
    bsoncxx::types::b_date inactive_end_time{std::chrono::milliseconds{
            -1L}}; //"iEt"; //mongodb Date or null; if this is a date type this will be the time the user becomes un-suspended
    int number_of_times_timed_out = 0; //"nTo"; //int32; number of times this account has been timed out (used as a counter to see when a time-out bans)
    std::vector<std::string> installation_ids; //"aIi"; //array of strings; all installation ids that have been logged into this account, this array has restricted size set by MAX_NUMBER_OF_INSTALLATION_IDS_STORED; NOTE: this array will be cleared in SMSVerification before the account is deleted, so never set this to empty
    bsoncxx::types::b_date last_verified_time{std::chrono::milliseconds{-1L}}; //"dVt"; //mongoDB Date
    bsoncxx::types::b_date time_created{std::chrono::milliseconds{-1L}}; //"dCt"; //mongoDB Date

    std::vector<std::string> account_id_list; //"sAI"; //array of strings; these are the accountIDs prefixed by FACEBOOK_ACCOUNT_ID_PREFIX or GOOGLE_ACCOUNT_ID_PREFIX

    UserSubscriptionStatus subscription_status = UserSubscriptionStatus::NO_SUBSCRIPTION; //"sUs"; //int32; An enum representing the current users' subscription tier. Follows UserSubscriptionStatus enum from UserSubscriptionStatus.proto.
    bsoncxx::types::b_date subscription_expiration_time{std::chrono::milliseconds{-1L}}; //"sEt"; //mongoDB Date; If the user is subscribed (SUBSCRIPTION_STATUS > NO_SUBSCRIPTION), this will be the time the subscription expires.
    UserAccountType account_type = UserAccountType::UNKNOWN_ACCOUNT_TYPE; //"aCt"; //int32; An enum representing the type of account this is (user or event types). Follows UserAccountType enum from UserAccountType.proto.

    std::string phone_number; //"sPn"; //string;
    std::string first_name = DEFAULT_STRING; //"sFn"; //string;
    std::string bio = DEFAULT_STRING; //"sBi"; //string;
    std::string city = DEFAULT_STRING; //"sCn"; //string;
    std::string gender = DEFAULT_STRING; //"sGe"; //string; "~"=unset, MALE_GENDER_VALUE=male, FEMALE_GENDER_VALUE=female, can also be "other"
    int birth_year = DEFAULT_INT; //"iBy"; //int32; -1=unset year format ex: 2012
    int birth_month = DEFAULT_INT; //"iBm"; //int32; -1=unset months are stored as values 1-12
    int birth_day_of_month = DEFAULT_INT; //"iBd"; //int32; -1=unset day of month 1-xx
    int birth_day_of_year = DEFAULT_INT; //"iBo"; //int32; this is the day of the year, taken from tm_yday, so it goes from [0-365]
    int age = DEFAULT_INT; //"iAg"; //int32; -1=unset day of month 1-xx
    std::string email_address = DEFAULT_STRING; //"sEa"; //string; the email address

    bool email_address_requires_verification = true; //"bEv"; //bool //NOTE: this is accessed in web server
    bool opted_in_to_promotional_email = true; //"pRo"; //bool; True if the user has opted in to receive promotional emails, false if not.

    struct PictureReference {
        //if default constructor is used, it will default to no stored picture reference
        PictureReference() = default;

        //NOTE: not strictly accurate but simpler than using a template
        PictureReference(
                const bsoncxx::oid _picture_reference,
                bsoncxx::types::b_date _timestamp_stored
        ) :
                picture_reference(_picture_reference),
                timestamp_stored(_timestamp_stored) {
            storedPicture = true;
        }

        //sets the picture to the parameter
        void setPictureReference(
                const bsoncxx::oid _picture_reference,
                bsoncxx::types::b_date _timestamp_stored
        ) {
            picture_reference = _picture_reference;
            timestamp_stored = _timestamp_stored;
            storedPicture = true;
        }

        //'removes' the picture by setting the stored state to false
        void removePictureReference() {
            storedPicture = false;
        }

        //if a picture reference is stored it will save it to the parameter
        //if no picture reference it will do nothing
        void getPictureReference(
                bsoncxx::oid& _picture_reference,
                bsoncxx::types::b_date& _timestamp_stored
        ) const {
            if (storedPicture) {
                _picture_reference = picture_reference;
                _timestamp_stored = timestamp_stored;
            }
        }

        //returns true if picture stored, false if not
        [[nodiscard]] bool pictureStored() const {
            return storedPicture;
        }

        bool operator==(const PictureReference& other) const {
            const std::string className = "Picture Reference Inside User Account";
            std::string valueStr;

            if (storedPicture != other.storedPicture) {
                valueStr = "Stored Picture";
                std::cout << "ERROR: " << className << ' ' << valueStr << " Variables Do Not Match\n"
                        << valueStr << " 1: " << storedPicture << '\n'
                        << valueStr << " 2: " << other.storedPicture << '\n';
                return false;
            } else if (
                    storedPicture &&
                    picture_reference.to_string() != other.picture_reference.to_string()
                    && timestamp_stored.value.count() != other.timestamp_stored.value.count()
                    ) { //if storedPicture is true then these need to be equal
                valueStr = "Picture Reference";
                std::cout << "ERROR: " << className << ' ' << valueStr << " Variables Do Not Match\n"
                        << valueStr << " 1: " << picture_reference.to_string() << '\n'
                        << valueStr << " 2: " << other.picture_reference.to_string() << '\n'
                        << valueStr << " 3: " << timestamp_stored.value.count() << '\n'
                        << valueStr << " 4: " << other.timestamp_stored.value.count() << '\n';
                return false;
            }

            return true;
        }

        bool operator!=(const PictureReference& other) const {
            return !(*this == other);
        }

        friend std::ostream& operator<<(std::ostream& os, const PictureReference& reference) {
            os << "storedPicture: " << reference.storedPicture << " picture_reference: " << reference.picture_reference.to_string() << " timestamp_stored: " << reference.timestamp_stored;
            return os;
        }

    private:
        bool storedPicture = false;
        bsoncxx::oid picture_reference; //"dId"; //OID; the oid reference of the picture
        bsoncxx::types::b_date timestamp_stored{
                std::chrono::milliseconds{-1}}; //"dTs"; //mongoDB Date; the timestamp the picture was stored
    };

    std::vector<PictureReference> pictures; //"aPI"; //array of Documents; Object Id and timestamp for picture; NOTE: these could be set to null if there is no picture stored and it may not be as large as NUMBER_PICTURES_STORED_PER_ACCOUNT, however it should never be larger

    AlgorithmSearchOptions search_by_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY; //"sBo"; //int32, follows AlgorithmSearchOptions enum from SetFields.proto

    std::vector<TestCategory> categories{
            TestCategory(AccountCategoryType::ACTIVITY_TYPE, 0),
            TestCategory(AccountCategoryType::CATEGORY_TYPE, 0)
    }; //"aCa"; //array of documents containing: activity (type int32), timeframes(document of time (int64) and startStop (1 for start time -1 for stop time))

    struct EventValues {
        std::string created_by; //"cB"; //string; The account oid (in string form) that created the event if created by a user OR the admin NAME if created by an admin.
        std::string chat_room_id; //"cId"; //string; The chat room id for the event.
        std::string event_title; //"tI"; //string; The title of the event. This is useful to allow spaces in it which first name does not.

        EventValues() = delete;

        EventValues(
                std::string _created_by,
                std::string _chat_room_id,
                std::string _event_title
        ) :
                created_by(std::move(_created_by)),
                chat_room_id(std::move(_chat_room_id)),
                event_title(std::move(_event_title)) {}
    };

    std::optional<EventValues> event_values{}; //document or does not exist; This will contain the values specific to an event. This document will only exist when ACCOUNT_TYPE is an event type.

    bsoncxx::types::b_date event_expiration_time{std::chrono::milliseconds{
            -1L}}; //"eEt"; //mongodb date; The time that this match can no longer be searched for. This field will exist regardless for the collection indexes.

    struct UserCreatedEvent {
        bsoncxx::oid event_oid; //"oId"; //oid; The oid of the user created event.
        bsoncxx::types::b_date expiration_time; //"eXt"; //mongodb date; The expiration time of the created event. Will follow EVENT_EXPIRATION_TIME of the event document (will never change to canceled).
        LetsGoEventStatus event_state; //"eVc"; //int32; roughly follows LetsGoEventStatus boolean, it is only ever set to ONGOING or CANCELED (used to determine if the event was canceled)

        UserCreatedEvent() = delete;

        UserCreatedEvent(
                bsoncxx::oid _event_oid,
                bsoncxx::types::b_date _expiration_time,
                LetsGoEventStatus _event_state
        ) :
                event_oid(_event_oid),
                expiration_time(_expiration_time),
                event_state(_event_state) {}
    };

    std::vector<UserCreatedEvent> user_created_events; //"uCe"; //array of documents; These are the events that this user has personally created.

    struct OtherAccountMatchedWith {
        std::string oid_string; //"sId"; //string; string representing oid of the match
        bsoncxx::types::b_date timestamp; //"dTs"; //mongoDB date; timestamp in mongodb date format that match was made

        OtherAccountMatchedWith() = delete;

        OtherAccountMatchedWith(
                std::string _oid_string,
                bsoncxx::types::b_date _timestamp
        ) :
                oid_string(std::move(_oid_string)),
                timestamp(_timestamp) {}

    };

    std::vector<OtherAccountMatchedWith> other_accounts_matched_with; //"aOa"; //array of documents; the other users this person has matched with and not yet opened a connection to; contains: oid of match (type string), timestamp(mongoDB date)

    bool matching_activated = false; //"bAt"; //bool; true if account is actively taking matches, false if not (this could be mixed with STATUS however leaving it separate so that a bool can be added to turn off matches later)

    struct Location {
        double longitude;
        double latitude;

        Location() = delete;

        Location(double _longitude, double _latitude) :
                longitude(_longitude), latitude(_latitude) {}
    };

    Location location = Location(-122, 37);

    bsoncxx::types::b_date find_matches_timestamp_lock{std::chrono::milliseconds{
            -1L}}; //"tSl"; //mongoDBDate, timestamp the find_matches is 'available' to run again
    bsoncxx::types::b_date last_time_find_matches_ran{std::chrono::milliseconds{
            -1L}}; //"dFr"; //mongoDBDate, timestamp of the last time this account was used (last time the algorithm was called from this account)

    struct AgeRange {
        int min; //"mIn"; //int32; min age range this user will accept
        int max; //"mAx"; //int32; min age range this user will accept

        AgeRange() = delete;

        AgeRange(int _min, int _max) :
                min(_min), max(_max) {}
    };

    AgeRange age_range{-1,-1}; //"aAr"; //document; contains fields for min and max age range this user will accept (it is a document for indexing purposes, arrays cannot index by the array index ex. AGE_RANGE.0 does not use the index)

    std::vector<std::string> genders_range; //"aGr"; //array of strings; list of genders this user matches with; values can be MALE_GENDER_VALUE for male FEMALE_GENDER_VALUE for female or the name of the selected gender (can be multiple gender)
    int max_distance = server_parameter_restrictions::DEFAULT_MAX_DISTANCE; //"iMd"; //int32; max Distance this account accepts in miles

    struct PreviouslyMatchedAccounts {
        bsoncxx::oid oid; //"oId"; //oid; account previously matched; using matching account oID
        bsoncxx::types::b_date timestamp; //"dPt"; //mongoDB Date; most recent time account was matched
        int number_times_matched; //"iPa"; //int32; total number of times the account has been matched with; NOTE: NEVER LET THIS BE 0

        PreviouslyMatchedAccounts() = delete;

        PreviouslyMatchedAccounts(
                bsoncxx::oid _oid,
                bsoncxx::types::b_date _timestamp,
                int _number_times_matched
        ) :
                oid(_oid), timestamp(_timestamp), number_times_matched(_number_times_matched) {}
    };

    std::vector<PreviouslyMatchedAccounts> previously_matched_accounts; //"aPm"; //array of documents; stores the matching account oID that it was previously matched with, the most recent time matched and the number of times matched

    bsoncxx::types::b_date last_time_empty_match_returned{std::chrono::milliseconds{
            -1L}}; //"dLe"; //mongoDB Date; this is the timestamp of the last time an empty match array was returned, it is reset to -1 when any match criteria is changed; used with TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND
    bsoncxx::types::b_date last_time_match_algorithm_ran{std::chrono::milliseconds{
            -1L}}; //"dMn"; //mongoDB Date; this is the timestamp of the last time the match algorithm ran, it prevents it from being spammed; used with TIME_BETWEEN_ALGORITHM_RUNS; this is actually updated in 2 places in case an error occurs during algorithm runtime

    int int_for_match_list_to_draw_from = INT_ZERO; //"iDf"; //int32; this value can be anything between 1 and whatever number is max, if it is at max it will be reset to 0 (this number is used with NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER to decide whether to draw from 'other users said yes' list or 'algorithm match' list)
    int total_number_matches_drawn = INT_ZERO; //"iMr"; //int32; this value will start at 0 and be incremented each time a match is returned

    //lists storing potential matches for this account
    std::vector<MatchingElement> has_been_extracted_accounts_list; //"aHe"; //array of documents; list of accounts a device has requested
    std::vector<MatchingElement> algorithm_matched_accounts_list; //"aMa"; //array of documents; list of accounts this user has algorithmically been matched with
    std::vector<MatchingElement> other_users_matched_accounts_list; //"aOu"; //array of documents; list of accounts that selected 'yes' on this user

    bsoncxx::types::b_date time_sms_can_be_sent_again{std::chrono::milliseconds{
            -1L}}; //"dIs"; //mongoDB Date; time when another SMS message is allowed to be sent for account authorization
    int number_swipes_remaining = matching_algorithm::MAXIMUM_NUMBER_SWIPES; //"iSr"; //int32; the total number of swipes remaining for this phone number
    long swipes_last_updated_time = -1; //"iSt"; //int64 (NOT MONGODB DATE TYPE); this is unixTimestamp/TIME_BETWEEN_SWIPES_UPDATED, so it is not the full timestamp

    std::string logged_in_token = DEFAULT_STRING; //"sLt"; //string; for now this just an OID converted to a string however, in the future it can become a more secure token, so leaving the type as utf8
    bsoncxx::types::b_date logged_in_token_expiration{
            std::chrono::milliseconds{-1L}}; //"dEt"; //mongoDB Date; login token expiration time
    std::string logged_in_installation_id = DEFAULT_STRING; //"sDi"; //string; ID of device used to log into this account

    //These 2 fields are for returning things to the user when using the findOneAndUpdate function
    int cool_down_on_sms_return_message = DEFAULT_INT; //"iCs"; //int32; time until sms is off cool down NOTE: IN SECONDS
    int logged_in_return_message = DEFAULT_INT; //"iRm"; //int32; follows ReturnStatus enum, or LoginFunctionResultValuesEnum depending on what calls it; this value not meant to be updated outside attempting to log in

    bsoncxx::types::b_date time_email_can_be_sent_again{
            std::chrono::milliseconds{-1L}}; //"aEC"; //mongoDB Date; time the email comes off cool down

    bsoncxx::types::b_date last_time_displayed_info_updated{
            std::chrono::milliseconds{-1L}}; //"aEC"; //mongoDB Date; time the email comes off cool down

    bsoncxx::types::b_date birthday_timestamp{std::chrono::milliseconds{
            -1L}}; //"dTb"; //mongoDB dates; timestamp for last time birthday was updated (in seconds)
    bsoncxx::types::b_date gender_timestamp{std::chrono::milliseconds{
            -1L}}; //"dTg"; //mongoDB dates; timestamp for last time gender was updated (in seconds)
    bsoncxx::types::b_date first_name_timestamp{std::chrono::milliseconds{
            -1L}}; //"dTn"; //mongoDB dates; timestamp for last time first name was updated (in seconds)
    bsoncxx::types::b_date bio_timestamp{std::chrono::milliseconds{
            -1L}}; //"dTi"; //mongoDB dates; timestamp for last time user bio was updated (in seconds)
    bsoncxx::types::b_date city_name_timestamp{std::chrono::milliseconds{
            -1L}}; //"dTm"; //mongoDB dates; timestamp for last time user city name was updated (in seconds)
    bsoncxx::types::b_date post_login_info_timestamp{std::chrono::milliseconds{
            -1L}}; //"dTc"; //mongoDB dates; timestamp for last time any of the 'post login' info was updated (info collected after login including bio, city name, age range, gender range, max distance) (in seconds)
    bsoncxx::types::b_date email_timestamp{std::chrono::milliseconds{
            -1L}}; //"dTe"; //mongoDB dates; timestamp for last time email was updated (this also includes if email verified bool was changed) (in seconds)
    bsoncxx::types::b_date categories_timestamp{
            std::chrono::milliseconds{-1L}}; //"dTs"; //mongoDB Date; timestamp for last time categories was updated

    struct SingleChatRoom {
        std::string chat_room_id; //"sIe"; //string; chat room id
        bsoncxx::types::b_date last_time_viewed; //"dTv"; //mongoDB dates; last time this user viewed the chat room
        std::optional<bsoncxx::oid> event_oid{}; //"eId"; //oid; Only present if this is an event chat room. The event oid.

        SingleChatRoom() = delete;

        SingleChatRoom(
                std::string _chat_room_id,
                bsoncxx::types::b_date _last_time_viewed,
                std::optional<bsoncxx::oid> _event_oid
        ) :
                chat_room_id(std::move(_chat_room_id)),
                last_time_viewed(_last_time_viewed),
                event_oid(_event_oid)
        {}

        SingleChatRoom(
                std::string _chat_room_id,
                bsoncxx::types::b_date _last_time_viewed
        ) :
                chat_room_id(std::move(_chat_room_id)),
                last_time_viewed(_last_time_viewed)
        {}
    };

    std::vector<SingleChatRoom> chat_rooms; //"aUp"; //array of documents; contains info about chat rooms this user is present inside

    struct OtherBlockedUser {
        std::string oid_string; //"sId"; //string; oid of account that is blocked for this user
        bsoncxx::types::b_date timestamp_blocked; //"dTs"; //mongoDB date; time block occurred

        OtherBlockedUser() = delete;

        OtherBlockedUser(
                std::string _oid_string,
                bsoncxx::types::b_date _timestamp_blocked
        ) :
                oid_string(std::move(_oid_string)), timestamp_blocked(_timestamp_blocked) {}
    };

    std::vector<OtherBlockedUser> other_users_blocked; //"aUb"; //array of documents; list of accounts blocked for user, contains VERIFIED_OTHER_USERS_BLOCKED_OID and TIMESTAMP_BLOCKED

    int number_times_swiped_yes = INT_ZERO; //"iNy"; //int32; number of times this user has swiped 'yes' on others
    int number_times_swiped_no = INT_ZERO; //"iNn"; //int32; number of times this user has swiped 'no' on others
    int number_times_swiped_block = INT_ZERO; //"iNb"; //int32; number of times this user has swiped 'block' on others
    int number_times_swiped_report = INT_ZERO; //"iNr"; //int32; number of times this user has swiped 'report' on others

    int number_times_others_swiped_yes = INT_ZERO; //"iOy"; //int32; number of times others have swiped 'yes' on this user
    int number_times_others_swiped_no = INT_ZERO; //"iOn"; //int32; number of times others have swiped 'no' on this user
    int number_times_others_swiped_block = INT_ZERO; //"iOb"; //int32; number of times others have swiped 'block' on this user
    int number_times_others_swiped_report = INT_ZERO; //"iOr"; //int32; number of times others have swiped 'report' on this user

    int number_times_sent_activity_suggestion = INT_ZERO; //"iSu"; //int32; number of times this user has sent an 'activity suggestion'
    int number_times_sent_bug_report = INT_ZERO; //"iBr"; //int32; number of times this user has sent a 'bug report'
    int number_times_sent_other_suggestion = INT_ZERO; //"iOs"; //int32; number of times this user has sent an 'other suggestion'

    int number_times_spam_feedback_sent = INT_ZERO; //"iTs"; //int32; number of times the user feedback has been set as spam by an admin
    int number_times_spam_reports_sent = INT_ZERO; //"iTr"; //int32; number of times the user reports have been set as spam by an admin

    int number_times_reported_by_others_in_chat_room = INT_ZERO; //"iCr"; //int32; number of times others have reported this user from chat room (swipes is NUMBER_TIMES_OTHERS_SWIPED_BLOCK)
    int number_times_blocked_by_others_in_chat_room = INT_ZERO; //iCb"; //int32; number of times others have blocked this user from chat room (swipes is NUMBER_TIMES_OTHERS_SWIPED_BLOCK)
    int number_times_this_user_reported_from_chat_room = INT_ZERO; //"iCe"; //int32; number of times this user has swiped report on other users from chat room (swipes is NUMBER_TIMES_SWIPED_REPORT)
    int number_times_this_user_blocked_from_chat_room = INT_ZERO; //"iCl"; //int32; number of times this user has swiped block on other users from chat room (swipes is NUMBER_TIMES_SWIPED_BLOCK)

    struct DisciplinaryRecord {
        bsoncxx::types::b_date submitted_time; //"dRs"; //mongoDB Date; timestamp this action took place
        bsoncxx::types::b_date end_time; //"dRe"; //mongoDB Date; timestamp this action ends (used for suspensions)
        DisciplinaryActionTypeEnum action_type; //"dRt"; //int32; type of action taken follows DisciplinaryActionTypeEnum
        std::string reason; //"dRr"; //string; the message for why the action was taken
        std::string admin_name; //"dRn"; //string; name of admin that send the discipline

        DisciplinaryRecord() = delete;

        DisciplinaryRecord(
                bsoncxx::types::b_date _submitted_time,
                bsoncxx::types::b_date _end_time,
                DisciplinaryActionTypeEnum _action_type,
                std::string _reason,
                std::string _admin_name
        ) :
                submitted_time(_submitted_time),
                end_time(_end_time),
                action_type(_action_type),
                reason(std::move(_reason)),
                admin_name(std::move(_admin_name)) {}
    };

    std::vector<DisciplinaryRecord> disciplinary_record; //"dRk"; //array of documents; container for records of disciplinary action, stored whenever STATUS is updated

    UserAccountDoc() = default;

    UserAccountDoc(
            std::string _phone_number,
            const std::string& device_id,
            std::chrono::milliseconds start_time
    ) :
            last_verified_time(bsoncxx::types::b_date{start_time}),
            time_created(bsoncxx::types::b_date{start_time}),
            phone_number(std::move(_phone_number)),
            last_time_find_matches_ran(bsoncxx::types::b_date{start_time}) {
        current_object_oid = bsoncxx::oid("000000000000000000000000");
        installation_ids.emplace_back(device_id);
    }

    explicit UserAccountDoc(const bsoncxx::oid& account_oid) {
        getFromCollection(account_oid);
    }

    explicit UserAccountDoc(const std::string& _phone_number) {
        current_object_oid = bsoncxx::oid("000000000000000000000000");
        getFromCollection(_phone_number);
    }

    virtual bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //will build the document from the values stored inside this class and upsert it into
    //the verified collection
    //will print any exceptions and return false if fails
    //returns true if successful
    bool setIntoCollection() override;

    //Converts current UserAccountDoc to a CompleteUserAccountInfo.
    [[nodiscard]] CompleteUserAccountInfo
    convertToCompleteUserAccountInfo(const std::chrono::milliseconds& current_timestamp) const;

    void generateNewUserAccount(
            const bsoncxx::oid& account_oid,
            const std::string& passed_phone_number,
            const std::vector<std::string>& passed_installation_ids,
            const std::vector<std::string>& passed_account_id_list,
            const std::chrono::milliseconds& current_timestamp,
            const std::chrono::milliseconds& passed_time_sms_can_be_sent_again,
            int passed_number_swipes_remaining,
            long passed_swipes_last_updated_time,
            const std::chrono::milliseconds& passed_time_email_can_be_sent_again
    );

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    virtual void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    //converts the passed document to this UserAccountDoc object
    virtual bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    //very similar to getFromEventRequestMessage(). However, will extract the randomly generated info from
    // UserAccountDoc. This means the event must have already been created by this point.
    void getFromEventRequestMessageAfterStored(
            const EventRequestMessage& event_request,
            const bsoncxx::oid& event_oid,
            UserAccountType user_account_type,
            const std::chrono::milliseconds& current_timestamp,
            const std::string& event_created_by,
            const std::string& event_chat_room_id
    );

    //converts this UserAccountDoc to the event created from the below info. See also
    // getFromEventRequestMessageAfterStored().
    void getFromEventRequestMessage(
            const EventRequestMessage& event_request,
            const bsoncxx::oid& event_oid,
            UserAccountType user_account_type,
            const std::chrono::milliseconds& current_timestamp,
            const std::string& _installation_id,
            const std::string& _phone_number,
            const std::vector<bsoncxx::oid>& picture_references,
            const std::string& event_created_by,
            const std::string& event_chat_room_id
    );

    //will extract a document from the verified collection and save it to this class instance
    //returns false if fails and prints the error
    //returns true if successful
    bool getFromCollection(const bsoncxx::oid& findOID) override;

    void setDatabaseAndCollection(
            const std::string& _database_name,
            const std::string& _collection_name
    ) {
        database_name = _database_name;
        collection_name = _collection_name;
    };

    bool getFromCollection(const std::string& _phoneNumber);

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const UserAccountDoc& v);

    bool operator==(const UserAccountDoc& other) const;

    bool operator!=(const UserAccountDoc& other) const {
        return !(*this == other);
    }

private:

    const std::string OBJECT_CLASS_NAME = "User Accounts";

    std::string database_name = database_names::ACCOUNTS_DATABASE_NAME;
    std::string collection_name = collection_names::USER_ACCOUNTS_COLLECTION_NAME;
};

class UserAccountStatisticsDoc : public BaseCollectionObject {
public:
    //NOTE: _id fields matches the _id from USER_ACCOUNTS_COLLECTION_NAME, this is separated into a different document to avoid overflow

    struct ParentTimestamp {
        bsoncxx::types::b_date timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

        ParentTimestamp() = delete;

        explicit ParentTimestamp(const bsoncxx::types::b_date& _timestamp) : timestamp(_timestamp) {}
    };

    struct LoginTime : public ParentTimestamp {
        std::string installation_id; //"i"; //string; the installation ID that logged in here
        std::string device_name; //"d"; //string; the name of the device that logged in here
        int api_number; //"a"; //int32; the api number that logged in here
        int lets_go_version; //"l"; //int32; the lets go version that logged in here
        LoginTime() = delete;

        LoginTime(
                std::string _installation_id,
                std::string _device_name,
                int _api_number,
                int _lets_go_version,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            installation_id(std::move(_installation_id)),
            device_name(std::move(_device_name)),
            api_number(_api_number),
            lets_go_version(_lets_go_version) {}
    };

    std::vector<LoginTime> login_times;

    struct TimesMatchOccurred : public ParentTimestamp {
        bsoncxx::oid matched_oid; //"o"; //oid; account oid this user matched with
        std::string chat_room_id; //"c"; //string; chat room users were put into
        std::optional<MatchType> match_type{}; //"y"; //int32 or does not exist; follows MatchType enum from MatchTypeEnum.proto (this was added later, it may not exist on older statistics docs)
        TimesMatchOccurred() = delete;

        TimesMatchOccurred(
                bsoncxx::oid _matched_oid,
                std::string _chat_room_id,
                std::optional<MatchType> _match_type,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            matched_oid(_matched_oid),
            chat_room_id(std::move(_chat_room_id)),
            match_type(_match_type) {}
    };

    std::vector<TimesMatchOccurred> times_match_occurred;

    struct Location : public ParentTimestamp {
        std::array<double, 2> location; //"n"; //array; array has 2 elements of type double, 1st is longitude, 2nd is latitude (to follow mongoDB type Point convention)
        Location() = delete;

        Location(
                double longitude,
                double latitude,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            location{longitude, latitude} {}
    };

    std::vector<Location> locations;

    struct PhoneNumber : public ParentTimestamp {
        std::string phone_number;

        PhoneNumber() = delete;

        PhoneNumber(
                std::string _phone_number,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            phone_number(std::move(_phone_number)) {}
    };

    std::vector<PhoneNumber> phone_numbers;

    struct Name : public ParentTimestamp {
        std::string name;

        Name() = delete;

        Name(
                std::string _name,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            name(std::move(_name)) {}
    };

    std::vector<Name> names;

    struct Bio : public ParentTimestamp {
        std::string bio;

        Bio() = delete;

        Bio(
                std::string _bio,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            bio(std::move(_bio)) {}
    };

    std::vector<Bio> bios;

    struct City : public ParentTimestamp {
        std::string city; //"n"; //string; phone number
        City() = delete;

        City(
                std::string _city,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            city(std::move(_city)) {}
    };

    std::vector<City> cities;

    struct Gender : public ParentTimestamp {
        std::string gender; //"n"; //string; phone number
        Gender() = delete;

        Gender(
                std::string _gender,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            gender(std::move(_gender)) {}
    };

    std::vector<Gender> genders;

    struct BirthInfo : public ParentTimestamp {
        int year; //"y"; //int32; birth year
        int month; //"m"; //int32; birth month
        int day_of_month; //"d"; //int32; birth day of month
        BirthInfo() = delete;

        BirthInfo(
                int _year,
                int _month,
                int _day_of_month,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            year(_year),
            month(_month),
            day_of_month(_day_of_month) {}
    };

    std::vector<BirthInfo> birth_info;

    struct EmailAddress : public ParentTimestamp {
        std::string email;

        EmailAddress() = delete;

        EmailAddress(
                std::string _email,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            email(std::move(_email)) {}
    };

    std::vector<EmailAddress> email_addresses;

    struct StoredCategories : public ParentTimestamp {
        std::vector<TestCategory> categories;

        StoredCategories() = delete;

        StoredCategories(
                const std::vector<TestCategory>& _categories,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp) {
            std::copy(_categories.begin(), _categories.end(), std::back_inserter(categories));
        }
    };

    std::vector<StoredCategories> categories;

    struct AgeRange : public ParentTimestamp {
        std::array<int, 2> age_range; //array of 2 int32 values; min and max age range
        AgeRange() = delete;

        AgeRange(
                int min_age,
                int max_age,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            age_range{min_age, max_age} {}
    };

    std::vector<AgeRange> age_ranges;

    struct GenderRange : public ParentTimestamp {
        std::vector<std::string> gender_range; //array of strings; list of genders this user matches with follows GENDERS_RANGE
        GenderRange() = delete;

        GenderRange(
                const std::vector<std::string>& _gender_range,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp) {
            std::copy(_gender_range.begin(), _gender_range.end(), std::back_inserter(gender_range));
        }
    };

    std::vector<GenderRange> gender_ranges;

    struct MaxDistance : public ParentTimestamp {
        int max_distance;

        MaxDistance() = delete;

        MaxDistance(
                int _max_distance,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            max_distance(_max_distance) {}
    };

    std::vector<MaxDistance> max_distances;

    std::vector<bsoncxx::types::b_date> sms_sent_times;

    struct EmailSentTime : public ParentTimestamp {
        std::string email_address; //"e"; //string; email address email will be sent TO
        std::string email_prefix; //"p"; //string; email prefix email will be sent FROM
        std::string email_subject; //"s"; //string; email message subject
        std::string email_content; //"c"; //string; email message content

        EmailSentTime() = delete;

        EmailSentTime(
                std::string _email_address,
                std::string _email_prefix,
                std::string _email_subject,
                std::string _email_content,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            email_address(std::move(_email_address)),
            email_prefix(std::move(_email_prefix)),
            email_subject(std::move(_email_subject)),
            email_content(std::move(_email_content)) {}
    };

    std::vector<EmailSentTime> email_sent_times;

    struct EmailVerified : public ParentTimestamp {
        std::string emails_verified_email; //"e"; //string; email address

        EmailVerified() = delete;

        EmailVerified(
                std::string _emails_verified_email,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            emails_verified_email(std::move(_emails_verified_email)) {}
    };

    std::vector<EmailVerified> emails_verified;

    struct AccountRecoveryTime : public ParentTimestamp {
        std::string previous_phone_number; //"p"; //string; previously used phone number
        std::string new_phone_number; //"n"; //string; updated phone number

        AccountRecoveryTime() = delete;

        AccountRecoveryTime(
                std::string _previous_phone_number,
                std::string _new_phone_number,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            previous_phone_number(std::move(_previous_phone_number)),
            new_phone_number(std::move(_new_phone_number)) {}
    };

    std::vector<AccountRecoveryTime> account_recovery_times;

    std::vector<bsoncxx::types::b_date> account_logged_out_times;

    struct SearchByOption : public ParentTimestamp {
        int new_search_by_options;

        SearchByOption() = delete;

        SearchByOption(
                int _new_search_by_options,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            new_search_by_options(_new_search_by_options) {}
    };

    std::vector<SearchByOption> account_search_by_options;

    struct OptedInToPromotionalEmail : public ParentTimestamp {
        bool new_opted_in_to_promotional_email;

        OptedInToPromotionalEmail() = delete;

        OptedInToPromotionalEmail(
                bool _new_opted_in_to_promotional_email,
                bsoncxx::types::b_date _timestamp
        ) : ParentTimestamp(_timestamp),
            new_opted_in_to_promotional_email(_new_opted_in_to_promotional_email) {}
    };

    std::vector<OptedInToPromotionalEmail> opted_in_to_promotional_email;

    virtual bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    virtual //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    virtual bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    UserAccountStatisticsDoc() = default;

    explicit UserAccountStatisticsDoc(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const UserAccountStatisticsDoc& v);

    bool operator==(const UserAccountStatisticsDoc& other) const;

    bool operator!=(const UserAccountStatisticsDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "User Account Statistics";

};

class UserPictureDoc : public BaseCollectionObject {
public:

    bsoncxx::oid user_account_reference{}; //"aR"; //ObjectId (type oid in C++)
    std::vector<std::string> thumbnail_references; //"tR"; //array of strings; list of chat room ids that hold a reference to this pictures thumbnail
    bsoncxx::types::b_date timestamp_stored = DEFAULT_DATE; //"pT"; //mongoDB Date; timestamp this picture was saved (in seconds) as shown in user account
    int picture_index{}; //"pN"; //int32; index this picture holds in the user account
    std::string thumbnail_in_bytes; //"tN"; //utf8; picture thumbnail in bytes
    int thumbnail_size_in_bytes{}; //"tS"; //int32; total size in bytes of this thumbnail
    std::string picture_in_bytes; //"pI"; //utf8; picture itself in bytes
    int picture_size_in_bytes{}; //"pS"; //int32; total size in bytes of this picture

    virtual bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    virtual void convertToDocument(
            bsoncxx::builder::stream::document& document_result,
            bool skip_long_strings
    ) const;

    virtual bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    void getFromEventsPictureMessage(
            const EventsPictureMessage& events_picture_message,
            bsoncxx::oid _picture_reference,
            bsoncxx::types::b_date _timestamp_stored,
            bsoncxx::oid _event_oid,
            int _index
    );

    virtual bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    virtual bool getFromCollection(const bsoncxx::document::view& find_doc);

    UserPictureDoc() = default;

    explicit UserPictureDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    virtual ~UserPictureDoc() = default;

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const UserPictureDoc& v);

    bool operator==(const UserPictureDoc& other) const;

    bool operator!=(const UserPictureDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "User Pictures";
};

class PreLoginCheckersDoc : public BaseCollectionObject {
public:

    std::string installation_id; //"_id" //string; The installation id, a UUID given to each installation of the app.

    int number_sms_verification_messages_sent = 0; //"nVs"; //int32; the total number of times sms verification messages have been sent from this documents' device ID NOTE: works with SMS_VERIFICATION_MESSAGES_LAST_UPDATE_TIME
    long sms_verification_messages_last_update_time = 0; //"vLu"; //int64; (NOT MONGODB DATE TYPE); this is unixTimestamp/TIME_BETWEEN_SMS_VERIFICATION_MESSAGES_BY_INSTALLATION_ID, so it is not the full timestamp NOTE: works with NUMBER_SMS_VERIFICATION_MESSAGES_SENT

    virtual bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this PreLoginCheckersDoc object to a document and saves it to the passed builder
    virtual void convertToDocument(
            bsoncxx::builder::stream::document& document_result
    ) const;

    virtual bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    virtual bool getFromCollection();

    bool getFromCollection(const std::string& _installation_id);

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    virtual bool getFromCollection(const bsoncxx::document::view& find_doc);

    PreLoginCheckersDoc() = default;

    explicit PreLoginCheckersDoc(const std::string& _installation_id) {
        getFromCollection(_installation_id);
    }

    virtual ~PreLoginCheckersDoc() = default;

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const PreLoginCheckersDoc& v);

    bool operator==(const PreLoginCheckersDoc& other) const;

    bool operator!=(const PreLoginCheckersDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "User Pictures";
};
