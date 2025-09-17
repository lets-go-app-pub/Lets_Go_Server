//
// Created by jeremiah on 5/31/22.
//

#pragma once

#include <utility>

#include "account_objects.h"
#include "individual_match_statistics_keys.h"
#include "utility_general_functions.h"

class IndividualMatchStatisticsDoc : public BaseCollectionObject {
public:

    struct StatusArray {
        std::string status; //"sT"; //utf8; essentially works like an enum for different cases; follows individual_match_statistics_keys::status_array::status
        bsoncxx::types::b_date response_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{
                -1L}}; //"rT"; //mongoDB Date; timestamp the match was returned from the device;

        StatusArray() = delete;

        explicit StatusArray(bool) {
            generateRandomValues();
        }

        StatusArray(
                std::string _status,
                bsoncxx::types::b_date _response_timestamp
        ) :
            status(std::move(_status)),
            response_timestamp(_response_timestamp) {}

        void generateRandomValues() {
            switch(rand() % 4) {
                case 0:
                    status = individual_match_statistics_keys::status_array::status_enum::YES;
                    break;
                case 1:
                    status = individual_match_statistics_keys::status_array::status_enum::NO;
                    break;
                case 2:
                    status = individual_match_statistics_keys::status_array::status_enum::BLOCK;
                    break;
                case 3:
                    status = individual_match_statistics_keys::status_array::status_enum::REPORT;
                    break;
                default:
                    status = individual_match_statistics_keys::status_array::status_enum::UN_KNOWN;
                    break;
            }
            response_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{rand() % getCurrentTimestamp().count()}};
        }
    };

    std::vector<StatusArray> status_array; //"aS"; //array of documents containing, STATUS and RESPONSE_TIMESTAMP, this could be empty or have up to 2 elements if 1 user swipes yes then another user swipes on that (the 2nd element will be the 2nd to swipe)

    std::vector<bsoncxx::types::b_date> sent_timestamp; //"tS"; //array of mongoDB Date; timestamp the match was sent to the first device (array index will align with STATUS_ARRAY)

    long day_timestamp = 0; //"tD"; //int64; the day number (from unix timestamp point of January 1970...) used for indexing and works with field GENERATED_FOR_DAY

    MatchingElement user_extracted_list_element_document; //"eD"; //document; document storing the 'extracted' list element from the match; minus the 2 OID fields because they are already extracted

    UserAccountDoc matched_user_account_document; //"uV"; //document; document of matched used account at the of algorithm running (the calling user account is inside of MATCH_ALGORITHM_STATISTICS_COLLECTION_NAME for when the algorithm was run)

    void generateRandomValues(const bsoncxx::oid& matched_user_oid);

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(
            bsoncxx::builder::stream::document& document_result
    ) const;

    bool convertDocumentToClass(const bsoncxx::document::view& document_parameter);

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    IndividualMatchStatisticsDoc() = default;

    explicit IndividualMatchStatisticsDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const IndividualMatchStatisticsDoc& v);

    bool operator==(const IndividualMatchStatisticsDoc& other) const;

    bool operator!=(const IndividualMatchStatisticsDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Individual Match Statistics Doc";
};

class MatchAlgorithmResultsDoc : public BaseCollectionObject {
public:

    long generated_for_day = 0; //"gD"; //int64; the day number (from unix timestamp point of January 1970...) this result was generated for
    UserAccountType account_type = UserAccountType(-1); //"aC"; //int32; An enum representing the type of account was matched (user or event types). Follows UserAccountType enum from UserAccountType.proto. Can also be set to ACCOUNT_TYPE_KEY_HEADER_VALUE.
    AccountCategoryType categories_type = AccountCategoryType(
            -1); //"tY"; //int32; enum representing what type of document this is, follows AccountCategoryType
    int categories_value = 0; //"vA"; //int32; integer value of activity or category
    long num_yes_yes = 0; //"yY"; //int64; the number of matches where the first user swiped yes and the response to that was also swiped yes on
    long num_yes_no = 0; //"yN"; //int64; the number of matches where the first user swiped yes and the response to that was swiped no on
    long num_yes_block_and_report = 0; //"yB"; //int64; the number of matches where the first user swiped yes and the response to that swiped block & report on
    long num_yes_incomplete = 0; //"yI"; //int64; the number of matches where the first user swiped yes and the response to that was not yet swiped on
    long num_no = 0; //"nO"; //int64; the number of matches where the user swiped no
    long num_block_and_report = 0; //"bR"; //int64; the number of matches where the user swiped block and report
    long num_incomplete = 0; //"iC"; //int64; the number of match was incomplete

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& document_parameter);

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    MatchAlgorithmResultsDoc() = default;

    explicit MatchAlgorithmResultsDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const MatchAlgorithmResultsDoc& v);

    bool operator==(const MatchAlgorithmResultsDoc& other) const;

    bool operator!=(const MatchAlgorithmResultsDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Match Algorithm Results Doc";
};

class MatchAlgorithmStatisticsDoc : public BaseCollectionObject {
public:

    bsoncxx::types::b_date timestamp_run = DEFAULT_DATE; //"Ts"; //mongoDB Date; timestamp the match was run and used as start time in algorithm
    bsoncxx::types::b_date end_time = DEFAULT_DATE; //"eT"; //mongoDB Date; end time in algorithm (not the time the algorithm took to run but the end point the algorithm uses to calculate matches)

    long algorithm_run_time = 0; //"rT"; //int64; Total time IN NANO SECONDS that the algorithm took to run.
    long query_activities_run_time = 0; // "qA"; //int64; Total time IN NANO SECONDS that the query during the algorithm took to run.

    UserAccountDoc user_account_document; //"vA"; //document; the user verified account doc

    struct RawAlgorithmResultsDoc {

        double mongo_pipeline_distance = 0;
        long mongo_pipeline_match_expiration_time = 0;
        double mongo_pipeline_final_points = 0;

        struct MatchStatisticsDoc {
            bsoncxx::types::b_date last_time_find_matches_ran = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
            long mongo_pipeline_time_match_ran = 0;
            long mongo_pipeline_earliest_start_time = 0;
            long mongo_pipeline_max_possible_time = 0;
            double mongo_pipeline_time_fall_off = 0;
            double mongo_pipeline_category_or_activity_points = 0;
            double mongo_pipeline_inactivity_points_to_subtract = 0;

            struct ActivityStatisticsDoc {
                int index_value = 0; //int32; integer value of activity or category
                AccountCategoryType type = AccountCategoryType(
                        -1); //int32; enum representing what type of document this is, follows AccountCategoryType
                long mongo_pipeline_total_user_time = 0;
                long mongo_pipeline_total_time_for_match = 0;
                long mongo_pipeline_total_overlap_time = 0;
                std::vector<long> mongo_pipeline_between_times_array;

                ActivityStatisticsDoc() = default;

                ActivityStatisticsDoc(
                        int _index_value,
                        AccountCategoryType _type,
                        long _mongo_pipeline_total_user_time,
                        long _mongo_pipeline_total_time_for_match,
                        long _mongo_pipeline_total_overlap_time,
                        std::vector<long> _mongo_pipeline_between_times_array
                ) :
                        index_value(_index_value),
                        type(_type),
                        mongo_pipeline_total_user_time(_mongo_pipeline_total_user_time),
                        mongo_pipeline_total_time_for_match(_mongo_pipeline_total_time_for_match),
                        mongo_pipeline_total_overlap_time(_mongo_pipeline_total_overlap_time),
                        mongo_pipeline_between_times_array(std::move(_mongo_pipeline_between_times_array)) {}

                void convertToDocument(bsoncxx::builder::stream::document& document_result) const;
            };

            std::vector<ActivityStatisticsDoc> activity_statistics;

            MatchStatisticsDoc() = default;

            MatchStatisticsDoc(
                    bsoncxx::types::b_date _last_time_find_matches_ran,
                    long _mongo_pipeline_time_match_ran,
                    long _mongo_pipeline_earliest_start_time,
                    long _mongo_pipeline_max_possible_time,
                    double _mongo_pipeline_time_fall_off,
                    double _mongo_pipeline_category_or_activity_points,
                    double _mongo_pipeline_inactivity_points_to_subtract,
                    std::vector<ActivityStatisticsDoc> _activity_statistics
            ) :
                    last_time_find_matches_ran(_last_time_find_matches_ran),
                    mongo_pipeline_time_match_ran(_mongo_pipeline_time_match_ran),
                    mongo_pipeline_earliest_start_time(_mongo_pipeline_earliest_start_time),
                    mongo_pipeline_max_possible_time(_mongo_pipeline_max_possible_time),
                    mongo_pipeline_time_fall_off(_mongo_pipeline_time_fall_off),
                    mongo_pipeline_category_or_activity_points(_mongo_pipeline_category_or_activity_points),
                    mongo_pipeline_inactivity_points_to_subtract(_mongo_pipeline_inactivity_points_to_subtract),
                    activity_statistics(std::move(_activity_statistics)) {}

            void convertToDocument(bsoncxx::builder::stream::document& document_result) const;
        };

        MatchStatisticsDoc mongo_pipeline_match_statistics;

        RawAlgorithmResultsDoc() = default;

        RawAlgorithmResultsDoc(
                double _mongo_pipeline_distance,
                long _mongo_pipeline_match_expiration_time,
                double _mongo_pipeline_final_points,
                MatchStatisticsDoc _mongo_pipeline_match_statistics
        ) :
                mongo_pipeline_distance(_mongo_pipeline_distance),
                mongo_pipeline_match_expiration_time(_mongo_pipeline_match_expiration_time),
                mongo_pipeline_final_points(_mongo_pipeline_final_points),
                mongo_pipeline_match_statistics(std::move(_mongo_pipeline_match_statistics)) {}

        void convertToDocument(bsoncxx::builder::stream::document& document_result) const;
    };

    std::vector<RawAlgorithmResultsDoc> results_docs; //"rS"; //array of documents; the returned document values, these are directly inserted from the algorithm results

    //requires a user account oid (with an existing account inside the database) in order to work
    void generateRandomValues(
            const bsoncxx::oid& user_account_oid,
            bool always_generate_results_doc = true
            );

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& document_parameter);

    bool setIntoCollection() override;

    //get first document in collection
    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    MatchAlgorithmStatisticsDoc() = default;

    explicit MatchAlgorithmStatisticsDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const MatchAlgorithmStatisticsDoc& v);

    bool operator==(const MatchAlgorithmStatisticsDoc& other) const;

    bool operator!=(const MatchAlgorithmStatisticsDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Match Algorithm Statics Doc";
};

class UserAccountStatisticsDocumentsCompletedDoc : public UserAccountStatisticsDoc {
public:
    bsoncxx::oid previous_id; //"p_id"; //oid; the oid this document was moved from (the user it belongs to)
    bsoncxx::types::b_date timestamp_moved = bsoncxx::types::b_date{
            std::chrono::milliseconds{-1L}}; //"tSm"; //mongodb Date; the time the document was moved to this collection

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) override;

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const override;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document) override;

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    UserAccountStatisticsDocumentsCompletedDoc() = default;

    [[maybe_unused]] explicit UserAccountStatisticsDocumentsCompletedDoc(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    explicit UserAccountStatisticsDocumentsCompletedDoc(bool) {
        getFromCollection();
    };

    UserAccountStatisticsDocumentsCompletedDoc(
            const UserAccountStatisticsDoc& other,
            const bsoncxx::oid& _previous_id,
            const std::chrono::milliseconds& _timestamp_moved
    ) : UserAccountStatisticsDoc(other),
        previous_id(_previous_id),
        timestamp_moved(_timestamp_moved) {}

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const UserAccountStatisticsDocumentsCompletedDoc& v);

    bool operator==(const UserAccountStatisticsDocumentsCompletedDoc& other) const;

    bool operator!=(const UserAccountStatisticsDocumentsCompletedDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "User Account Statistics Documents Completed";
};