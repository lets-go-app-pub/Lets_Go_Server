//
// Created by jeremiah on 6/1/22.
//

#pragma once

#include <chat_rooms_objects.h>
#include "account_objects.h"
#include "reason_picture_deleted.h"

class DeletedAccountsDoc : public UserAccountDoc {
public:
    bsoncxx::types::b_date timestamp_removed = DEFAULT_DATE; //"tst_rem"; //mongoDB Date; time the picture was moved to the deleted collection

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) override;

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const override;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document) override;

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    DeletedAccountsDoc() = default;

    explicit DeletedAccountsDoc(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    DeletedAccountsDoc(
            const UserAccountDoc& other,
            bsoncxx::types::b_date _timestamp_removed
    ) : UserAccountDoc(other),
        timestamp_removed(_timestamp_removed) {}

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const DeletedAccountsDoc& v);

    bool operator==(const DeletedAccountsDoc& other) const;

    bool operator!=(const DeletedAccountsDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Deleted Account Doc";
};

class DeletedChatMessagePicturesDoc : public ChatMessagePictureDoc {
public:
    bsoncxx::types::b_date timestamp_removed = DEFAULT_DATE; //"tst_rem"; //mongoDB Date; time the picture was moved to the deleted collection

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) override;

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(
            bsoncxx::builder::stream::document& document_result,
            bool skip_long_strings
    ) const override;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document) override;

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    DeletedChatMessagePicturesDoc() = delete;

    explicit DeletedChatMessagePicturesDoc(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    DeletedChatMessagePicturesDoc(
            const ChatMessagePictureDoc& other,
            bsoncxx::types::b_date _timestamp_removed
    ) : ChatMessagePictureDoc(other),
        timestamp_removed(_timestamp_removed) {}

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const DeletedChatMessagePicturesDoc& v);

    bool operator==(const DeletedChatMessagePicturesDoc& other) const;

    bool operator!=(const DeletedChatMessagePicturesDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Deleted Chat Message Pictures Doc";
};

class DeletedUserPictureDoc : public UserPictureDoc {
public:
    std::unique_ptr<bool> references_removed_after_delete = nullptr; //"rem_aft"; //bool or missing; MAY exist inside a deleted picture (field may not exist as well), will be set to true if the references were deleted AFTER the picture was set to deleted.
    bsoncxx::types::b_date timestamp_removed = DEFAULT_DATE; //"tst_rem"; //mongoDB Date; time the picture was moved to the deleted collection
    ReasonPictureDeleted reason_removed = ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE; //"rsn_rem"; //int32; reason picture deleted; follows ReasonPictureDeleted
    std::unique_ptr<std::string> admin_name = nullptr; //"adm_nam"; //utf8 or missing; admin name when ReasonPictureDeleted == REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE; will not exist otherwise

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) override;

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(
            bsoncxx::builder::stream::document& document_result,
            bool skip_long_strings
    ) const override;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document) override;

    bool setIntoCollection() override;

    bool getFromCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    bool getFromCollection(const bsoncxx::document::view& find_doc) override;

    DeletedUserPictureDoc() = default;

    explicit DeletedUserPictureDoc(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    DeletedUserPictureDoc(
            const UserPictureDoc& other,
            const std::unique_ptr<bool> _references_removed_after_delete, //set to nullptr if field is missing
            bsoncxx::types::b_date _timestamp_removed,
            ReasonPictureDeleted _reason_removed,
            const std::unique_ptr<std::string>& _admin_name //set to nullptr if field is missing
    ) : UserPictureDoc(other),
        timestamp_removed(_timestamp_removed),
        reason_removed(_reason_removed) {
        if (_references_removed_after_delete == nullptr) {
            references_removed_after_delete = nullptr;
        } else {
            references_removed_after_delete = std::make_unique<bool>(*_references_removed_after_delete);
        }

        if (_admin_name == nullptr) {
            admin_name = nullptr;
        } else {
            admin_name = std::make_unique<std::string>(*_admin_name);
        }
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const DeletedUserPictureDoc& v);

    bool operator==(const DeletedUserPictureDoc& other) const;

    bool operator!=(const DeletedUserPictureDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Deleted User Pictures Doc";
};