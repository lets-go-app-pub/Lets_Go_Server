//
// Created by jeremiah on 6/4/22.
//

#pragma once

#include <boost/optional.hpp>
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
#include "ErrorHandledMoveReasonEnum.grpc.pb.h"
#include "ErrorOriginEnum.grpc.pb.h"

class ActivitySuggestionFeedbackDoc : public BaseCollectionObject {
public:

    bsoncxx::oid account_oid; //"iD"; //OID; the account OID of the person that sent the feedback
    std::string activity_name; //"aN"; //utf8; the name of the activity that was suggested (optional could be empty)
    std::string message; //"mE"; //utf8; the message body from the suggestion
    bsoncxx::types::b_date timestamp_stored = DEFAULT_DATE; //"dA"; //mongodb Date type; time this feedback was inserted
    std::unique_ptr<std::string> marked_as_spam = nullptr; //"sP"; //utf8; field is not set by default, set to SHARED_NAME_KEY of admin that reported it as spam if reported as spam

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    ActivitySuggestionFeedbackDoc() = default;

    ActivitySuggestionFeedbackDoc(const ActivitySuggestionFeedbackDoc& other)  : BaseCollectionObject(other) {
        *this = other;
    }

    ActivitySuggestionFeedbackDoc(ActivitySuggestionFeedbackDoc&& other) = default;

    ActivitySuggestionFeedbackDoc& operator=(const ActivitySuggestionFeedbackDoc& other) {
        account_oid = other.account_oid;
        activity_name = other.activity_name;
        message = other.message;
        timestamp_stored = other.timestamp_stored;

        if(!other.marked_as_spam) {
            marked_as_spam = nullptr;
        } else {
            marked_as_spam = std::make_unique<std::string>(*other.marked_as_spam);
        }

        return *this;
    }

    virtual ~ActivitySuggestionFeedbackDoc() = default;

    explicit ActivitySuggestionFeedbackDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    void generateRandomValues(bool random_marked_by_spam = false);

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const ActivitySuggestionFeedbackDoc& v);

    bool operator==(const ActivitySuggestionFeedbackDoc& other) const;

    bool operator!=(const ActivitySuggestionFeedbackDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Activity Suggestion Feedback Doc";
};

class BugReportFeedbackDoc : public BaseCollectionObject {
public:

    bsoncxx::oid account_oid; //"iD"; //OID; the account OID of the person that sent the feedback
    std::string message; //"mE"; //utf8; the message body from the suggestion
    bsoncxx::types::b_date timestamp_stored = DEFAULT_DATE; //"dA"; //mongodb Date type; time this feedback was inserted
    std::unique_ptr<std::string> marked_as_spam; //"sP"; //utf8; field is not set by default, set to SHARED_NAME_KEY of admin that reported it as spam if reported as spam

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    BugReportFeedbackDoc() = default;

    BugReportFeedbackDoc(const BugReportFeedbackDoc& other)  : BaseCollectionObject(other) {
        *this = other;
    }

    BugReportFeedbackDoc(BugReportFeedbackDoc&& other) = default;

    BugReportFeedbackDoc& operator=(const BugReportFeedbackDoc& other) {
        account_oid = other.account_oid;
        message = other.message;
        timestamp_stored = other.timestamp_stored;

        if(!other.marked_as_spam) {
            marked_as_spam = nullptr;
        } else {
            marked_as_spam = std::make_unique<std::string>(*other.marked_as_spam);
        }

        return *this;
    }

    virtual ~BugReportFeedbackDoc() = default;

    explicit BugReportFeedbackDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    void generateRandomValues(bool random_marked_by_spam = false);

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const BugReportFeedbackDoc& v);

    bool operator==(const BugReportFeedbackDoc& other) const;

    bool operator!=(const BugReportFeedbackDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Bug Report Feedback Doc";
};

class OtherSuggestionFeedbackDoc : public BaseCollectionObject {
public:

    bsoncxx::oid account_oid; //"iD"; //OID; the account OID of the person that sent the feedback
    std::string message; //"mE"; //utf8; the message body from the suggestion
    bsoncxx::types::b_date timestamp_stored = DEFAULT_DATE; //"dA"; //mongodb Date type; time this feedback was inserted
    std::unique_ptr<std::string> marked_as_spam; //"sP"; //utf8; field is not set by default, set to SHARED_NAME_KEY of admin that reported it as spam if reported as spam

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    OtherSuggestionFeedbackDoc() = default;

    OtherSuggestionFeedbackDoc(const OtherSuggestionFeedbackDoc& other)  : BaseCollectionObject(other) {
        *this = other;
    }

    OtherSuggestionFeedbackDoc(OtherSuggestionFeedbackDoc&& other) = default;

    OtherSuggestionFeedbackDoc& operator=(const OtherSuggestionFeedbackDoc& other) {
        account_oid = other.account_oid;
        message = other.message;
        timestamp_stored = other.timestamp_stored;

        if(!other.marked_as_spam) {
            marked_as_spam = nullptr;
        } else {
            marked_as_spam = std::make_unique<std::string>(*other.marked_as_spam);
        }

        return *this;
    }

    virtual ~OtherSuggestionFeedbackDoc() = default;

    explicit OtherSuggestionFeedbackDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    void generateRandomValues(bool random_marked_by_spam = false);

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const OtherSuggestionFeedbackDoc& v);

    bool operator==(const OtherSuggestionFeedbackDoc& other) const;

    bool operator!=(const OtherSuggestionFeedbackDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Other Suggestion Feedback Doc";
};