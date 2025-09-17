//
// Created by jeremiah on 6/3/22.
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

class FreshErrorsDoc : public BaseCollectionObject {
public:

    ErrorOriginType error_origin = ErrorOriginType::ERROR_ORIGIN_UNKNOWN; //"eO"; //int32; follows ErrorOriginType enum inside ErrorOriginEnum.proto
    ErrorUrgencyLevel error_urgency = ErrorUrgencyLevel::ERROR_URGENCY_LEVEL_UNKNOWN; //"eU"; //int32; follows ErrorUrgencyLevel enum inside ErrorOriginEnum.proto
    int version_number = 0; //"vN"; //int32; version number (should be greater than 0)
    std::string file_name; //"fN"; //string; file name where error occurred
    int line_number = 0; //"lN"; //int32; line number where error occurred (should be greater than or equal to 0)
    std::string stack_trace; //"sT"; //string; stack trace (if available) of error ALWAYS EXISTS
    bsoncxx::types::b_date timestamp_stored = DEFAULT_DATE; //"tS"; //mongoDB Date; timestamp error was stored
    std::unique_ptr<int> api_number = nullptr; //"aP"; //int32 or does not exist; Android API number (only used when ERROR_ORIGIN == ErrorOriginType.ERROR_ORIGIN_ANDROID)
    std::unique_ptr<std::string> device_name = nullptr; //"dN"; //string or does not exist; Device name (only used when ERROR_ORIGIN == ErrorOriginType.ERROR_ORIGIN_ANDROID)
    std::string error_message; //"eM"; //string; error message

    void generateRandomValues();

    virtual bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    virtual void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    virtual bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    bool getFromCollection(const bsoncxx::document::view& find_doc);

    FreshErrorsDoc() = default;

    explicit FreshErrorsDoc(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    virtual ~FreshErrorsDoc() = default;

    FreshErrorsDoc(const FreshErrorsDoc& other) : BaseCollectionObject(other) {
        *this = other;
    }

    FreshErrorsDoc& operator=(const FreshErrorsDoc& other) {
        current_object_oid = other.current_object_oid;
        error_origin = other.error_origin;
        error_urgency = other.error_urgency;
        version_number = other.version_number;
        file_name = other.file_name;
        line_number = other.line_number;
        stack_trace = other.stack_trace;
        timestamp_stored = other.timestamp_stored;

        if(other.api_number == nullptr) {
            api_number = nullptr;
        } else {
            api_number = std::make_unique<int>(*other.api_number);
        }

        if(other.device_name == nullptr) {
            device_name = nullptr;
        } else {
            device_name = std::make_unique<std::string>(*other.device_name);
        }

        error_message = other.error_message;

        return *this;
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const FreshErrorsDoc& v);

    bool operator==(const FreshErrorsDoc& other) const;

    bool operator!=(const FreshErrorsDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Fresh Errors Doc";

    bool store_id_during_convert = true;
};

class HandledErrorsDoc : public FreshErrorsDoc {
public:

    std::string admin_name; //"aN"; //string; admin name that set this 'chunk' of reports to handled
    bsoncxx::types::b_date timestamp_handled = DEFAULT_DATE; //"tH"; //mongoDB Date; timestamp admin handled this 'chunk' of reports
    ErrorHandledMoveReason error_handled_move_reason = ErrorHandledMoveReason(-1); // = "mR"; //int32; reason this message was moved from FRESH_ERRORS_COLLECTION_NAME to HANDLED_ERRORS_COLLECTION_NAME; follows ErrorHandledMoveReason inside ErrorHandledMoveReasonEnum.proto
    std::string errors_description; //"dE"; //string or does not exist; short description of bug; currently used in all cases, could NOT be used in a case in the future

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) override;

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const override;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document) override;

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    HandledErrorsDoc() = default;

    explicit HandledErrorsDoc(const FreshErrorsDoc& other) : FreshErrorsDoc(other) {}

    explicit HandledErrorsDoc(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const HandledErrorsDoc& v);

    bool operator==(const HandledErrorsDoc& other) const;

    bool operator!=(const HandledErrorsDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Handled Errors Doc";
};

class HandledErrorsListDoc : public BaseCollectionObject {
public:

    ErrorOriginType error_origin = ErrorOriginType::ERROR_ORIGIN_UNKNOWN; //"eO"; //int32; follows ErrorOriginType enum inside ErrorOriginEnum.proto
    int version_number = 0; //"vN"; //int32; version number (should be greater than 0)
    std::string file_name; //"fN"; //string; file name where error occurred
    int line_number = 0; //"lN"; //int32; line number where error occurred (should be greater than 0)
    ErrorHandledMoveReason error_handled_move_reason = ErrorHandledMoveReason(-1); //"mR"; //int32; reason this message was moved from FRESH_ERRORS_COLLECTION_NAME to HANDLED_ERRORS_COLLECTION_NAME; follows ErrorHandledMoveReason inside ErrorHandledMoveReasonEnum.proto
    std::unique_ptr<std::string> description = nullptr; //"dE"; //string or does not exist; short description of bug; currently used in all cases, could NOT be used in a case in the future

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& find_oid) override;

    bool getFromCollection(const bsoncxx::document::view& doc);

    HandledErrorsListDoc() = default;

    explicit HandledErrorsListDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const HandledErrorsListDoc& v);

    bool operator==(const HandledErrorsListDoc& other) const;

    bool operator!=(const HandledErrorsListDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Handled Errors List Doc";
};

